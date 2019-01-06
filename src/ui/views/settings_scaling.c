/* ***************************************************************************
 * settings_scaling.c -- scaling setting view
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
 * settings_scaling.c -- “设置”视图中的缩放设置项
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

#include <stdio.h>
#include <string.h>
#include "finder.h"
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "ui.h"
#include "i18n.h"
#include "textview_i18n.h"
#include "settings.h"

static void RefreshScalingText(void)
{
	char str[32];
	LCUI_Widget txt;

	SelectWidget(txt, ID_TXT_CURRENT_SCALING);
	sprintf(str, "%d%%", finder.config.scaling);
	TextView_SetText(txt, str);
}

static void OnChangeScaling(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	int scaling;
	const char *value = Widget_GetAttribute(e->target, "value");

	if (!value || sscanf(value, "%d", &scaling) < 1) {
		return;
	}
	if (scaling >= 100 && scaling <= 200) {
		finder.config.scaling = scaling;
		LCUIMetrics_SetScale((float)(scaling / 100.0));
		LCUIWidget_RefreshStyle();
		LCFinder_SaveConfig();
		RefreshScalingText();
	}
}

void SettingsView_InitScaling(void)
{
	LCUI_Widget menu;

	SelectWidget(menu, ID_DROPDOWN_SCALING);
	BindEvent(menu, "change.dropdown", OnChangeScaling);
	RefreshScalingText();
}
