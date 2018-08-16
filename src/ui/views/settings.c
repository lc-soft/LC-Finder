/* ***************************************************************************
 * view_settings.c -- settings view
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
 * view_settings.c -- “设置”视图
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

/* clang-format off */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "finder.h"
#include "ui.h"
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "switch.h"
#include "dialog.h"
#include "textview_i18n.h"
#include "bridge.h"
#include "i18n.h"

#define KEY_CLEANING 			"button.cleaning"
#define KEY_CLEAR			"button.clear" 
#define KEY_DIALOG_TITLE_DEL_DIR	"settings.source_folders.removing_dialog.title"
#define KEY_DIALOG_TEXT_DEL_DIR		"settings.source_folders.removing_dialog.content"
#define KEY_VERIFY_PASSWORD_TITLE	"settings.private_space.verify_dialog.title"
#define KEY_VERIFY_PASSWORD_TEXT	"settings.private_space.verify_dialog.text"
#define KEY_NEW_PASSWORD_TITLE		"settings.private_space.new_dialog.title"
#define KEY_NEW_PASSWORD_TEXT		"settings.private_space.new_dialog.text"
#define KEY_RESET_PASSWORD_TITLE	"settings.private_space.reset_dialog.title"
#define KEY_RESET_PASSWORD_TEXT		"settings.private_space.reset_dialog.text"

static struct SettingsViewData {
	LCUI_Widget source_dirs;
	LCUI_Widget thumb_db_stats;
	LCUI_Widget language;
	Dict *dirpaths;
} this_view;

static struct PrivateSpaceViewData {
	LCUI_Widget view;
	LCUI_Widget source_dirs;
	LCUI_BOOL is_loaded;
	Dict *dirpaths;
} private_space_view;

/* clang-format on */

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

static LCUI_Widget NewDirListItem(DB_Dir dir)
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
	if (dir->visible) {
		dirpaths = this_view.dirpaths;
	} else {
		dirpaths = private_space_view.dirpaths;
	}
	item = Dict_FetchValue(dirpaths, dir->path);
	if (item) {
		Dict_Delete(dirpaths, dir->path);
		Widget_Destroy(item);
	}
}

static void OnAddDir(void *privdata, void *arg)
{
	DB_Dir dir = arg;
	LCUI_Widget item = NewDirListItem(dir);
	if (dir->visible) {
		Widget_Append(this_view.source_dirs, item);
		Dict_Add(this_view.dirpaths, dir->path, item);
	} else {
		Widget_Append(private_space_view.source_dirs, item);
		Dict_Add(private_space_view.dirpaths, dir->path, item);
	}
}

/** 初始化文件夹目录控件 */
static void UI_InitDirList(void)
{
	size_t i;
	LCUI_Widget item;
	this_view.dirpaths = StrDict_Create(NULL, NULL);
	for (i = 0; i < finder.n_dirs; ++i) {
		if (!finder.dirs[i]->visible) {
			continue;
		}
		item = NewDirListItem(finder.dirs[i]);
		Widget_Append(this_view.source_dirs, item);
		Dict_Add(this_view.dirpaths, finder.dirs[i]->path, item);
	}
}

