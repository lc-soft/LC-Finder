/* ***************************************************************************
 * dropdown.c -- contextual menu for displaying options of target
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
 * dropdown.c -- 上下文菜单，用于显示目标的选项列表
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
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "dropdown.h"

typedef struct DropdownRec_ {
	LCUI_Widget target;
	LCUI_Widget header;
	int position;
} DropdownRec, *Dropdown;

typedef struct DropdownItemRec_ {
	void *data;
} DropdownItemRec, *DropdownItem;

static struct DropdwonModule {
	LCUI_WidgetPrototype dropdown;
	LCUI_WidgetPrototype item;
	LCUI_WidgetPrototype header;
	int change_event;
} self;

static char *dropdown_css = ToString(

dropdown {
	position: absolute;
	margin-top: 4px;
	padding: 10px 0;
	min-width: 140px;
	background-color: #fff;
	border: 1px solid #ccc;
	box-shadow: 0 6px 12px rgba(0,0,0,.175);
	z-index: 1000;
}
dropdown dropdown-item {
	width: 100%;
	padding: 5px 10px;
	line-height: 24px;
	display: block;
}
dropdown dropdown-header {
	color: #aaa;
	width: 100%;
	padding: 0 10px 5px 10px;
	line-height: 24px;
	display: block;
	margin-bottom: 10px;
	border-bottom: 1px solid #eee;
}
dropdown dropdown-item:hover {
	background-color: #eee;
}
dropdown dropdown-item:active {
	background-color: #ddd;
}

);

static void Dropdown_OnClickOther( LCUI_Widget w, 
				   LCUI_WidgetEvent e, void *arg )
{
	Dropdown_Hide( e->data );
}

static void Dropdown_Init( LCUI_Widget w )
{
	const size_t data_size = sizeof( DropdownRec );
	Dropdown data = Widget_AddData( w, self.dropdown, data_size );
	data->header = data->target = NULL;
	data->position = SV_BOTTOM_LEFT;
	Widget_Hide( w );
	Widget_BindEvent( LCUIWidget_GetRoot(), "click", 
			  Dropdown_OnClickOther, w, NULL );
}

static void DropdownItem_Init( LCUI_Widget w )
{
	const size_t data_size = sizeof( DropdownItemRec );
	DropdownItem item = Widget_AddData( w, self.item, data_size );
	item->data = NULL;
	self.item->proto->init( w );
}

static void DropdownItem_Destrtoy( LCUI_Widget w )
{
	DropdownItem item = Widget_GetData( w, self.item );
	item->data = NULL;
}

static void DropdownHeader_Destrtoy( LCUI_Widget w )
{
	Dropdown menu = Widget_GetData( w->parent, self.dropdown );
	if( menu->header == w ) {
		menu->header = NULL;
	}
}

void Dropdown_UpdatePosition( LCUI_Widget w )
{
	int x, y;
	Dropdown data = Widget_GetData( w, self.dropdown );
	if( !data->target ) {
		Widget_Move( w, 0, 0 );
		return;
	}
	Widget_GetAbsXY( data->target, w->parent, &x, &y );
	switch( data->position ) {
	case SV_TOP_LEFT:
		y -= data->target->height + w->height;
		break;
	case SV_TOP_RIGHT:
		y -= data->target->height + w->height;
		x += data->target->width - w->width;
		break;
	case SV_BOTTOM_RIGHT:
		y += data->target->height;
		x += data->target->width - w->width;
		break;
	case SV_BOTTOM_LEFT:
	default:
		y += data->target->height;
		break;
	}
	switch( data->position ) {
	case SV_TOP_LEFT:
	case SV_TOP_RIGHT:
		if( y < 0 ) {
			Widget_GetAbsXY( data->target, w->parent, &x, &y );
			y += data->target->height;
		}
		break;
	case SV_BOTTOM_LEFT:
	case SV_BOTTOM_RIGHT:
	default: 
		if( y + w->height > w->parent->height ) {
			Widget_GetAbsXY( data->target, w->parent, &x, &y );
			y -= data->target->height + w->height;
		}
		break;
	}
	switch( data->position ) {
	case SV_BOTTOM_RIGHT:
	case SV_TOP_RIGHT:
		if( x < 0 ) {
			Widget_GetAbsXY( data->target, w->parent, &x, &y );
			y += data->target->height;
		}
		break;
	case SV_TOP_LEFT:
	case SV_BOTTOM_LEFT:
	default: 
		if( x + w->width > w->parent->width ) {
			Widget_GetAbsXY( data->target, w->parent, &x, &y );
			x += data->target->width - w->width;
		}
		break;
	}
	Widget_Move( w, x, y );
}

void Dropdown_Show( LCUI_Widget w )
{
	Dropdown data = Widget_GetData( w, self.dropdown );
	if( data->target ) {
		Widget_AddClass( data->target, "active" );
	}
	Dropdown_UpdatePosition( w );
	Widget_Show( w );
}

void Dropdown_Hide( LCUI_Widget w )
{
	Dropdown data = Widget_GetData( w, self.dropdown );
	if( data->target ) {
		Widget_RemoveClass( data->target, "active" );
	}
	Dropdown_UpdatePosition( w );
	Widget_Hide( w );
}

void Dropdown_Toggle( LCUI_Widget w )
{
	if( w->computed_style.visible ) {
		Dropdown_Hide( w );
	} else {
		Dropdown_Show( w );
	}
}

void DropdownItem_SetData( LCUI_Widget w, void *data )
{
	DropdownItem item = Widget_GetData( w, self.item );
	item->data = data;
}

void DropdownItem_SetText( LCUI_Widget w, const char *text )
{
	TextView_SetText( w, text );
}

void DropdownItem_SetTextW( LCUI_Widget w, const wchar_t *text )
{
	TextView_SetTextW( w, text );
}

static void DropdownItem_OnClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	Dropdown menu;
	DropdownItem item;
	LCUI_Widget parent = e->data;
	LCUI_WidgetEventRec ev = {0};
	menu = Widget_GetData( w, self.dropdown );
	if( w == menu->header ) {
		return;
	}
	e->cancel_bubble = TRUE;
	ev.type = self.change_event;
	item = Widget_GetData( w, self.item );
	Widget_TriggerEvent( parent, &ev, item->data );
	Dropdown_Hide( parent );
}

static void Dropdown_OnClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( w->disabled ) {
		return;
	}
	Dropdown_Toggle( e->data );
	e->cancel_bubble = TRUE;
}

void Dropdown_BindTarget( LCUI_Widget w, LCUI_Widget target )
{
	Dropdown data = Widget_GetData( w, self.dropdown );
	data->target = target;
	if( data->target ) {
		Widget_UnbindEvent( target, "click", Dropdown_OnClick );
	}
	Widget_BindEvent( target, "click", Dropdown_OnClick, w, NULL );
	data->target = target;
}

static void Dropdown_SetAttr( LCUI_Widget w, const char *name,
			      const char *value )
{
	if( strcasecmp( name, "data-target" ) == 0 ) {
		LCUI_Widget target = LCUIWidget_GetById( value );
		if( target ) {
			Dropdown_BindTarget( w, target );
		}
		return;
	}
	if( strcasecmp( name, "data-position" ) == 0 ) {
		Dropdown menu = Widget_GetData( w, self.dropdown );
		menu->position = LCUI_GetStyleValue( value );
		if( menu->position <= 0 ) {
			menu->position = SV_BOTTOM_LEFT;
		}
	}
}

void Dropdown_SetHeader( LCUI_Widget w, const char *header )
{
	Dropdown data = Widget_GetData( w, self.dropdown );
	if( !data->header ) {
		data->header = LCUIWidget_New( "dropdown-header" );
		Widget_Prepend( w, data->header );
	}
	DropdownItem_SetText( data->header, header );
}

void Dropdown_SetHeaderW( LCUI_Widget w, const wchar_t *header )
{
	Dropdown data = Widget_GetData( w, self.dropdown );
	if( !data->header ) {
		data->header = LCUIWidget_New( "dropdown-header" );
		Widget_Prepend( w, data->header );
	}
	DropdownItem_SetTextW( data->header, header );
}

LCUI_Widget Dropdwon_AddItem( LCUI_Widget w, void *data, const char *text )
{
	LCUI_Widget item = LCUIWidget_New( "dropdown-item" );
	DropdownItem_SetData( item, data );
	DropdownItem_SetText( item, text );
	Widget_BindEvent( item, "click", DropdownItem_OnClick, NULL, NULL );
	Widget_Append( w, item );
	return item;
}

LCUI_Widget Dropdwon_AddItemW( LCUI_Widget w, void *data, const wchar_t *text )
{
	LCUI_Widget item = LCUIWidget_New( "dropdown-item" );
	DropdownItem_SetData( item, data );
	DropdownItem_SetTextW( item, text );
	Widget_BindEvent( item, "click", DropdownItem_OnClick, w, NULL );
	Widget_Append( w, item );
	return item;
}

void Dropdown_SetPosition( LCUI_Widget w, int position )
{
	Dropdown data = Widget_GetData( w, self.dropdown );
	data->position = position;
	Dropdown_UpdatePosition( w );
}

void LCUIWidget_AddDropdown( void )
{
	self.dropdown = LCUIWidget_NewPrototype( "dropdown", NULL );
	self.item = LCUIWidget_NewPrototype( "dropdown-item", "textview" );
	self.header = LCUIWidget_NewPrototype( "dropdown-header", "textview" );
	self.dropdown->init = Dropdown_Init;
	self.dropdown->setattr = Dropdown_SetAttr;
	self.item->init = DropdownItem_Init;
	self.item->destroy = DropdownItem_Destrtoy;
	self.header->destroy = DropdownHeader_Destrtoy;
	self.change_event = LCUIWidget_AllocEventId();
	LCUIWidget_SetEventName( self.change_event, "change.dropdown" );
	LCUI_LoadCSSString( dropdown_css, NULL );
}
