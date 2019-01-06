/* ***************************************************************************
 * settings_thumb_cache.c -- thumbnail cache setting view
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
 * settings_thumb_cache.c -- “设置”视图中的缩略图缓存设置项
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
#include "textview_i18n.h"
#include "settings.h"

#define KEY_CLEANING 	"button.cleaning"
#define KEY_CLEAR	"button.clear" 

static struct ThumbCacheSettingView {
	LCUI_Widget thumb_db_stats;
} view;

static void OnBtnSettingsClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	TextViewI18n_Refresh(view.thumb_db_stats);
}

static void OnThumbDBDelDone(void *data, void *arg)
{
	TextViewI18n_Refresh(view.thumb_db_stats);
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

void SettingsView_InitThumbCache(void)
{
	LCUI_Widget btn;
	SelectWidget(btn, ID_BTN_CLEAR_THUMB_DB);
	SelectWidget(view.thumb_db_stats, ID_TXT_THUMB_DB_SIZE);
	BindEvent(btn, "click", OnBtnClearThumbDBClick);
	SelectWidget(btn, ID_BTN_SIDEBAR_SETTINGS);
	BindEvent(btn, "click", OnBtnSettingsClick);
	LCFinder_BindEvent(EVENT_THUMBDB_DEL_DONE, OnThumbDBDelDone, NULL);
	TextViewI18n_SetFormater(view.thumb_db_stats,
				 RenderThumbDBSizeText, NULL);
	TextViewI18n_Refresh(view.thumb_db_stats);
}
