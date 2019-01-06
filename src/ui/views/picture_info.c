/* ***************************************************************************
 * picture_info.c -- picture info view
 *
 * Copyright (C) 2016-2018 by Liu Chao <lc-soft@live.cn>
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
 * picture_info.c -- "图片信息" 视图
 *
 * 版权所有 (C) 2016-2018 归属于 刘超 <lc-soft@live.cn>
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

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "finder.h"
#include "ui.h"
#include "i18n.h"
#include "file_storage.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/cursor.h>
#include <LCUI/graph.h>
#include <LCUI/gui/builder.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "dialog.h"
#include "starrating.h"

#define MAX_TAG_LEN			256
#define KEY_UNKNOWN			"picture.unknown"
#define KEY_TITLE_ADD_TAG		"picture.dialog.title.add_tag"
#define KEY_TITLE_DEL_TAG		"picture.dialog.title.delete_tag"
#define KEY_PLACEHOLDER_INPUT_TAG	"picture.dialog.placeholder.input_tag"
#define KEY_TEXT_DEL_TAG		"picture.dialog.text.delete_tag"

static struct PictureInfoPanel {
	LCUI_Widget window;
	LCUI_Widget panel;
	LCUI_Widget txt_name;
	LCUI_Widget txt_time;
	LCUI_Widget txt_size;
	LCUI_Widget txt_fsize;
	LCUI_Widget txt_dirpath;
	LCUI_Widget view_tags;
	LCUI_Widget rating;
	wchar_t *filepath;
	uint64_t size;
	uint_t width;
	uint_t height;
	time_t mtime;
	DB_File file;
	DB_Tag *tags;
	int n_tags;
} view = { 0 };

typedef struct TagInfoPackRec_ {
	LCUI_Widget widget;
	DB_Tag tag;
} TagInfoPackRec, *TagInfoPack;

static int wgettimestr(wchar_t *str, int max_len, time_t time)
{
	int n;
	struct tm *t;
	char key[64], wday_key[10], month_key[4];
	const wchar_t *format, *month_str, *wday_str;
	wchar_t buf[256], year_str[6], mday_str[4], hour_str[4], min_str[4];
	t = localtime(&time);
	if (!t) {
		return -1;
	}
	format = I18n_GetText("datetime.format");
	if (!format) {
		wcscpy(str, L"<translation missing>");
		return 0;
	}
	wcscpy(buf, format);
	swprintf(year_str, 5, L"%d", 1900 + t->tm_year);
	swprintf(mday_str, 3, L"%d", t->tm_mday);
	swprintf(hour_str, 3, L"%d", t->tm_hour);
	swprintf(min_str, 3, L"%d", t->tm_min);
	snprintf(month_key, 3, "%d", t->tm_mon);
	snprintf(wday_key, 9, "%d", t->tm_wday);
	snprintf(key, 63, "datetime.months.%s", month_key);
	month_str = I18n_GetText(key);
	if (!month_str) {
		wcscpy(str, L"<translation missing>");
		return 0;
	}
	snprintf(key, 63, "datetime.days.%s", wday_key);
	wday_str = I18n_GetText(key);
	if (!wday_str) {
		wcscpy(str, L"<translation missing>");
		return 0;
	}
	n = wcsreplace(buf, 255, L"YYYY", year_str);
	n += wcsreplace(buf, 255, L"MM", month_str);
	n += wcsreplace(buf, 255, L"DD", mday_str);
	n += wcsreplace(buf, 255, L"DATE", wday_str);
	n += wcsreplace(buf, 255, L"hh", hour_str);
	n += wcsreplace(buf, 255, L"mm", min_str);
	wcsncpy(str, buf, max_len);
	return n;
}

static LCUI_BOOL CheckTagName(const wchar_t *tagname)
{
	if (wgetcharcount(tagname, L",;\n\r\t") > 0) {
		return FALSE;
	}
	if (wcslen(tagname) >= MAX_TAG_LEN) {
		return FALSE;
	}
	return TRUE;
}

static void OnBtnDeleteTagClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	int i;
	wchar_t buf[256] = { 0 };
	TagInfoPack pack = e->data;
	wchar_t *name = DecodeUTF8(pack->tag->name);
	const wchar_t *text = I18n_GetText(KEY_TEXT_DEL_TAG);
	const wchar_t *title = I18n_GetText(KEY_TITLE_DEL_TAG);

	swprintf(buf, 255, text, name);
	free(name);

	if (!LCUIDialog_Confirm(view.window, title, buf)) {
		return;
	}
	LCFinder_TriggerEvent(EVENT_TAG_UPDATE, pack->tag);
	DBFile_RemoveTag(view.file, pack->tag);
	Widget_Destroy(pack->widget);
	pack->tag->count -= 1;
	for (i = 0; i < view.n_tags; ++i) {
		if (view.tags[i]->id != pack->tag->id) {
			return;
		}
		view.n_tags -= 1;
		for (; i < view.n_tags; ++i) {
			view.tags[i] = view.tags[i + 1];
		}
		view.tags[view.n_tags] = NULL;
	}
}

static void PictureInfo_AppendTag(DB_Tag tag)
{
	size_t size;
	DB_Tag *tags;
	TagInfoPack pack;
	LCUI_Widget box, txt_name, btn_del;

	view.n_tags += 1;
	size = (view.n_tags + 1) * sizeof(DB_Tag);
	tags = realloc(view.tags, size);
	if (!tags) {
		view.n_tags -= 1;
		return;
	}
	tags[view.n_tags - 1] = tag;
	tags[view.n_tags] = NULL;
	pack = NEW(TagInfoPackRec, 1);
	box = LCUIWidget_New(NULL);
	txt_name = LCUIWidget_New("textview");
	btn_del = LCUIWidget_New("textview");
	Widget_AddClass(box, "tag-list-item");
	Widget_AddClass(btn_del, "btn-delete icon icon-close");
	TextView_SetText(txt_name, tag->name);
	Widget_Append(box, txt_name);
	Widget_Append(box, btn_del);
	Widget_Append(view.view_tags, box);
	pack->widget = box;
	pack->tag = tag;
	view.tags = tags;
	Widget_BindEvent(btn_del, "click", OnBtnDeleteTagClick, pack, free);
}

static void OnSetRating(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	int rating = StarRating_GetRating(w);
	DBFile_SetScore(view.file, rating);
}

static void OnBtnHideClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	UI_HidePictureInfoView();
}

static void OnBtnOpenDirClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	OpenFileManagerW(view.filepath);
}

static void OnBtnAddTagClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	int i, j;
	DB_Tag tag;
	char *buf, **tagnames;
	wchar_t text[MAX_TAG_LEN];
	const wchar_t *title = I18n_GetText(KEY_TITLE_ADD_TAG);
	const wchar_t *holder = I18n_GetText(KEY_PLACEHOLDER_INPUT_TAG);
	LCUI_Widget window = LCUIWidget_GetById(ID_WINDOW_PCITURE_VIEWER);
	if (0 != LCUIDialog_Prompt(window, title, holder, NULL, text,
				   MAX_TAG_LEN, CheckTagName)) {
		return;
	}
	buf = EncodeUTF8(text);
	strsplit(buf, " ", &tagnames);
	for (i = 0; tagnames[i]; ++i) {
		if (strlen(tagnames[i]) == 0) {
			continue;
		}
		tag = LCFinder_AddTagForFile(view.file, tagnames[i]);
		if (tag) {
			for (j = 0; j < view.n_tags; ++j) {
				if (view.tags[j]->id == tag->id) {
					break;
				}
			}
			if (j >= view.n_tags) {
				PictureInfo_AppendTag(tag);
			}
		}
	}
	freestrs(tagnames);
	free(buf);
}

void UI_InitPictureInfoView(void)
{
	LCUI_Widget btn_hide, btn_add_tag, btn_open;

	view.n_tags = 0;
	view.tags = NULL;
	view.filepath = NULL;
	btn_hide = LCUIWidget_GetById(ID_BTN_HIDE_PICTURE_INFO);
	btn_open = LCUIWidget_GetById(ID_BTN_OPEN_PICTURE_DIR);
	btn_add_tag = LCUIWidget_GetById(ID_BTN_ADD_PICTURE_TAG);
	view.txt_fsize = LCUIWidget_GetById(ID_TXT_PICTURE_FILE_SIZE);
	view.txt_size = LCUIWidget_GetById(ID_TXT_PICTURE_SIZE);
	view.txt_name = LCUIWidget_GetById(ID_TXT_PICTURE_NAME);
	view.txt_dirpath = LCUIWidget_GetById(ID_TXT_PICTURE_PATH);
	view.txt_time = LCUIWidget_GetById(ID_TXT_PICTURE_TIME);
	view.panel = LCUIWidget_GetById(ID_PANEL_PICTURE_INFO);
	view.view_tags = LCUIWidget_GetById(ID_VIEW_PICTURE_TAGS);
	view.window = LCUIWidget_GetById(ID_WINDOW_PCITURE_VIEWER);
	view.rating = LCUIWidget_GetById(ID_VIEW_PCITURE_RATING);
	Widget_BindEvent(view.rating, "click", OnSetRating, NULL, NULL);
	Widget_BindEvent(btn_hide, "click", OnBtnHideClick, NULL, NULL);
	Widget_BindEvent(btn_add_tag, "click", OnBtnAddTagClick, NULL, NULL);
	Widget_BindEvent(btn_open, "click", OnBtnOpenDirClick, NULL, NULL);
	Widget_Hide(view.panel);
}

static void OnGetFileStatus(FileStatus *status, void *data)
{
	wchar_t mtime_str[256], fsize_str[256];
	const wchar_t *unknown = I18n_GetText(KEY_UNKNOWN);
	if (!status) {
		TextView_SetTextW(view.txt_time, unknown);
		TextView_SetTextW(view.txt_fsize, unknown);
		TextView_SetTextW(view.txt_size, unknown);
		return;
	}
	if (status->image) {
		wchar_t str[256];
		view.width = status->image->width;
		view.height = status->image->height;
		swprintf(str, 256, L"%dx%d",
			 status->image->width, status->image->height);
		TextView_SetTextW(view.txt_size, str);
	} else {
		TextView_SetTextW(view.txt_size, unknown);
	}
	view.size = status->size;
	view.mtime = status->mtime;
	wgetsizestr(fsize_str, 256, view.size);
	if (wgettimestr(mtime_str, 256, view.mtime) < 0) {
		wcscpy(mtime_str, unknown);
	}
	TextView_SetTextW(view.txt_time, mtime_str);
	TextView_SetTextW(view.txt_fsize, fsize_str);
}

void UI_SetPictureInfoView(const char *filepath)
{
	int i, n;
	DB_Tag *tags;
	wchar_t *path, *dirpath;
	int storage = finder.storage;

	path = DecodeUTF8(filepath);
	dirpath = wgetdirname(path);
	FileStorage_GetStatus(storage, path, TRUE, OnGetFileStatus, NULL);
	TextView_SetTextW(view.txt_dirpath, dirpath);
	TextView_SetTextW(view.txt_name, wgetfilename(path));
	if (view.filepath) {
		free(view.filepath);
	}
	free(view.tags);
	view.n_tags = 0;
	view.tags = NULL;
	view.filepath = path;
	view.file = DB_GetFile(filepath);
	Widget_Empty(view.view_tags);
	/* 当没有文件记录时，说明该文件并不是源文件夹内的文件，暂时禁用部分操作 */
	if (!view.file) {
		Widget_Hide(view.view_tags->parent);
		Widget_Hide(view.rating->parent);
		return;
	}
	Widget_Show(view.view_tags->parent);
	Widget_Show(view.rating->parent);
	StarRating_SetRating(view.rating, view.file->score);
	n = LCFinder_GetFileTags(view.file, &tags);
	for (i = 0; i < n; ++i) {
		PictureInfo_AppendTag(tags[i]);
	}
	/* 因为标签记录是引用自 finder.tags 里的，所以这里只需要释放 tags */
	if (n > 0) {
		free(tags);
	}
}

void UI_ShowPictureInfoView(void)
{
	Widget_Show(view.panel);
	Widget_AddClass(view.panel->parent, "has-panel");
}

void UI_HidePictureInfoView(void)
{
	Widget_Hide(view.panel);
	Widget_RemoveClass(view.panel->parent, "has-panel");
}
