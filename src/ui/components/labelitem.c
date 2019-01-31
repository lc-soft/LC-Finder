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
#include "common.h"

#define COLOR_COUNT 12

typedef struct LabelItemRec_ {
	wchar_t name[256];
	char classname[32];
	LCUI_Widget txt_name;
	LCUI_Widget txt_info;
} LabelItemRec, *LabelItem;

static struct LabelItemModule {
	LCUI_WidgetPrototype proto;
} self;

static void LabelItem_OnRemove(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LCUI_WidgetEventRec ev;

	LCUI_InitWidgetEvent(&ev, "remove");
	Widget_TriggerEvent(e->data, &ev, NULL);
}

static void LabelItem_OnInit(LCUI_Widget w)
{
	LCUI_Widget marker, btn;
	LabelItem that = Widget_AddData(w, self.proto, sizeof(LabelItemRec));

	btn = LCUIWidget_New("icon");
	marker = LCUIWidget_New(NULL);
	that->txt_name = LCUIWidget_New("textview");
	that->txt_info = LCUIWidget_New("textview");
	Widget_SetAttribute(btn, "name", "trash-can-outline");
	Widget_AddClass(that->txt_name, "labelitem-name");
	Widget_AddClass(that->txt_info, "labelitem-info");
	Widget_AddClass(marker, "labelitem-marker");
	Widget_AddClass(btn, "labelitem-remove");
	Widget_BindEvent(btn, "click", LabelItem_OnRemove, w, NULL);
	Widget_Append(w, marker);
	Widget_Append(w, btn);
	Widget_Append(w, that->txt_name);
	Widget_Append(w, that->txt_info);
}

void LabelItem_SetNameW(LCUI_Widget w, const wchar_t *name)
{
	LabelItem that = Widget_GetData(w, self.proto);

	that->name[255] = 0;
	wcsncpy(that->name, name, 255);
	TextView_SetTextW(that->txt_name, name);
	if (that->classname[0]) {
		Widget_RemoveClass(w, that->classname);
	}
	snprintf(that->classname, 32, "labelitem-color-%d",
		 get_wcs_sum(that->name) % COLOR_COUNT + 1);
	Widget_AddClass(w, that->classname);
}

void LabelItem_SetRect(LCUI_Widget w, LCUI_Rect *rect)
{
	wchar_t str[256] = { 0 };
	LabelItem that = Widget_GetData(w, self.proto);

	swprintf(str, 255, L"(%d,%d,%d,%d)",
		 rect->x, rect->y, rect->width, rect->height);
	TextView_SetTextW(that->txt_info, str);
}

void LCUIWidget_AddLabelItem(void)
{
	self.proto = LCUIWidget_NewPrototype("labelitem", NULL);
	self.proto->init = LabelItem_OnInit;
}
