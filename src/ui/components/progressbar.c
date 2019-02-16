/* ***************************************************************************
 * progressbar.c -- progressbar
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
 * progressbar.c -- 进度条
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

#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>

typedef struct ProgressRec_ {
	size_t value;
	size_t max_value;
	LCUI_Widget bar;
} ProgressRec, *Progress;

static LCUI_WidgetPrototype prototype = NULL;

static void Progress_OnInit(LCUI_Widget w)
{
	Progress data;
	const size_t data_size = sizeof(ProgressRec);

	data = Widget_AddData(w, prototype, data_size);
	data->bar = LCUIWidget_New("progressbar");
	data->max_value = 100;
	data->value = 0;
	Widget_Append(w, data->bar);
}

void ProgressBar_Update(LCUI_Widget w)
{
	Progress self = Widget_GetData(w, prototype);
	float n = (float)(1.0 * self->value / self->max_value);
	Widget_SetStyle(self->bar, key_width, n, scale);
	Widget_UpdateStyle(self->bar, TRUE);
}

void ProgressBar_SetValue(LCUI_Widget w, size_t value)
{
	Progress self = Widget_GetData(w, prototype);
	self->value = value;
	if (self->value > self->max_value) {
		self->value = self->max_value;
	}
	ProgressBar_Update(w);
}

void ProgressBar_SetMaxValue(LCUI_Widget w, size_t max_value)
{
	Progress self = Widget_GetData(w, prototype);
	if (max_value <= 0) {
		return;
	}
	self->max_value = max_value;
	if (self->value > self->max_value) {
		self->value = self->max_value;
	}
	ProgressBar_Update(w);
}

void LCUIWidget_AddProgressBar(void)
{
	prototype = LCUIWidget_NewPrototype("progress", NULL);
	prototype->init = Progress_OnInit;
}
