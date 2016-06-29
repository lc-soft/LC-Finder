/* ***************************************************************************
 * view_search.c -- search view
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
 * view_search.c -- “搜索”视图
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
#include "ui.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "thumbview.h"

static struct SearchView {
	LCUI_Widget input;
	LCUI_Widget view_tags;
	LinkedList tags;
	LCUI_BOOL need_update;
} this_view;

typedef struct TagItemRec_ {
	LCUI_Widget widget;
	DB_Tag tag;
} TagItemRec, *TagItem;

static void OnTagClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( Widget_HasClass(w, "selected") ) {
		Widget_RemoveClass( w, "selected" );
	} else {
		Widget_AddClass( w, "selected" );
	}
}

static void SetTagCover( LCUI_Widget w, LCUI_Graph *thumb )
{
	Widget_Lock( w );
	SetStyle( w->custom_style, key_background_image, thumb, image );
	Widget_UpdateStyle( w, FALSE );
	Widget_Unlock( w );
}

static void UnsetTagCover( LCUI_Widget w )
{
	Widget_Lock( w );
	DEBUG_MSG("remove thumb\n");
	Graph_Init( &w->computed_style.background.image );
	w->custom_style->sheet[key_background_image].is_valid = FALSE;
	Widget_UpdateStyle( w, FALSE );
	Widget_Unlock( w );
}

static DB_File GetFileByTag( DB_Tag tag )
{
	DB_File file;
	DB_Query query;
	DB_QueryTermsRec terms;
	terms.dirpath = NULL;
	terms.tags = malloc( sizeof( DB_Tag ) );
	terms.tags[0] = tag;
	terms.n_dirs = 1;
	terms.n_tags = 1;
	terms.limit = 1;
	terms.offset = 0;
	terms.dirs = NULL;
	terms.create_time = DESC;
	query = DB_NewQuery( &terms );
	file = DBQuery_FetchFile( query );
	free( terms.tags );
	return file;
}

static LCUI_Widget CreateTagWidget( DB_Tag tag )
{
	DB_File file;
	wchar_t str[64] = {0};
	LCUI_Widget cover = LCUIWidget_New( NULL );
	LCUI_Widget check = LCUIWidget_New( "textview" );
	LCUI_Widget box = LCUIWidget_New( "thumbviewitem" );
	LCUI_Widget txt_name = LCUIWidget_New( "textview" );
	LCUI_Widget txt_count = LCUIWidget_New( "textview" );
	LCUI_Widget info = LCUIWidget_New( NULL );
	swprintf( str, 63, L"%d", tag->count );
	Widget_AddClass( box, "tag-thumb-list-item" );
	Widget_AddClass( info, "tag-thumb-list-item-info" );
	Widget_AddClass( cover, "tag-thumb-list-item-cover" );
	Widget_AddClass( check, "mdi mdi-check tag-thumb-list-item-checkbox" );
	Widget_AddClass( txt_name, "name" );
	Widget_AddClass( txt_count, "count" );
	TextView_SetText( txt_name, tag->name );
	TextView_SetTextW( txt_count, str );
	Widget_Append( cover, check );
	Widget_Append( info, txt_name );
	Widget_Append( info, txt_count );
	Widget_Append( box, info );
	Widget_Append( box, cover );
	Widget_Append( this_view.view_tags, box );
	Widget_BindEvent( box, "click", OnTagClick, tag, NULL );
	file = GetFileByTag( tag );
	ThumbViewItem_BindFile( box, file );
	ThumbViewItem_SetFunction( box, SetTagCover, UnsetTagCover );
	return box;
}

static void OnTagUpdate( LCUI_Event e, void *arg )
{
	this_view.need_update = TRUE;
}

static void OnBtnClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.need_update ) {
		UI_UpdateSerarchView();
	}
}

void UI_UpdateSerarchView( void )
{
	int i;
	ThumbView_Empty( this_view.view_tags );
	LinkedList_Clear( &this_view.tags, free );
	for( i = 0; i < finder.n_tags; ++i ) {
		TagItem item = NEW( TagItemRec, 1 );
		item->tag = finder.tags[i];
		item->widget = CreateTagWidget( finder.tags[i] );
		ThumbView_Append( this_view.view_tags, item->widget );
		LinkedList_Append( &this_view.tags, item );
	}
	this_view.need_update = FALSE;
}

void UI_InitSearchView( void )
{
	LCUI_Widget btn;
	LinkedList_Init( &this_view.tags );
	this_view.input = LCUIWidget_GetById( ID_INPUT_SEARCH );
	this_view.view_tags = LCUIWidget_GetById( ID_VIEW_SEARCH_TAGS );
	btn = LCUIWidget_GetById( ID_BTN_SIDEBAR_SEEARCH );
	Widget_BindEvent( btn, "click", OnBtnClick, NULL, NULL );
	LCFinder_BindEvent( EVENT_TAG_UPDATE, OnTagUpdate, NULL );
	UI_UpdateSerarchView();
}