static void UI_InitPrivateDirList(void)
{
	size_t i;
	LCUI_Widget item;
	struct PrivateSpaceViewData *self = &private_space_view;
	self->dirpaths = StrDict_Create(NULL, NULL);
	for (i = 0; i < finder.n_dirs; ++i) {
		if (finder.dirs[i]->visible) {
			continue;
		}
		item = NewDirListItem(finder.dirs[i]);
		Widget_Append(private_space_view.source_dirs, item);
		Dict_Add(self->dirpaths, finder.dirs[i]->path, item);
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

static void OnSelectDir(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	SelectFolderAsyncW(OnSelectDirDone);
}

static void OnSelectPrivateDir(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	SelectFolderAsyncW(OnSelectPrivateDirDone);
}

/** 渲染缩略图缓存占用空间文本 */
static void RenderThumbDBSizeText(wchar_t *buf, const wchar_t *text, void *data)
{
	wchar_t size_str[128];
	int64_t size = LCFinder_GetThumbDBTotalSize();

	wgetsizestr(size_str, 127, size);
	wcsncpy(buf, text, TXTFMT_BUF_MAX_LEN);
	wcsreplace(buf, TXTFMT_BUF_MAX_LEN, L"%s", size_str);
}

static void OnBtnSettingsClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	TextViewI18n_Refresh(this_view.thumb_db_stats);
}

static void OnThumbDBDelDone(void *data, void *arg)
{
	TextViewI18n_Refresh(this_view.thumb_db_stats);
}

/** 在“清除”按钮被点击时 */
static void OnBtnClearThumbDBClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LinkedListNode *node;
	LCUI_Widget text = NULL;

	if (w->disabled) {
		return;
	}
	for (LinkedList_Each(node, &w->children)) {
		LCUI_Widget child = node->data;
		if (Widget_HasClass(child, "text")) {
			text = child;
			break;
		}
	}
	if (!text) {
		return;
	}
	Widget_SetDisabled(w, TRUE);
	Widget_AddClass(w, "disabled");
	TextView_SetTextW(text, I18n_GetText(KEY_CLEANING));
	LCFinder_ClearThumbDB();
	Widget_SetDisabled(w, FALSE);
	Widget_RemoveClass(w, "disabled");
	TextView_SetTextW(text, I18n_GetText(KEY_CLEAR));
}

/** 在“许可协议”按钮被点击时 */
static void OnBtnLicenseClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	OpenUriW(L"http://www.gnu.org/licenses/gpl-2.0.html");
}

/** 在“官方网站”按钮被点击时 */
static void OnBtnWebSiteClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	OpenUriW(L"https://lc-soft.io/");
}

/** 在“问题反馈”按钮被点击时 */
static void OnBtnFeedbackClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	OpenUriW(L"https://github.com/lc-soft/LC-Finder/issues");
}

/** 在“源代码”按钮被点击时 */
static void OnBtnSourceCodeClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	OpenUriW(L"https://github.com/lc-soft/LC-Finder");
}

static void OnSelectLanguage(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	const char *code = Widget_GetAttribute(e->target, "value");
	const Language lang = I18n_SetLanguage(code);
	if (lang) {
		LCUIWidget_RefreshTextViewI18n();
		TextView_SetText(this_view.language, lang->name);
		strcpy(finder.config.language, lang->code);
		LCFinder_SaveConfig();
		LCFinder_TriggerEvent(EVENT_LANG_CHG, lang);
	}
}

static void UI_InitLanguages(void)
{
	int i, n;
	Language lang;
	Language *langs;
	LCUI_Widget menu;
	LCUI_Widget item;

	n = I18n_GetLanguages(&langs);
	SelectWidget(menu, ID_DROPDOWN_LANGUAGES);
	SelectWidget(this_view.language, ID_TXT_CURRENT_LANGUAGE);
	for (i = 0; i < n; ++i) {
		lang = langs[i];
		item = LCUIWidget_New("textview");
		Widget_AddClass(item, "dropdown-item");
		Widget_SetAttributeEx(item, "value", lang->code, 0, NULL);
		Widget_Append(menu, item);
		TextView_SetText(item, lang->name);
		if (strcmp(finder.config.language, lang->code) == 0) {
			TextView_SetText(this_view.language, lang->name);
		}
	}
	BindEvent(menu, "change.dropdown", OnSelectLanguage);
}

static LCUI_BOOL OnCheckPassword(const char *password, const char *data)
{
	char buf[48];
	const char *pwd = data;
	EncodeSHA1(buf, password, strlen(password));
	return strcmp(pwd, buf) == 0;
}

