/* ***************************************************************************
 * switch.c --switch widget
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
 * switch.c -- 开关部件
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

#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/gui/widget.h>
#include "textview_i18n.h"
#include "switch.h"

#define KEY_OFF	"switch.off"
#define KEY_ON	"switch.on"

typedef struct SwitchRec_ {
	LCUI_Widget txt_off;
	LCUI_Widget txt_on;
	LCUI_Widget slider;
	LCUI_Widget bar;
	LCUI_BOOL checked;
} SwitchRec, *Switch;

static struct SwitchModule {
	LCUI_WidgetPrototype prototype;
	int event_change;
} self;

static const char *switch_css = CodeToString(

.switch {
	height: 20px;
	width: 44px;
	display: inline-block;
	border: 2px solid #c8d3d9;
	background-color: #c8d3d9;
}
.switch .switch-bar {
	left: 24px;
	height: 100%;
	width: 68px;
	position: relative;
}
.switch .switch-slider {
	width: 20px;
	height: 100%;
	background-color: #8c9da5;
}
.switch.checked {
	border: 2px solid #80dea0;
	background-color: #80dea0;
}
.switch.checked .switch-slider {
	background-color: #44BB55;
}
.switch.checked .switch-bar {
	left: 0;
}
.switch .switch-on-block,
.switch .switch-off-block {
	width: 24px;
	color: #fff;
	font-size: 16px;
	text-align: center;
}
.switch .switch-on-block,
.switch .switch-off-block,
.switch .switch-slider {
	display: inline-block;
}
.switch-text {
	margin-left: 10px;
	line-height: 24px;
}

);

static void Switch_OnClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	LCUI_WidgetEventRec ev = {0};
	Switch data = Widget_GetData( w, self.prototype );
	Switch_SetChecked( w, !data->checked );
	ev.cancel_bubble = TRUE;
	ev.type = self.event_change;
	Widget_TriggerEvent( w, &ev, NULL );
}

static void Switch_OnInit( LCUI_Widget w )
{
	const size_t data_size = sizeof( SwitchRec );
	Switch data = Widget_AddData( w, self.prototype, data_size );

	data->checked = FALSE;
	data->bar = LCUIWidget_New( NULL );
	data->slider = LCUIWidget_New( NULL );
	data->txt_off = LCUIWidget_New( "textview" );
	data->txt_on = LCUIWidget_New( "textview" );
	Widget_AddClass( w, "switch" );
	Widget_AddClass( data->bar, "switch-bar" );
	Widget_AddClass( data->slider, "switch-slider" );
	Widget_AddClass( data->txt_on, "icon icon-check switch-on-block" );
	Widget_AddClass( data->txt_off, "icon icon-close switch-off-block" );
	Widget_Append( data->bar, data->txt_on );
	Widget_Append( data->bar, data->slider );
	Widget_Append( data->bar, data->txt_off );
	Widget_Append( w, data->bar );
	Widget_BindEvent( w, "click", Switch_OnClick, NULL, NULL );
}

LCUI_BOOL Switch_IsChecked( LCUI_Widget w )
{
	Switch data = Widget_GetData( w, self.prototype );
	return data->checked;
}

void Switch_SetChecked( LCUI_Widget w, LCUI_BOOL checked )
{
	LCUI_Widget textview;
	Switch data = Widget_GetData( w, self.prototype );
	data->checked = checked;
	textview = Widget_GetNext( w );
	if( Widget_CheckType( textview, "textview-i18n" ) ) {
		const char *value;
		value = Widget_GetAttribute( textview, "data-bind" );
		if( !value || strcmp( value, "switch" ) != 0 ) {
			textview = NULL;
		}
	} else {
		textview = NULL;
	}
	if( data->checked ) {
		Widget_AddClass( w, "checked" );
		if( textview ) {
			TextViewI18n_SetKey( textview, KEY_ON );
		}
	} else {
		Widget_RemoveClass( w, "checked" );
		if( textview ) {
			TextViewI18n_SetKey( textview, KEY_OFF );
		}
	}
}

void LCUIWidget_AddSwitch( void )
{
	self.prototype = LCUIWidget_NewPrototype( "switch", NULL );
	self.prototype->init = Switch_OnInit;
	self.event_change = LCUIWidget_AllocEventId();
	LCUIWidget_SetEventName( self.event_change, "change.switch" );
	LCUI_LoadCSSString( switch_css, NULL );
}
