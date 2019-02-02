/* ***************************************************************************
 * labelbox.c -- label a bounding box and add name
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
 * labelbox.c -- 标签框，标记一个边界框并添加名称
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
#include <LCUI/timer.h>
#include <LCUI/input.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/widget/textedit.h>
#include "labelbox.h"
#include "common.h"

// clang-format off

#define BOX_MIN_WIDTH	32
#define BOX_MIN_HEIGHT	32
#define COLOR_TOTAL	12
#define MAX_NAME_LEN	256
#define DEFAULT_NAME	L"unknown"
#define LABEL_HEIGHT	24
#define COLOR_COUNT	12

typedef struct DraggingContextRec_ {
	LCUI_BOOL active;
	LCUI_Pos point;
	float x, y;
} DraggingContextRec, *DraggingContext;

typedef struct ResizingContextRec_ {
	LCUI_BOOL active;
	LCUI_Pos point;
	float width, height;
} ResizingContextRec, *ResizingContext;

typedef struct LabelBoxRec_ {
	wchar_t name[MAX_NAME_LEN];
	char classname[32];

	LCUI_Widget header;
	LCUI_Widget edit;
	LCUI_Widget label;
	LCUI_Widget box;
	LCUI_Widget resizer;
	LCUI_RectF rect;

	DraggingContextRec dragging;
	ResizingContextRec resizing;
} LabelBoxRec, *LabelBox;

static struct LabelBoxModule {
	LCUI_WidgetPrototype proto;
} self;

// clang-format on

static void Resizer_OnMouseDown(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LabelBox target = Widget_GetData(e->data, self.proto);
	ResizingContext ctx = &target->resizing;

	ctx->width = target->box->width;
	ctx->height = target->box->height;
	ctx->point.x = e->motion.x;
	ctx->point.y = e->motion.y;
	ctx->active = TRUE;
	e->cancel_bubble = TRUE;
	Widget_SetMouseCapture(w);
}

static void Resizer_OnMouseMove(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	float width, height;
	LabelBox target = Widget_GetData(e->data, self.proto);
	ResizingContext ctx = &target->resizing;

	if (!ctx->active) {
		return;
	}
	e->cancel_bubble = TRUE;
	width = ctx->width + e->motion.x - ctx->point.x;
	height = ctx->height + e->motion.y - ctx->point.y;
	target->rect.width = width;
	target->rect.height = height;
	Widget_Resize(target->box, width, height);
}

static void Resizer_OnMouseUp(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LCUI_WidgetEventRec change_event;
	LabelBox target = Widget_GetData(e->data, self.proto);

	e->cancel_bubble = TRUE;
	target->resizing.active = FALSE;
	LCUI_InitWidgetEvent(&change_event, "change");
	Widget_TriggerEvent(e->data, &change_event, NULL);
	Widget_ReleaseMouseCapture(w);
}

static LCUI_Widget Resizer_New(LCUI_Widget w)
{
	LCUI_Widget resizer;

	resizer = LCUIWidget_New("icon");
	Widget_SetAttribute(resizer, "name", "resize-bottom-right");
	Widget_BindEvent(resizer, "mousedown", Resizer_OnMouseDown, w, NULL);
	Widget_BindEvent(resizer, "mousemove", Resizer_OnMouseMove, w, NULL);
	Widget_BindEvent(resizer, "mouseup", Resizer_OnMouseUp, w, NULL);
	return resizer;
}

static void Label_OnClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LabelBox_Edit(e->data);
	e->cancel_bubble = TRUE;
}

static LCUI_Widget Label_New(LCUI_Widget w)
{
	LCUI_Widget label = LCUIWidget_New("textview");

	Widget_BindEvent(label, "click", Label_OnClick, w, NULL);
	return label;
}

static void LabelBox_OnMouseDown(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LabelBox that = Widget_GetData(w, self.proto);
	DraggingContext ctx = &that->dragging;

	e->cancel_bubble = TRUE;
	if (e->target != that->box) {
		return;
	}

	ctx->x = w->x;
	ctx->y = w->y;
	ctx->point.x = e->motion.x;
	ctx->point.y = e->motion.y;
	ctx->active = TRUE;
	Widget_SetMouseCapture(w);
}

static void LabelBox_OnMouseMove(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	float x, y;
	LabelBox that = Widget_GetData(w, self.proto);
	DraggingContext ctx = &that->dragging;

	if (!ctx->active) {
		return;
	}
	x = ctx->x + e->motion.x - ctx->point.x;
	y = ctx->y + e->motion.y - ctx->point.y;
	that->rect.x = x;
	that->rect.y = y + LABEL_HEIGHT;
	Widget_Move(w, x, y);
}

static void LabelBox_OnMouseUp(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LCUI_WidgetEventRec change_event;
	LabelBox that = Widget_GetData(w, self.proto);

	that->dragging.active = FALSE;
	LCUI_InitWidgetEvent(&change_event, "change");
	Widget_TriggerEvent(w, &change_event, NULL);
	Widget_ReleaseMouseCapture(w);
}

static void LabelBox_OnBlurEdit(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LabelBox_Save(e->data);
}

static void LabelBox_OnKeydown(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	if (e->key.code == LCUI_KEY_ENTER) {
		LabelBox_Save(e->data);
	}
}

static void LabelBox_OnFocusEdit(void *arg)
{
	LabelBox that = Widget_GetData(arg, self.proto);

	LCUIWidget_SetFocus(that->edit);
}

static void LabelBox_OnInit(LCUI_Widget w)
{
	LabelBox that = Widget_AddData(w, self.proto, sizeof(LabelBoxRec));

	that->classname[0] = 0;
	that->header = LCUIWidget_New(NULL);
	that->edit = LCUIWidget_New("textedit");
	that->label = Label_New(w);
	that->resizer = Resizer_New(w);
	that->box = LCUIWidget_New(NULL);
	that->rect.x = that->rect.y = 0;
	that->rect.width = BOX_MIN_WIDTH;
	that->rect.height = BOX_MIN_HEIGHT;
	that->dragging.active = FALSE;
	that->resizing.active = FALSE;

	Widget_AddClass(that->box, "labelbox-box");
	Widget_AddClass(that->header, "labelbox-header");
	Widget_AddClass(that->resizer, "labelbox-resizer");
	Widget_Append(that->box, that->resizer);
	Widget_Append(that->header, that->label);
	Widget_Append(w, that->header);
	Widget_Append(w, that->edit);
	Widget_Append(w, that->box);

	Widget_BindEvent(that->edit, "blur", LabelBox_OnBlurEdit, w, NULL);
	Widget_BindEvent(that->edit, "keydown", LabelBox_OnKeydown, w, NULL);
	Widget_BindEvent(w, "mousedown", LabelBox_OnMouseDown, NULL, NULL);
	Widget_BindEvent(w, "mousemove", LabelBox_OnMouseMove, NULL, NULL);
	Widget_BindEvent(w, "mouseup", LabelBox_OnMouseUp, NULL, NULL);

	LabelBox_SetNameW(w, DEFAULT_NAME);
}

static void LabelBox_UpdateColor(LCUI_Widget w)
{
	LabelBox that = Widget_GetData(w, self.proto);

	if (that->classname[0]) {
		Widget_RemoveClass(w, that->classname);
	}
	snprintf(that->classname, 32, "labelbox-color-%d",
		 get_wcs_sum(that->name) % COLOR_COUNT + 1);
	Widget_AddClass(w, that->classname);
}

void LabelBox_SetNameW(LCUI_Widget w, const wchar_t *label)
{
	LabelBox that = Widget_GetData(w, self.proto);

	that->name[255] = 0;
	wcsncpy(that->name, label, 255);
	TextView_SetTextW(that->label, label);
	TextEdit_SetTextW(that->edit, label);
	LabelBox_UpdateColor(w);
}

const wchar_t *LabelBox_GetNameW(LCUI_Widget w)
{
	LabelBox that = Widget_GetData(w, self.proto);

	return that->name;
}

void LabelBox_GetRect(LCUI_Widget w, LCUI_RectF *rect)
{
	LabelBox that = Widget_GetData(w, self.proto);

	*rect = that->rect;
}

void LabelBox_SetRect(LCUI_Widget w, const LCUI_RectF *rect)
{
	LabelBox that = Widget_GetData(w, self.proto);

	that->rect = *rect;
	Widget_Move(w, that->rect.x, rect->y - LABEL_HEIGHT);
	Widget_Resize(that->box, that->rect.width, that->rect.height);
}

void LabelBox_Edit(LCUI_Widget w)
{
	LabelBox that = Widget_GetData(w, self.proto);

	TextEdit_SetTextW(that->edit, that->name);
	Widget_AddClass(w, "editing");
	LCUI_SetTimeout(100, LabelBox_OnFocusEdit, w);
}

void LabelBox_Save(LCUI_Widget w)
{
	size_t len;
	LCUI_WidgetEventRec e;
	wchar_t name[MAX_NAME_LEN];
	LabelBox that = Widget_GetData(w, self.proto);

	wcscpy(name, that->name);
	len = TextEdit_GetTextW(that->edit, 0, MAX_NAME_LEN, that->name);
	if (len > 0) {
		that->name[len] = 0;
	} else {
		wcscpy(that->name, L"unknown");
	}
	TextView_SetTextW(that->label, that->name);
	LabelBox_UpdateColor(w);
	Widget_RemoveClass(w, "editing");
	LCUI_InitWidgetEvent(&e, "change");
	Widget_TriggerEvent(w, &e, name);
}

void LCUIWidget_AddLabelBox(void)
{
	self.proto = LCUIWidget_NewPrototype("labelbox", NULL);
	self.proto->init = LabelBox_OnInit;
}
