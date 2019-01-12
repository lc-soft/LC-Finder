/* ***************************************************************************
 * taskitem.c -- task item
 *
 * Copyright (C) 2018-2019 by Liu Chao <lc-soft@live.cn>
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
 * taskitem.c -- 任务项，用于展示任务的进度信息
 *
 * 版权所有 (C) 2018-2019 归属于 刘超 <lc-soft@live.cn>
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
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "textview_i18n.h"
#include "progressbar.h"
#include "taskitem.h"
#include "common.h"
#include "i18n.h"

#define KEY_TXT_COMPLETED "taskitem.completed"
#define KEY_TXT_PREPARING "taskitem.preparing"

typedef struct TaskItemRec_ {
	int timer;
	size_t total;
	size_t current;
	unsigned start_time;
	LCUI_BOOL active;

	LCUI_Widget icon;
	LCUI_Widget name;
	LCUI_Widget text;
	LCUI_Widget content;
	LCUI_Widget action;
	LCUI_Widget action_text;
	LCUI_Widget progress;
	LCUI_Widget progress_text;
} TaskItemRec, *TaskItem;

static struct TaskItemModule {
	LCUI_WidgetPrototype proto;
} self;

static void TaskItem_OnClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LCUI_WidgetEventRec ev;
	TaskItem that = Widget_GetData(e->data, self.proto);

	if (that->active) {
		TaskItem_StopTask(e->data);
		LCUI_InitWidgetEvent(&ev, "stop");
	} else {
		TaskItem_StartTask(e->data);
		LCUI_InitWidgetEvent(&ev, "start");
	}
	Widget_TriggerEvent(w, &ev, NULL);
}

static void TaskItem_OnInit(LCUI_Widget w)
{
	TaskItem that = Widget_AddData(w, self.proto, sizeof(TaskItemRec));

	that->timer = 0;
	that->total = 0;
	that->current = 0;
	that->active = FALSE;
	that->content = LCUIWidget_New(NULL);
	that->icon = LCUIWidget_New("icon");
	that->name = LCUIWidget_New("textview-i18n");
	that->text = LCUIWidget_New("textview-i18n");
	that->action = LCUIWidget_New(NULL);
	that->action_text = LCUIWidget_New("textview-i18n");
	that->progress = LCUIWidget_New("progress");
	that->progress_text = LCUIWidget_New("textview");

	TextViewI18n_SetKey(that->action_text, "button.start");
	Widget_AddClass(w, "taskitem");
	Widget_AddClass(that->content, "taskitem-content");
	Widget_AddClass(that->icon, "taskitem-icon");
	Widget_AddClass(that->name, "taskitem-name");
	Widget_AddClass(that->text, "taskitem-text");
	Widget_AddClass(that->action_text, "text");
	Widget_AddClass(that->action, "btn taskitem-action");
	Widget_AddClass(that->progress, "taskitem-progress");
	Widget_AddClass(that->progress_text, "taskitem-progress-text");
	Widget_BindEvent(that->action, "click", TaskItem_OnClick, w, NULL);
	Widget_Append(that->action, that->action_text);
	Widget_Append(that->content, that->name);
	Widget_Append(that->content, that->text);
	Widget_Append(that->content, that->progress);
	Widget_Append(that->content, that->progress_text);
	Widget_Append(w, that->icon);
	Widget_Append(w, that->content);
	Widget_Append(w, that->action);
}

static void TaskItem_OnTimer(void *arg)
{
	unsigned seconds;
	unsigned seconds_left;
	const wchar_t *message;
	wchar_t time_str[64];
	wchar_t text[128];
	LCUI_Widget w = arg;
	TaskItem t = Widget_GetData(w, self.proto);

	ProgressBar_SetMaxValue(t->progress, (int)t->total);
	ProgressBar_SetValue(t->progress, (int)t->current);

	if (t->total == 0 || t->current == 0) {
		t->start_time = (unsigned)(LCUI_GetTime() / 1000);
		message = I18n_GetText(KEY_TXT_PREPARING);
		TextView_SetTextW(t->progress_text, message);
		return;
	}
	if (t->total > 0 && t->current >= t->total) {
		message = I18n_GetText(KEY_TXT_COMPLETED);
		swprintf(text, 128, message, t->total);
		TextView_SetTextW(t->progress_text, text);
		TextViewI18n_SetKey(t->action_text, "button.restart");
		Widget_AddClass(w, "completed");
		Widget_RemoveClass(w, "active");
		LCUITimer_Free(t->timer);
		t->active = FALSE;
		return;
	}
	if (t->start_time < 1) {
		t->start_time = (unsigned)(LCUI_GetTime() / 1000);
	}
	seconds = (unsigned)(LCUI_GetTime() / 1000) - t->start_time;
	if (seconds > 0 && t->current > 0) {
		seconds_left = (unsigned)((t->total - t->current) * seconds / t->current);
	} else {
		seconds_left = 0;
	}
	get_human_time_left_wcs(time_str, 32, seconds_left);
	swprintf(text, 128, L"%zu/%zu - %ls", t->current, t->total, time_str);
	TextView_SetTextW(t->progress_text, text);
}

void TaskItem_SetIcon(LCUI_Widget w, const char *icon)
{
	TaskItem that = Widget_GetData(w, self.proto);

	Widget_SetAttribute(that->icon, "name", icon);
}

void TaskItem_SetNameKey(LCUI_Widget w, const char *key)
{
	TaskItem that = Widget_GetData(w, self.proto);

	TextViewI18n_SetKey(that->name, key);
}

void TaskItem_SetTextKey(LCUI_Widget w, const char *key)
{
	TaskItem that = Widget_GetData(w, self.proto);

	TextViewI18n_SetKey(that->text, key);
}

void TaskItem_SetActionDisabled(LCUI_Widget w, LCUI_BOOL disabled)
{
	if (disabled) {
		Widget_AddClass(w, "disabled");
	} else {
		Widget_RemoveClass(w, "disabled");
	}
}

int TaskItem_StartTask(LCUI_Widget w)
{
	TaskItem that = Widget_GetData(w, self.proto);

	if (that->active) {
		return -1;
	}
	that->total = 0;
	that->current = 0;
	that->active = TRUE;
	that->start_time = 0;
	that->timer = LCUI_SetInterval(1000, TaskItem_OnTimer, w);
	TextViewI18n_SetKey(that->action_text, "button.stop");
	Widget_AddClass(w, "active");
	Widget_RemoveClass(w, "error");
	Widget_RemoveClass(w, "completed");
	TaskItem_OnTimer(w);
	return 0;
}

int TaskItem_StopTask(LCUI_Widget w)
{
	TaskItem that = Widget_GetData(w, self.proto);

	if (!that->active) {
		return -1;
	}
	that->active = FALSE;
	if (that->timer) {
		LCUITimer_Free(that->timer);
		that->timer = 0;
	}
	TextViewI18n_SetKey(that->action_text, "button.start");
	Widget_RemoveClass(w, "active");
	return 0;
}

void TaskItem_SetError(LCUI_Widget w, const wchar_t *message)
{
	TaskItem that = Widget_GetData(w, self.proto);

	that->active = FALSE;
	if (that->timer) {
		LCUITimer_Free(that->timer);
		that->timer = 0;
	}
	Widget_AddClass(w, "error");
	Widget_RemoveClass(w, "active");
	TextView_SetTextW(that->progress_text, message);
	TextViewI18n_SetKey(that->action_text, "button.start");
}

void TaskItem_SetProgress(LCUI_Widget w, size_t current, size_t total)
{
	TaskItem that = Widget_GetData(w, self.proto);

	that->current = current;
	that->total = total;
}

void LCUIWidget_AddTaskItem(void)
{
	self.proto = LCUIWidget_NewPrototype("taskitem", NULL);
	self.proto->init = TaskItem_OnInit;
}
