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
	DB_Tag *tags;
	size_t n_tags;
	LCUI_BOOL visible;
	LCUI_BOOL loadable;
	LCUI_Widget panel;
	LCUI_Widget labels;
	LCUI_Widget available_labels;
	PictureLabelsViewContextRec ctx;
	LinkedList labels_trending;
	LinkedList boxes;
} labels_panel;

#define GetWidget LCUIWidget_GetById

static void RenderAvailableLabels(void);
static void LabelsPanel_SaveBoxesAsync(void);

static DB_Tag GetTagById(int id)
{
	size_t i;

	if (!labels_panel.tags) {
		return NULL;
	}
	for (i = 0; i < labels_panel.n_tags; ++i) {
		if (labels_panel.tags[i]->id == id) {
			return labels_panel.tags[i];
		}
	}
	return NULL;
}

static wchar_t *GetDataFileName(wchar_t *file)
{
	const wchar_t *ext;
	wchar_t *datafile;

	datafile = malloc(sizeof(wchar_t) * (wcslen(file) + 7));
	if (!datafile) {
		return NULL;
	}
	wcscpy(datafile, file);
	ext = wgetfileext(datafile);
	if (ext) {
		wcscpy(datafile + (ext - datafile), L".txt");
	} else {
		wcscat(datafile, L".txt");
	}
	return datafile;
}

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
	if (!tag) {
		tag = LCFinder_AddTag(tagname);
	}
	box->id = tag->id;
	box->x = item->rect.x;
	box->y = item->rect.y;
	box->w = item->rect.width;
	box->h = item->rect.height;
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
		i += 1;
	}
	boxes[labels_panel.boxes.length] = NULL;
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

static void LabelsPanel_UpdateLabelItem(BoundingBoxItem item)
{
	LCUI_Rect img_rect;
	PictureLabelsViewContext ctx = &labels_panel.ctx;
	float width = ctx->scale * ctx->width;
	float height = ctx->scale * ctx->height;

	img_rect.x = iround(item->rect.x * width);
	img_rect.y = iround(item->rect.y * height);
	img_rect.width = iround(item->rect.width * width);
	img_rect.height = iround(item->rect.height * height);
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

static void LabelsPanel_ReloadTags(void)
{
	size_t i;

	if (labels_panel.tags) {
		for (i = 0; i < labels_panel.n_tags; ++i) {
			DBTag_Release(labels_panel.tags[i]);
		}
		free(labels_panel.tags);
	}
	labels_panel.n_tags = DB_GetTagsOrderById(&labels_panel.tags);
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
	LabelItem_SetNameW(item->item, LabelBox_GetNameW(item->box));
	LabelsPanel_UpdateLabelItem(item);
	LabelsPanel_SaveBoxesAsync();
}

static void OnLabelItemRemove(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	BoundingBoxItem data = e->data;

	LinkedList_Unlink(&labels_panel.boxes, &data->node);
	LabelsPanel_SaveBoxesAsync();
	Widget_Destroy(data->box);
	Widget_Destroy(data->item);
	free(data);
}

static int LabelsPanel_SetBox(BoundingBoxItem item, BoundingBox box)
{
	DB_Tag tag;
	PictureLabelsViewContext ctx = &labels_panel.ctx;
	float width = ctx->scale * ctx->width;
	float height = ctx->scale * ctx->height;
	wchar_t *name;

	tag = GetTagById(box->id);
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

static void OnDestroyBox(void *arg)
{
	BoundingBoxItem item = arg;

	Widget_Destroy(item->box);
	Widget_Destroy(item->item);
	item->box = NULL;
	item->item = NULL;
}

static void LabelsPanel_ClearBoxes(void)
{
	LinkedList_ClearData(&labels_panel.boxes, OnDestroyBox);
}

static void LabelsPanel_LoadBoxes(wchar_t *file)
{
	FILE *fp;
	wchar_t *datafile;
	BoundingBoxRec box;
	BoundingBoxItem item;

	if (file != labels_panel.ctx.file || !labels_panel.visible) {
		return;
	}
	LabelsPanel_ClearBoxes();
	datafile = GetDataFileName(file);
	if (!datafile) {
		return;
	}
	fp = wfopen(datafile, L"r");
	LOG("[labels-panel] open data file: %ls\n", datafile);
	free(datafile);
	if (!fp) {
		return;
	}
	LabelsPanel_ReloadTags();
	if (file != labels_panel.ctx.file || !labels_panel.visible) {
		fclose(fp);
		return;
	}
	while (5 == fscanf(fp, "%d %f %f %f %f", &box.id, &box.x, &box.y,
			   &box.w, &box.h)) {
		item = LabelsPanel_AddBox();
		LabelsPanel_SetBox(item, &box);
	}
	fclose(fp);
}

static void LabelsPanel_SaveBoxes(wchar_t *file, BoundingBox *boxes)
{
	FILE *fp;
	size_t i;
	wchar_t *datafile;
	BoundingBox box;

	datafile = GetDataFileName(file);
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

	data = LabelsPanel_AddBox();
	if (!data) {
		return;
	}
	SetRandomRect(data);
	LabelItem_SetNameW(data->item, LabelBox_GetNameW(data->box));
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

void PictureView_InitLabels(void)
{
	LCUI_Widget btn_add, btn_hide;

	LinkedList_Init(&labels_panel.boxes);
	LinkedList_Init(&labels_panel.labels_trending);
	labels_panel.visible = FALSE;
	labels_panel.loadable = TRUE;
	labels_panel.worker_id = 0;
	labels_panel.tags = NULL;
	labels_panel.n_tags = 0;
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

void PictureView_SetLabelsContext(PictureLabelsViewContext ctx)
{
	wchar_t *file = labels_panel.ctx.file;

	if (!labels_panel.visible) {
		labels_panel.loadable = TRUE;
		return;
	}
	if (!ctx->file) {
		return;
	}
	if (!labels_panel.loadable && file && wcscmp(ctx->file, file) == 0) {
		labels_panel.ctx = *ctx;
		labels_panel.ctx.file = file;
		LabelsPanel_UpdateBoxes();
	} else {
		if (file) {
			free(file);
		}
		file = wcsdup2(ctx->file);
		labels_panel.ctx = *ctx;
		labels_panel.ctx.file = file;
		LabelsPanel_LoadBoxesAsync();
		labels_panel.loadable = FALSE;
	}
}

static void PictureView_ReloadLabelsContext(void)
{
	wchar_t *file = labels_panel.ctx.file;

	if (!labels_panel.loadable || !file) {
		return;
	}
	LabelsPanel_LoadBoxesAsync();
	labels_panel.loadable = FALSE;
}

void PictureView_ShowLabels(void)
{
	labels_panel.visible = TRUE;
	Widget_Show(labels_panel.panel);
	PictureView_ReloadLabelsContext();
	Widget_AddClass(labels_panel.panel->parent, "has-panel");
}

void PictureView_HideLabels(void)
{
	labels_panel.visible = FALSE;
	labels_panel.loadable = TRUE;
	Widget_Hide(labels_panel.panel);
	Widget_RemoveClass(labels_panel.panel->parent, "has-panel");
	LabelsPanel_ClearBoxes();
}

LCUI_BOOL PictureView_VisibleLabels(void)
{
	return labels_panel.visible;
}
