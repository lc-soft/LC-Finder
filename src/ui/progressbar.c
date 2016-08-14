/* ***************************************************************************
 * progressbar.c -- progressbar
 *
 * Copyright (C) 2016 by Liu Chao <lc-soft@live.cn>
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
 * 版权所有 (C) 2016 归属于 刘超 <lc-soft@live.cn>
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

#include <LCUI_Build.h>
#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>

typedef struct ProgressRec_ {
	int value;
	int max_value;
	LCUI_Widget bar;
} ProgressRec, *Progress;

static void Progress_OnInit( LCUI_Widget w )
{
	Progress self;
	self = Widget_NewPrivateData( w, ProgressRec );
	self->bar = LCUIWidget_New( "progressbar" );
	self->max_value = 100;
	self->value = 0;
	Widget_Append( w, self->bar );
}

static void Progress_OnDestroy( LCUI_Widget w )
{

}

void LCUIWidget_AddProgressBar( void )
{
	LCUI_WidgetClass *wc = LCUIWidget_AddClass( "progress" );
	wc->methods.init = Progress_OnInit;
	wc->methods.destroy = Progress_OnDestroy;
}

void ProgressBar_Update( LCUI_Widget w )
{
	float n;
	Progress self = w->private_data;
	n = (float)(1.0 * self->value / self->max_value);
	SetStyle( self->bar->custom_style, key_width, n, scale );
	Widget_UpdateStyle( self->bar, FALSE );
}

void ProgressBar_SetValue( LCUI_Widget w, int value )
{
	Progress self = w->private_data;
	self->value = value;
	if( self->value > self->max_value ) {
		self->value = self->max_value;
	}
	ProgressBar_Update( w );
}

void ProgressBar_SetMaxValue( LCUI_Widget w, int max_value )
{
	Progress self = w->private_data;
	if( max_value <= 0 ) {
		return;
	}
	self->max_value = max_value;
	if( self->value > self->max_value ) {
		self->value = self->max_value;
	}
	ProgressBar_Update( w );
}
