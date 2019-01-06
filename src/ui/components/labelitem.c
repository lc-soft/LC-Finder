/* ***************************************************************************
 * labelitem.c -- label item for view labeled box info and name
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
 * labelitem.c -- 标签项，用于展示标记的区域和名称
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
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "labelitem.h"

typedef struct LabelItemRec_ {
	LCUI_Widget name;
	LCUI_Widget info;
} LabelItemRec, *LabelItem;

static struct LabelItemModule {
	LCUI_WidgetPrototype proto;
} self;

static void LabelItem_OnInit(LCUI_Widget w)
{
	LabelItem that = Widget_AddData(w, self.proto, sizeof(LabelItemRec));

	that->name = LCUIWidget_New("textview");
	that->info = LCUIWidget_New("textview");

	Widget_AddClass(that->name, "labelitem-name");
	Widget_AddClass(that->info, "labelitem-info");
	Widget_Append(w, that->name);
	Widget_Append(w, that->info);
}

void LabelItem_SetNameW(LCUI_Widget w, const wchar_t *name)
{
	LabelItem that = Widget_GetData(w, self.proto);

	TextView_SetTextW(that->name, name);
}

void LabelItem_SetRect(LCUI_Widget w, LCUI_Rect *rect)
{
	wchar_t str[256] = { 0 };
	LabelItem that = Widget_GetData(w, self.proto);

	swprintf(str, 255, L"(%d,%d,%d,%d)",
		 rect->x, rect->y, rect->width, rect->height);
	TextView_SetTextW(that->info, str);
}

void LCUIWidget_AddLabelItem(void)
{
	self.proto = LCUIWidget_NewPrototype("labelitem", NULL);
	self.proto->init = LabelItem_OnInit;
}