static void OnPrivateSpaceSwitchCahnge(LCUI_Widget w, LCUI_WidgetEvent e,
				       void *arg)
{
	int ret;
	const wchar_t *title, *text;
	char *pwd = finder.config.encrypted_password;
	LCUI_Widget window = LCUIWidget_GetById(ID_WINDOW_MAIN);
	if (!Switch_IsChecked(w)) {
		finder.open_private_space = FALSE;
		Widget_AddClass(private_space_view.view, "hide");
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
			Widget_AddClass(private_space_view.view, "hide");
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
			Widget_AddClass(private_space_view.view, "hide");
			return;
		}
		buf = EncodeUTF8(wbuf);
		LOGW(L"new password: %s\n", wbuf);
		EncodeSHA1(pwd, buf, strlen(buf));
		LCFinder_SaveConfig();
		free(buf);
	}
	if (!private_space_view.is_loaded) {
		UI_InitPrivateDirList();
		private_space_view.is_loaded = TRUE;
	}
	finder.open_private_space = TRUE;
	Widget_RemoveClass(private_space_view.view, "hide");
	LCFinder_TriggerEvent(EVENT_PRIVATE_SPACE_CHG, NULL);
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
	LOGW(L"new password: %s\n", wbuf);
	EncodeSHA1(pwd, buf, strlen(buf));
	LCFinder_SaveConfig();
	free(buf);
}

void UI_InitPrivateSpaceView(void)
{
	LCUI_Widget btn, btn_reset;
	struct PrivateSpaceViewData *self = &private_space_view;
	SelectWidget(self->source_dirs, ID_VIEW_PRIVATE_SOURCE_LIST);
	SelectWidget(self->view, ID_VIEW_PRIVATE_SPACE);
	SelectWidget(btn, ID_BTN_ADD_PRIVATE_SOURCE);
	SelectWidget(btn_reset, ID_BTN_RESET_PASSWORD);
	BindEvent(btn, "click", OnSelectPrivateDir);
	BindEvent(btn_reset, "click", OnBtnResetPasswordClick);
	self->is_loaded = FALSE;
}

static void CheckLicense(void)
{
	LCUI_Widget txt = LCUIWidget_GetById(ID_TXT_TRIAL_LICENSE);
	if (finder.license.is_active && !finder.license.is_trial) {
		Widget_Destroy(txt);
	}
}

static void OnLicenseChange(void *privdata, void *data)
{
	CheckLicense();
}

void UI_InitSettingsView(void)
{
	LCUI_Widget btn, switcher;
	SelectWidget(this_view.source_dirs, ID_VIEW_SOURCE_LIST);
	SelectWidget(this_view.thumb_db_stats, ID_TXT_THUMB_DB_SIZE);
	SelectWidget(switcher, ID_SWITCH_PRIVATE_SPACE);
	BindEvent(switcher, "change.switch", OnPrivateSpaceSwitchCahnge);
	SelectWidget(btn, ID_BTN_ADD_SOURCE);
	BindEvent(btn, "click", OnSelectDir);
	SelectWidget(btn, ID_BTN_SIDEBAR_SETTINGS);
	BindEvent(btn, "click", OnBtnSettingsClick);
	SelectWidget(btn, ID_BTN_CLEAR_THUMB_DB);
	BindEvent(btn, "click", OnBtnClearThumbDBClick);
	SelectWidget(btn, ID_BTN_OPEN_LICENSE);
	BindEvent(btn, "click", OnBtnLicenseClick);
	SelectWidget(btn, ID_BTN_OPEN_WEBSITE);
	BindEvent(btn, "click", OnBtnWebSiteClick);
	SelectWidget(btn, ID_BTN_OPEN_FEEDBACK);
	BindEvent(btn, "click", OnBtnFeedbackClick);
	SelectWidget(btn, ID_BTN_OPEN_SOURCECODE);
	BindEvent(btn, "click", OnBtnSourceCodeClick);
	LCFinder_BindEvent(EVENT_THUMBDB_DEL_DONE, OnThumbDBDelDone, NULL);
	TextViewI18n_SetFormater(this_view.thumb_db_stats,
				 RenderThumbDBSizeText, NULL);
	TextViewI18n_Refresh(this_view.thumb_db_stats);
	LCFinder_BindEvent(EVENT_DIR_ADD, OnAddDir, NULL);
	LCFinder_BindEvent(EVENT_DIR_DEL, OnDelDir, NULL);
	LCFinder_BindEvent(EVENT_LICENSE_CHG, OnLicenseChange, NULL);
	UI_InitPrivateSpaceView();
	UI_InitLanguages();
	UI_InitDirList();
	CheckLicense();
}
