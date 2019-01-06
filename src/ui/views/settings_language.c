/* ***************************************************************************
 * settings_language.c -- language setting view
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
 * settings_language.c -- “设置”视图中的语言设置项
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

static struct LanguageSettingsView {
	LCUI_Widget language;
} view;

static void OnSelectLanguage(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	const char *code = Widget_GetAttribute(e->target, "value");
	const Language lang = I18n_SetLanguage(code);
	if (lang) {
		LCUIWidget_RefreshTextViewI18n();
		TextView_SetText(view.language, lang->name);
		strcpy(finder.config.language, lang->code);
		LCFinder_SaveConfig();
		LCFinder_TriggerEvent(EVENT_LANG_CHG, lang);
	}
}

void SettingsView_InitLanguage(void)
{

	int i, n;
	Language lang;
	Language *langs;
	LCUI_Widget menu;
	LCUI_Widget item;

	n = I18n_GetLanguages(&langs);
	SelectWidget(menu, ID_DROPDOWN_LANGUAGES);
	SelectWidget(view.language, ID_TXT_CURRENT_LANGUAGE);
	for (i = 0; i < n; ++i) {
		lang = langs[i];
		item = LCUIWidget_New("textview");
		Widget_AddClass(item, "dropdown-item");
		Widget_SetAttributeEx(item, "value", lang->code, 0, NULL);
		Widget_Append(menu, item);
		TextView_SetText(item, lang->name);
		if (strcmp(finder.config.language, lang->code) == 0) {
			TextView_SetText(view.language, lang->name);
		}
	}
	BindEvent(menu, "change.dropdown", OnSelectLanguage);
}
