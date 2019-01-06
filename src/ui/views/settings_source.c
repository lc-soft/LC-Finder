/* ***************************************************************************
 * settings_source.c -- source folders setting view
 *
 * Copyright (C) 2019 by Liu Chao <lc-soft@live.cn>
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
 * settings_source.c -- “设置”视图中的数据源设置项
 *
 * 版权所有 (C) 2019 归属于 刘超 <lc-soft@live.cn>
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

#include "finder.h"
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "ui.h"
#include "i18n.h"
#include "dialog.h"
#include "settings.h"

static struct SourceSettingView {
	LCUI_Widget source_dirs;
	LCUI_Widget language;
	Dict *dirpaths;
} view;

#define KEY_DIALOG_TITLE_DEL_DIR	"settings.source_folders.removing_dialog.title"
#define KEY_DIALOG_TEXT_DEL_DIR		"settings.source_folders.removing_dialog.content"

static void OnBtnRemoveClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	DB_Dir dir = e->data;
	const wchar_t *text = I18n_GetText(KEY_DIALOG_TEXT_DEL_DIR);
	const wchar_t *title = I18n_GetText(KEY_DIALOG_TITLE_DEL_DIR);
	LCUI_Widget window = LCUIWidget_GetById(ID_WINDOW_MAIN);
	if (!LCUIDialog_Confirm(window, title, text)) {
		return;
	}
	LCFinder_DeleteDir(dir);
}

LCUI_Widget SourceListItem_Create(DB_Dir dir)
{
	LCUI_Widget item, icon, text, btn;

	item = LCUIWidget_New(NULL);
	icon = LCUIWidget_New("textview");
	text = LCUIWidget_New("textview");
	btn = LCUIWidget_New("textview");
	Widget_AddClass(item, "source-list-item");
	Widget_AddClass(icon, "icon icon-folder-outline");
	Widget_AddClass(text, "text");
	Widget_AddClass(btn, "button icon icon-close");
	TextView_SetText(text, dir->path);
	Widget_BindEvent(btn, "click", OnBtnRemoveClick, dir, NULL);
	Widget_Append(item, icon);
	Widget_Append(item, text);
	Widget_Append(item, btn);
	return item;
}

static void OnDelDir(void *privdata, void *arg)
{
	Dict *dirpaths;
	DB_Dir dir = arg;
	LCUI_Widget item;

	if (!dir->visible) {
		return;
	}
	dirpaths = view.dirpaths;
	item = Dict_FetchValue(dirpaths, dir->path);
	if (item) {
		Dict_Delete(dirpaths, dir->path);
		Widget_Destroy(item);
	}
}

static void OnAddDir(void *privdata, void *arg)
{
	DB_Dir dir = arg;
	LCUI_Widget item = SourceListItem_Create(dir);

	if (dir->visible) {
		Widget_Append(view.source_dirs, item);
		Dict_Add(view.dirpaths, dir->path, item);
	}
}

static void OnSelectDirDone(const wchar_t *wpath, const wchar_t *wtoken)
{
	char *token = NULL;
	char *path = EncodeUTF8(wpath);
	if (wtoken) {
		token = EncodeUTF8(wtoken);
	}
	if (!LCFinder_GetDir(path)) {
		DB_Dir dir = LCFinder_AddDir(path, token, TRUE);
		if (dir) {
			LCFinder_TriggerEvent(EVENT_DIR_ADD, dir);
		}
	}
	free(path);
	if (token) {
		free(token);
	}
}

static void OnSelectDir(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	SelectFolderAsyncW(OnSelectDirDone);
}

static void InitDirList(void)
{
	size_t i;
	LCUI_Widget item;
	view.dirpaths = StrDict_Create(NULL, NULL);
	for (i = 0; i < finder.n_dirs; ++i) {
		if (!finder.dirs[i]->visible) {
			continue;
		}
		item = SourceListItem_Create(finder.dirs[i]);
		Widget_Append(view.source_dirs, item);
		Dict_Add(view.dirpaths, finder.dirs[i]->path, item);
	}
}

void SettingsView_InitSource(void)
{
	LCUI_Widget btn;

	SelectWidget(view.source_dirs, ID_VIEW_SOURCE_LIST);
	SelectWidget(btn, ID_BTN_ADD_SOURCE);
	BindEvent(btn, "click", OnSelectDir);
	LCFinder_BindEvent(EVENT_DIR_ADD, OnAddDir, NULL);
	LCFinder_BindEvent(EVENT_DIR_DEL, OnDelDir, NULL);
	InitDirList();
}