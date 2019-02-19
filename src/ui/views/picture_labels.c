/* ***************************************************************************
 * picture_label.c -- picture label labels_panel
 *
 * Copyright (C) 2018 by Liu Chao <lc-soft@live.cn>
 *
 * This file is part of the LC-Finder project, and may only be used, modified,
 * and distributed under the terms of the GPLv2.
 *
 * By continuing to use, modify, or distribute this file you indicate that you
 * have read the license and understand and accept it fully.
 *
 * The LC-Finder project is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GPL v2 for more details.
 *
 * You should have received a copy of the GPLv2 along with this file. It is
 * usually in the LICENSE.TXT file, If not, see <http://www.gnu.org/licenses/>.
 * ****************************************************************************/

/* ****************************************************************************
 * picture_label.c -- "图片标记" 视图
 *
 * 版权所有 (C) 2018 归属于 刘超 <lc-soft@live.cn>
 *
 * 这个文件是 LC-Finder 项目的一部分，并且只可以根据GPLv2许可协议来使用、更改和
 * 发布。
 *
 * 继续使用、修改或发布本文件，表明您已经阅读并完全理解和接受这个许可协议。
 *
 * LC-Finder 项目是基于使用目的而加以散布的，但不负任何担保责任，甚至没有适销
 * 性或特定用途的隐含担保，详情请参照GPLv2许可协议。
 *
 * 您应已收到附随于本文件的GPLv2许可协议的副本，它通常在 LICENSE 文件中，如果
 * 没有，请查看：<http://www.gnu.org/licenses/>.
 * ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "finder.h"
#include "ui.h"
#include "i18n.h"
#include "file_storage.h"
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "labelbox.h"
#include "labelitem.h"
#include "picture.h"

typedef struct BoundingBoxRec_ {
	int id;
	float x, y, w, h;
} BoundingBoxRec, *BoundingBox;

typedef struct BoundingBoxItemRec_ {
	LCUI_Widget box;
	LCUI_Widget item;
	LCUI_RectF rect;
	LinkedListNode node;
} BoundingBoxItemRec, *BoundingBoxItem;

static struct PictureLabelsPanel {
	int worker_id;
	DB_File file;
	LCUI_BOOL visible;
	LCUI_BOOL loadable;
	LCUI_Widget panel;
	LCUI_Widget labels;
	LCUI_Widget available_labels;
	PictureLabelsViewContextRec ctx;
	Dict *labels_stats;
	LinkedList labels_trending;
	LinkedList boxes;
} labels_panel;

#define GetWidget LCUIWidget_GetById

static void RenderAvailableLabels(void);
static void LabelsPanel_SaveBoxesAsync(void);

static BoundingBox GetBoundingBox(BoundingBoxItem item)
{
	char *tagname;
	const wchar_t *name;

	DB_Tag tag;
	BoundingBox box;

	box = malloc(sizeof(BoundingBoxRec));
	if (!box) {
		return box;
	}
	name = LabelBox_GetNameW(item->box);
	tagname = EncodeANSI(name);
	tag = LCFinder_GetTag(tagname);
	if (tag) {
		box->id = tag->id;
		box->x = item->rect.x + item->rect.width / 2;
		box->y = item->rect.y + item->rect.height / 2;
		box->w = item->rect.width;
		box->h = item->rect.height;
	} else {
		free(box);
		box = NULL;
	}
	free(tagname);
	return box;
}

static BoundingBox *GetBoundingBoxes(void)
{
	size_t i = 0;
	LinkedListNode *node;
	BoundingBox *boxes;

	boxes = malloc(sizeof(BoundingBox) * (labels_panel.boxes.length + 1));
	if (!boxes) {
		return NULL;
	}
	for (LinkedList_Each(node, &labels_panel.boxes)) {
		boxes[i] = GetBoundingBox(node->data);
		if (boxes[i]) {
			i += 1;
		}
	}
	boxes[i] = NULL;
	return boxes;
}

static void DestroyBoundingBoxes(void *arg)
{
	size_t i;
	BoundingBox *boxes = arg;

	for (i = 0; boxes[i]; ++i) {
		free(boxes[i]);
	}
	free(boxes);
}

static int AddFileTag(const wchar_t *tagname)
{
	DB_Tag tag;
	char *name = EncodeANSI(tagname);

	tag = LCFinder_GetTag(name);
	if (!tag) {
		tag = LCFinder_AddTag(name);
	}
	free(name);
	return DBFile_AddTag(labels_panel.file, tag);
}

static int RemoveFileTag(const wchar_t *tagname)
{
	DB_Tag tag;
	char *name = EncodeANSI(tagname);

	tag = LCFinder_GetTag(name);
	free(name);
	if (!tag) {
		return -1;
	}
	return DBFile_RemoveTag(labels_panel.file, tag);
}

static LabelsStats_Add(const char *tagname)
{
	int *count;
	Dict *stats = labels_panel.labels_stats;

	count = Dict_FetchValue(stats, tagname);
	if (!count) {
		count = malloc(sizeof(int));
		*count = 0;
	}
	*count = *count + 1;
	Dict_Add(stats, (void *)tagname, count);
}

static LCUI_BOOL LabelsStats_Remove(const char *tagname)
{
	int *count;
	Dict *stats = labels_panel.labels_stats;

	count = Dict_FetchValue(stats, tagname);
	if (!count) {
		return FALSE;
	}
	*count = *count - 1;
	if (*count < 1) {
		Dict_Delete(stats, tagname);
		return FALSE;
	}
	Dict_Add(stats, (void *)tagname, count);
	return TRUE;
}

static void LabelsPanel_AddTag(const wchar_t *name)
{
	char *tagname;

	tagname = EncodeUTF8(name);
	AddFileTag(name);
	LabelsStats_Add(tagname);
	free(tagname);
}

static void LabelsPanel_RemoveTag(const wchar_t *name)
{
	char *tagname;

	tagname = EncodeUTF8(name);
	if (!LabelsStats_Remove(tagname)) {
		RemoveFileTag(name);
	}
	free(tagname);
}

static void LabelsTrending_Add(DB_Tag tag)
{
	DB_Tag t;
	LinkedListNode *node;
	LinkedList *list = &labels_panel.labels_trending;

	for (LinkedList_Each(node, list)) {
		t = node->data;
		if (t->id == tag->id) {
			LinkedList_Unlink(list, node);
			LinkedList_InsertNode(list, 0, node);
			return;
		}
	}
	LinkedList_Append(list, DBTag_Dup(tag));
}

static void OnDestroyBox(void *arg)
{
	BoundingBoxItem item = arg;

	Widget_Destroy(item->box);
	Widget_Destroy(item->item);
	item->box = NULL;
	item->item = NULL;
}

static void LabelsPanel_UpdateLabelItem(BoundingBoxItem item)
{
	LCUI_Rect img_rect;
	PictureLabelsViewContext ctx = &labels_panel.ctx;

	img_rect.x = iround(item->rect.x * ctx->width);
	img_rect.y = iround(item->rect.y * ctx->height);
	img_rect.width = iround(item->rect.width * ctx->width);
	img_rect.height = iround(item->rect.height * ctx->height);
	LabelItem_SetRect(item->item, &img_rect);
}

static void LabelsPanel_UpdateBox(BoundingBoxItem data)
{
	LCUI_RectF rect;
	PictureLabelsViewContext ctx = &labels_panel.ctx;
	float width = ctx->scale * ctx->width;
	float height = ctx->scale * ctx->height;

	rect.width = data->rect.width * width;
	rect.height = data->rect.height * height;
	rect.x = data->rect.x * width - (ctx->focus_x - ctx->offset_x);
	rect.y = data->rect.y * height - (ctx->focus_y - ctx->offset_y);
	LabelBox_SetRect(data->box, &rect);
	LabelsPanel_UpdateLabelItem(data);
	if (ctx->width < 1 || ctx->height < 1) {
		Widget_Hide(data->box);
	} else {
		Widget_Show(data->box);
	}
}

static void LabelsPanel_UpdateBoxes(void)
{
	LinkedListNode *node;

	for (LinkedList_Each(node, &labels_panel.boxes)) {
		LabelsPanel_UpdateBox(node->data);
	}
}

static void SetRandomRect(BoundingBoxItem data)
{
	data->rect.x = 0.1f + (rand() % 10) * 0.04f;
	data->rect.y = 0.1f + (rand() % 10) * 0.04f;
	data->rect.width = 0.3f + (rand() % 10) * 0.02f;
	data->rect.height = 0.3f + (rand() % 10) * 0.02f;
	LCUIRectF_ValidateArea(&data->rect, 1.0f, 1.0f);
	LabelsPanel_UpdateBox(data);
}

static void OnLabelBoxUpdate(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LCUI_RectF rect;
	BoundingBoxItem item = e->data;
	PictureLabelsViewContext ctx = &labels_panel.ctx;
	const wchar_t *tagname = LabelBox_GetNameW(item->box);
	const wchar_t *oldname = arg;
	float width = ctx->scale * ctx->width;
	float height = ctx->scale * ctx->height;

	LabelBox_GetRect(item->box, &rect);
	item->rect.x = (rect.x + ctx->focus_x - ctx->offset_x) / width;
	item->rect.y = (rect.y + ctx->focus_y - ctx->offset_y) / height;
	item->rect.width = rect.width / width;
	item->rect.height = rect.height / height;
	if (LCUIRectF_ValidateArea(&item->rect, 1.0f, 1.0f)) {
		LabelsPanel_UpdateBox(item);
	}
	LabelItem_SetNameW(item->item, tagname);
	LabelsPanel_UpdateLabelItem(item);
	if (oldname && wcscmp(oldname, tagname) != 0) {
		LabelsPanel_RemoveTag(oldname);
		LabelsPanel_AddTag(tagname);
	}
	LabelsPanel_SaveBoxesAsync();
}

static void LabelsPanel_RemoveBox(BoundingBoxItem item)
{
	LinkedList_Unlink(&labels_panel.boxes, &item->node);
	OnDestroyBox(item);
	free(item);
}

static void OnLabelItemRemove(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	BoundingBoxItem item = e->data;

	LabelsPanel_RemoveTag(LabelBox_GetNameW(item->box));
	LabelsPanel_RemoveBox(item);
	LabelsPanel_SaveBoxesAsync();
}

static int LabelsPanel_SetBox(BoundingBoxItem item, BoundingBox box)
{
	DB_Tag tag;
	PictureLabelsViewContext ctx = &labels_panel.ctx;
	float width = ctx->scale * ctx->width;
	float height = ctx->scale * ctx->height;
	wchar_t *name;

	tag = LCFinder_GetTagById(box->id);
	if (!tag) {
		return -ENOENT;
	}
	item->rect.x = box->x;
	item->rect.y = box->y;
	item->rect.width = box->w;
	item->rect.height = box->h;
	name = DecodeANSI(tag->name);
	LCUIRectF_ValidateArea(&item->rect, 1.0f, 1.0f);
	LabelBox_SetNameW(item->box, name);
	LabelItem_SetNameW(item->item, name);
	LabelsPanel_UpdateBox(item);
	LabelsStats_Add(tag->name);
	free(name);
	return 0;
}

static BoundingBoxItem LabelsPanel_AddBox(void)
{
	BoundingBoxItem data;

	if (!labels_panel.ctx.view) {
		return NULL;
	}

	data = malloc(sizeof(BoundingBoxItemRec));
	if (!data) {
		return NULL;
	}

	data->box = LCUIWidget_New("labelbox");
	data->item = LCUIWidget_New("labelitem");
	data->node.data = data;

	Widget_BindEvent(data->box, "change", OnLabelBoxUpdate, data, NULL);
	Widget_BindEvent(data->item, "remove", OnLabelItemRemove, data, NULL);
	Widget_Append(labels_panel.ctx.view, data->box);
	Widget_Append(labels_panel.labels, data->item);
	LinkedList_AppendNode(&labels_panel.boxes, &data->node);
	return data;
}

static void LabelsPanel_FocusBox(LCUI_Widget labelitem)
{
	LinkedListNode *node;
	BoundingBoxItem item;

	for (LinkedList_Each(node, &labels_panel.boxes)) {
		item = node->data;
		if (labelitem) {
			if (item->item == labelitem) {
				Widget_UnsetStyle(item->box, key_opacity);
			} else {
				Widget_SetOpacity(item->box, 0.4f);
			}
		} else {
			Widget_UnsetStyle(item->box, key_opacity);
		}
		Widget_UpdateStyle(item->box, FALSE);
	}
}

static void LabelsPanel_ClearLabelStats(void)
{
	Dict_Empty(labels_panel.labels_stats);
}

static void LabelsPanel_ClearBoxes(void)
{
	LinkedList_ClearData(&labels_panel.boxes, OnDestroyBox);
}

static void LabelsPanel_OnLoadBoxes(LinkedList *list)
{
	LinkedListNode *node;
	BoundingBoxItem item;

	LabelsPanel_ClearBoxes();
	LabelsPanel_ClearLabelStats();
	for (LinkedList_Each(node, list)) {
		item = LabelsPanel_AddBox();
		if (LabelsPanel_SetBox(item, node->data) != 0) {
			LabelsPanel_RemoveBox(item);
		}
	}
	LinkedList_Clear(list, free);
	free(list);
}

static void LabelsPanel_LoadBoxes(wchar_t *filename)
{
	FILE *fp;
	wchar_t *datafile;
	BoundingBox box;
	LinkedList *boxes;

	if (filename != labels_panel.ctx.file || !labels_panel.visible) {
		return;
	}
	boxes = NEW(LinkedList, 1);
	LinkedList_Init(boxes);
	do {
		datafile = GetAnnotationFileNameW(filename);
		if (!datafile) {
			break;
		}
		fp = wfopen(datafile, L"r");
		LOG("[labels-panel] open data file: %ls\n", datafile);
		free(datafile);
		if (!fp) {
			break;
		}
		while (1) {
			box = NEW(BoundingBoxRec, 1);
			if (5 != fscanf(fp, "%d %f %f %f %f", &box->id, &box->x,
					&box->y, &box->w, &box->h)) {
				free(box);
				break;
			}
			box->x -= box->w / 2;
			box->y -= box->h / 2;
			LinkedList_Append(boxes, box);
		}
		fclose(fp);
	} while (0);
	// Switch to the UI thread to render data
	LCUI_PostSimpleTask(LabelsPanel_OnLoadBoxes, boxes, NULL);
}

static void LabelsPanel_SaveBoxes(wchar_t *file, BoundingBox *boxes)
{
	FILE *fp;
	size_t i;
	wchar_t *datafile;
	BoundingBox box;

	datafile = GetAnnotationFileNameW(file);
	fp = wfopen(datafile, L"w+");
	LOG("[labels-panel] write data file: %ls\n", datafile);
	free(datafile);
	if (!fp) {
		return;
	}
	for (i = 0; boxes[i]; ++i) {
		box = boxes[i];
		fprintf(fp, "%d %f %f %f %f\n", box->id, box->x, box->y, box->w,
			box->h);
	}
	fclose(fp);
}

static void LabelsPanel_LoadBoxesAsync(void)
{
	LCUI_TaskRec task = { 0 };

	if (!labels_panel.ctx.file) {
		return;
	}
	task.func = (LCUI_AppTaskFunc)LabelsPanel_LoadBoxes;
	task.arg[0] = labels_panel.ctx.file;
	if (labels_panel.worker_id) {
		LCUI_PostAsyncTaskTo(&task, labels_panel.worker_id);
	} else {
		labels_panel.worker_id = LCUI_PostAsyncTask(&task);
	}
}

static void LabelsPanel_SaveBoxesAsync(void)
{
	LCUI_TaskRec task = { 0 };

	if (!labels_panel.ctx.file) {
		return;
	}
	task.func = (LCUI_AppTaskFunc)LabelsPanel_SaveBoxes;
	task.arg[0] = wcsdup2(labels_panel.ctx.file);
	task.arg[1] = GetBoundingBoxes();
	task.destroy_arg[0] = free;
	task.destroy_arg[1] = DestroyBoundingBoxes;
	if (labels_panel.worker_id) {
		LCUI_PostAsyncTaskTo(&task, labels_panel.worker_id);
	} else {
		labels_panel.worker_id = LCUI_PostAsyncTask(&task);
	}
}

static void OnAddLabel(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	BoundingBoxItem data;
	PictureLabelsViewContext ctx = &labels_panel.ctx;
	float width = ctx->scale * ctx->width;
	float height = ctx->scale * ctx->height;
	const wchar_t *name;

	data = LabelsPanel_AddBox();
	if (!data) {
		return;
	}
	SetRandomRect(data);
	name = LabelBox_GetNameW(data->box);
	LabelItem_SetNameW(data->item, name);
	LabelsPanel_AddTag(name);
	LabelsPanel_SaveBoxesAsync();
}

static void OnHideView(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	PictureView_HideLabels();
}

static void OnLabelClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	BoundingBoxItem data;
	DB_Tag tag = e->data;
	wchar_t *name = DecodeUTF8(tag->name);

	LabelsTrending_Add(tag);
	data = LabelsPanel_AddBox();
	SetRandomRect(data);
	LabelBox_SetNameW(data->box, name);
	LabelItem_SetNameW(data->item, name);
	LabelsPanel_AddTag(name);
	free(name);
	RenderAvailableLabels();
	LabelsPanel_SaveBoxesAsync();
}

static void OnLabelEventDestroy(void *data)
{
	DBTag_Release(data);
}

static LCUI_Widget CreateLabel(DB_Tag tag)
{
	LCUI_Widget label;

	tag = DBTag_Dup(tag);
	label = LCUIWidget_New("textview");
	Widget_AddClass(label, "label");
	Widget_BindEvent(label, "click", OnLabelClick, tag,
			 OnLabelEventDestroy);
	TextView_SetText(label, tag->name);
	return label;
}

static void RenderAvailableLabels(void)
{
	size_t i;
	DB_Tag tag;
	LCUI_BOOL found;
	LinkedListNode *node;

	Widget_Empty(labels_panel.available_labels);
	for (LinkedList_Each(node, &labels_panel.labels_trending)) {
		Widget_Append(labels_panel.available_labels,
			      CreateLabel(node->data));
	}
	for (i = 0; i < finder.n_tags; ++i) {
		found = FALSE;
		for (LinkedList_Each(node, &labels_panel.labels_trending)) {
			tag = node->data;
			if (tag->id == finder.tags[i]->id) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			Widget_Append(labels_panel.available_labels,
				      CreateLabel(finder.tags[i]));
		}
	}
}

static void OnMouseOver(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LCUI_Widget target;

	target = Widget_GetClosest(e->target, "labelitem");
	if (target) {
		LabelsPanel_FocusBox(target);
	}
}

static void OnMouseOut(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	if (e->target == w) {
		LabelsPanel_FocusBox(NULL);
	}
}

static void OnDestroyLabelStat(void *privdata, void *data)
{
	free(data);
}

void PictureView_InitLabels(void)
{
	LCUI_Widget btn_add, btn_hide;

	LinkedList_Init(&labels_panel.boxes);
	LinkedList_Init(&labels_panel.labels_trending);
	labels_panel.visible = FALSE;
	labels_panel.loadable = TRUE;
	labels_panel.worker_id = 0;
	labels_panel.labels_stats = StrDict_Create(NULL, OnDestroyLabelStat);
	labels_panel.panel = GetWidget(ID_PANEL_PICTURE_LABELS);
	labels_panel.labels = GetWidget(ID_VIEW_PICTURE_LABELS);
	labels_panel.available_labels = GetWidget(ID_VIEW_PICTURE_AVAIL_LABELS);
	btn_add = GetWidget(ID_BTN_ADD_PICTURE_LABEL);
	btn_hide = GetWidget(ID_BTN_HIDE_PICTURE_LABEL);
	Widget_BindEvent(btn_add, "click", OnAddLabel, NULL, NULL);
	Widget_BindEvent(btn_hide, "click", OnHideView, NULL, NULL);
	Widget_BindEvent(labels_panel.labels, "mouseover", OnMouseOver, NULL,
			 NULL);
	Widget_BindEvent(labels_panel.labels, "mouseout", OnMouseOut, NULL,
			 NULL);
	Widget_Hide(labels_panel.panel);
	RenderAvailableLabels();
}

static LCUI_BOOL LabelsPanel_SaveContext(PictureLabelsViewContext ctx)
{
	PictureLabelsViewContext current = &labels_panel.ctx;
	wchar_t *wfile = current->file;
	char *file;

	if (!ctx->file) {
		return FALSE;
	}
	if (wfile && wcscmp(ctx->file, wfile) == 0) {
		*current = *ctx;
		current->file = wfile;
		return FALSE;
	}
	if (wfile) {
		free(wfile);
	}
	if (labels_panel.file) {
		DBFile_Release(labels_panel.file);
	}
	file = EncodeUTF8(ctx->file);
	*current = *ctx;
	current->file = wcsdup2(ctx->file);
	labels_panel.file = DB_GetFile(file);
	free(file);
	return TRUE;
}

void PictureView_SetLabelsContext(PictureLabelsViewContext ctx)
{
	LCUI_BOOL file_changed;
	wchar_t *file = labels_panel.ctx.file;

	if (!labels_panel.visible) {
		LabelsPanel_SaveContext(ctx);
		labels_panel.loadable = TRUE;
		return;
	}
	if (!ctx->file) {
		return;
	}
	file_changed = LabelsPanel_SaveContext(ctx);
	if (labels_panel.loadable || file_changed) {
		LabelsPanel_LoadBoxesAsync();
		labels_panel.loadable = FALSE;
	} else {
		LCUI_PostSimpleTask(LabelsPanel_UpdateBoxes, NULL, NULL);
	}
}

static void LabelsPanel_ReloadContext(void)
{
	wchar_t *filename = labels_panel.ctx.file;

	if (!labels_panel.loadable || !filename) {
		return;
	}
	LabelsPanel_LoadBoxesAsync();
	labels_panel.loadable = FALSE;
}

void PictureView_ShowLabels(void)
{
	labels_panel.visible = TRUE;
	Widget_Show(labels_panel.panel);
	LabelsPanel_ReloadContext();
	Widget_AddClass(labels_panel.panel->parent, "has-panel");
}

void PictureView_HideLabels(void)
{
	labels_panel.visible = FALSE;
	labels_panel.loadable = TRUE;
	Widget_Hide(labels_panel.panel);
	Widget_RemoveClass(labels_panel.panel->parent, "has-panel");
	LabelsPanel_ClearBoxes();
	LabelsPanel_ClearLabelStats();
}

LCUI_BOOL PictureView_VisibleLabels(void)
{
	return labels_panel.visible;
}
