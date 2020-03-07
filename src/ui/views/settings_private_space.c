/* ***************************************************************************
 * settings_private_space.c -- private space setting view
 *
 * Copyright (C) 2019-2020 by Liu Chao <lc-soft@live.cn>
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
 * settings_private_space.c -- “设置”视图中的私人空间设置项
 *
 * 版权所有 (C) 2019-2020 归属于 刘超 <lc-soft@live.cn>
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

#include <string.h>
#include <stdlib.h>
#include "finder.h"
#include <LCDesign.h>
#include <LCUI/gui/widget.h>
#include "ui.h"
#include "i18n.h"
#include "dialog.h"
#include "textview_i18n.h"
#include "settings.h"

#define KEY_VERIFY_PASSWORD_TITLE	"settings.private_space.verify_dialog.title"
#define KEY_VERIFY_PASSWORD_TEXT	"settings.private_space.verify_dialog.text"
#define KEY_NEW_PASSWORD_TITLE		"settings.private_space.new_dialog.title"
#define KEY_NEW_PASSWORD_TEXT		"settings.private_space.new_dialog.text"
#define KEY_RESET_PASSWORD_TITLE	"settings.private_space.reset_dialog.title"
#define KEY_RESET_PASSWORD_TEXT		"settings.private_space.reset_dialog.text"

static struct PrivateSpaceSettingView {
	LCUI_Widget view;
	LCUI_Widget source_dirs;
	LCUI_BOOL is_loaded;
	Dict *dirpaths;
} view;

static LCUI_BOOL OnCheckPassword(const char *password, const char *data)
{
	char buf[48];
	const char *pwd = data;
	EncodeSHA1(buf, password, strlen(password));
	return strcmp(pwd, buf) == 0;
}

static void InitPrivateDirList(void)
{
	size_t i;
	LCUI_Widget item;

	view.dirpaths = StrDict_Create(NULL, NULL);
	for (i = 0; i < finder.n_dirs; ++i) {
		if (finder.dirs[i] && finder.dirs[i]->visible) {
			continue;
		}
		item = SourceListItem_Create(finder.dirs[i]);
		Widget_Append(view.source_dirs, item);
		Dict_Add(view.dirpaths, finder.dirs[i]->path, item);
	}
}

static void OnBtnResetPasswordClick(LCUI_Widget w, LCUI_WidgetEvent e,
				    void *arg)
{
	char *buf;
	wchar_t wbuf[64];
	char *pwd = finder.config.encrypted_password;
	LCUI_Widget window = LCUIWidget_GetById(ID_WINDOW_MAIN);
	const wchar_t *title = I18n_GetText(KEY_RESET_PASSWORD_TITLE);
	const wchar_t *text = I18n_GetText(KEY_RESET_PASSWORD_TEXT);
	int ret = LCUIDialog_NewPassword(window, title, text, wbuf);
	if (ret != 0) {
		return;
	}
	buf = EncodeUTF8(wbuf);
	Logger_Debug("new password: %ls\n", wbuf);
	EncodeSHA1(pwd, buf, strlen(buf));
	LCFinder_SaveConfig();
	free(buf);
}

static void OnPrivateSpaceSwitchChange(LCUI_Widget w, LCUI_WidgetEvent e,
				       void *arg)
{
	int ret;
	const wchar_t *title, *text;
	char *pwd = finder.config.encrypted_password;
	LCUI_Widget window = LCUIWidget_GetById(ID_WINDOW_MAIN);
	if (!Switch_IsChecked(w)) {
		finder.open_private_space = FALSE;
		Widget_AddClass(view.view, "hide");
		LCFinder_TriggerEvent(EVENT_PRIVATE_SPACE_CHG, NULL);
		return;
	}
	if (strlen(pwd) > 0) {
		title = I18n_GetText(KEY_VERIFY_PASSWORD_TITLE);
		text = I18n_GetText(KEY_VERIFY_PASSWORD_TEXT);
		ret = LCUIDialog_CheckPassword(window, title, text,
					       OnCheckPassword, pwd);
		if (ret != 0) {
			Switch_SetChecked(w, FALSE);
			Widget_AddClass(view.view, "hide");
			return;
		}
	} else {
		char *buf;
		wchar_t wbuf[64];
		title = I18n_GetText(KEY_NEW_PASSWORD_TITLE);
		text = I18n_GetText(KEY_NEW_PASSWORD_TEXT);
		ret = LCUIDialog_NewPassword(window, title, text, wbuf);
		if (ret != 0) {
			Switch_SetChecked(w, FALSE);
			Widget_AddClass(view.view, "hide");
			return;
		}
		buf = EncodeUTF8(wbuf);
		Logger_Debug("new password: %ls\n", wbuf);
		EncodeSHA1(pwd, buf, strlen(buf));
		LCFinder_SaveConfig();
		free(buf);
	}
	if (!view.is_loaded) {
		InitPrivateDirList();
		view.is_loaded = TRUE;
	}
	finder.open_private_space = TRUE;
	Widget_RemoveClass(view.view, "hide");
	LCFinder_TriggerEvent(EVENT_PRIVATE_SPACE_CHG, NULL);
}

static void OnSelectPrivateDirDone(const wchar_t *wpath, const wchar_t *wtoken)
{
	char *token = NULL;
	char *path = EncodeUTF8(wpath);
	if (wtoken) {
		token = EncodeUTF8(wtoken);
	}
	if (!LCFinder_GetDir(path)) {
		DB_Dir dir = LCFinder_AddDir(path, token, FALSE);
		if (dir) {
			LCFinder_TriggerEvent(EVENT_DIR_ADD, dir);
		}
	}
	free(path);
	if (token) {
		free(token);
	}
}

static void OnSelectPrivateDir(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	SelectFolderAsyncW(OnSelectPrivateDirDone);
}

static void OnDelDir(void *privdata, void *arg)
{
	Dict *dirpaths;
	DB_Dir dir = arg;
	LCUI_Widget item;

	if (dir->visible) {
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
		return;
	}
	Widget_Append(view.source_dirs, item);
	Dict_Add(view.dirpaths, dir->path, item);
}

void SettingsView_InitPrivateSpace(void)
{
	LCUI_Widget btn, btn_reset, switcher;

	SelectWidget(switcher, ID_SWITCH_PRIVATE_SPACE);
	BindEvent(switcher, "change", OnPrivateSpaceSwitchChange);
	SelectWidget(view.source_dirs, ID_VIEW_PRIVATE_SOURCE_LIST);
	SelectWidget(view.view, ID_VIEW_PRIVATE_SPACE);
	SelectWidget(btn, ID_BTN_ADD_PRIVATE_SOURCE);
	SelectWidget(btn_reset, ID_BTN_RESET_PASSWORD);
	BindEvent(btn, "click", OnSelectPrivateDir);
	BindEvent(btn_reset, "click", OnBtnResetPasswordClick);
	LCFinder_BindEvent(EVENT_DIR_ADD, OnAddDir, NULL);
	LCFinder_BindEvent(EVENT_DIR_DEL, OnDelDir, NULL);
	InitPrivateDirList();
	view.is_loaded = FALSE;
}
