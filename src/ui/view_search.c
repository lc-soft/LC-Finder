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

#include <stdlib.h>
#include <string.h>
#include "finder.h"
#include "ui.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/widget/textedit.h>
#include "thumbview.h"
#include "browser.h"

#define TEXT_TITLE		L"搜索结果"
#define TAG_MAX_WIDTH		180
#define TAG_MARGIN_RIGHT	10

 /** 文件扫描功能的相关数据 */
typedef struct FileScannerRec_ {
	LCUI_Thread tid;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	LCUI_BOOL is_running;
	LinkedList files;
	DB_Tag *tags;
	int n_tags;
	int count, total;
} FileScannerRec, *FileScanner;

/** 视图同步功能的相关数据 */
typedef struct ViewSyncRec_ {
	LCUI_Thread tid;
	LCUI_BOOL is_running;
	LCUI_Mutex mutex;
	LCUI_Cond ready;
} ViewSyncRec, *ViewSync;

static struct SearchView {
	LCUI_Widget input;
	LCUI_Widget view_tags;
	LCUI_Widget view_result;
	LCUI_Widget view_files;
	LCUI_Widget btn_search;
	LCUI_Widget tip_empty_tags;
	LCUI_Widget tip_empty_files;
	LCUI_BOOL need_update;
	struct {
		int count;
		int tags_per_row;
		int max_width;
	} layout;
	LinkedList tags;
	ViewSyncRec viewsync;
	FileScannerRec scanner;
	FileBrowserRec browser;
} this_view;

typedef struct TagItemRec_ {
	LCUI_Widget widget;
	DB_Tag tag;
} TagItemRec, *TagItem;

static void OnDeleteDBFile( void *arg )
{
	DBFile_Release( arg );
}

/** 扫描全部文件 */
static int FileScanner_ScanAll( FileScanner scanner )
{
	DB_File file;
	DB_Query query;
	int i, total, count;
	DB_QueryTermsRec terms;
	terms.dirpath = NULL;
	terms.n_dirs = 0;
	terms.dirs = NULL;
	terms.limit = 100;
	terms.offset = 0;
	terms.score = NONE;
	terms.for_tree = FALSE;
	terms.tags = scanner->tags;
	terms.n_tags = scanner->n_tags;
	terms.create_time = DESC;
	query = DB_NewQuery( &terms );
	count = total = DBQuery_GetTotalFiles( query );
	scanner->total = total;
	scanner->count = 0;
	while( scanner->is_running && count > 0 ) {
		DB_DeleteQuery( query );
		query = DB_NewQuery( &terms );
		i = count < terms.limit ? count : terms.limit;
		for( ; scanner->is_running && i > 0; --i ) {
			file = DBQuery_FetchFile( query );
			if( !file ) {
				break;
			}
			LCUIMutex_Lock( &scanner->mutex );
			LinkedList_Append( &scanner->files, file );
			LCUICond_Signal( &scanner->cond );
			LCUIMutex_Unlock( &scanner->mutex );
			scanner->count += 1;
		}
		count -= terms.limit;
		terms.offset += terms.limit;
	}
	return total;
}

/** 初始化文件扫描 */
static void FileScanner_Init( FileScanner scanner )
{
	LCUICond_Init( &scanner->cond );
	LCUIMutex_Init( &scanner->mutex );
	LinkedList_Init( &scanner->files );
	this_view.scanner.tags = NULL;
	this_view.scanner.n_tags = 0;
}

/** 重置文件扫描 */
static void FileScanner_Reset( FileScanner scanner )
{
	if( scanner->is_running ) {
		scanner->is_running = FALSE;
		LCUIThread_Join( scanner->tid, NULL );
	}
	LCUIMutex_Lock( &scanner->mutex );
	LinkedList_Clear( &scanner->files, OnDeleteDBFile );
	LCUICond_Signal( &scanner->cond );
	LCUIMutex_Unlock( &scanner->mutex );
}

/** 文件扫描线程 */
static void FileScanner_Thread( void *arg )
{
	int count;
	this_view.scanner.is_running = TRUE;
	count = FileScanner_ScanAll( &this_view.scanner );
	if( count > 0 ) {
		Widget_AddClass( this_view.tip_empty_files, "hide" );
		Widget_Hide( this_view.tip_empty_files );
	} else {
		Widget_RemoveClass( this_view.tip_empty_files, "hide" );
		Widget_Show( this_view.tip_empty_files );
	}
	this_view.scanner.is_running = FALSE;
	LCUIThread_Exit( NULL );
}

/** 开始扫描 */
static void FileScanner_Start( FileScanner scanner )
{
	LCUIThread_Create( &scanner->tid, FileScanner_Thread, NULL );
}

static void FileScanner_Destroy( FileScanner scanner )
{
	FileScanner_Reset( scanner );
	LCUICond_Destroy( &scanner->cond );
	LCUIMutex_Destroy( &scanner->mutex );
}

/** 视图同步线程 */
static void ViewSyncThread( void *arg )
{
	ViewSync vs;
	FileScanner scanner;
	vs = &this_view.viewsync;
	scanner = &this_view.scanner;
	LCUIMutex_Lock( &vs->mutex );
	/* 等待缩略图列表部件准备完毕 */
	while( this_view.view_files->state < WSTATE_READY ) {
		LCUICond_TimedWait( &vs->ready, &vs->mutex, 100 );
	}
	LCUIMutex_Unlock( &vs->mutex );
	vs->is_running = TRUE;
	while( vs->is_running ) {
		LinkedListNode *node;
		LCUIMutex_Lock( &scanner->mutex );
		if( scanner->files.length == 0 ) {
			LCUICond_Wait( &scanner->cond, &scanner->mutex );
			if( !vs->is_running ) {
				LCUIMutex_Unlock( &scanner->mutex );
				break;
			}
		}
		LCUIMutex_Lock( &vs->mutex );
		node = LinkedList_GetNode( &scanner->files, 0 );
		if( !node ) {
			LCUIMutex_Unlock( &vs->mutex );
			LCUIMutex_Unlock( &scanner->mutex );
			continue;
		}
		LinkedList_Unlink( &scanner->files, node );
		LCUIMutex_Unlock( &scanner->mutex );
		FileBrowser_AppendPicture( &this_view.browser, node->data );
		LCUIMutex_Unlock( &vs->mutex );
		free( node );
	}
	LCUIThread_Exit( NULL );
}

static void UpdateLayoutContext( void )
{
	double tmp;
	int max_width, n;
	max_width = this_view.view_tags->box.content.width;
	n = max_width + TAG_MARGIN_RIGHT;
	tmp = 1.0 * n / (TAG_MAX_WIDTH + TAG_MARGIN_RIGHT);
	n /= TAG_MAX_WIDTH + TAG_MARGIN_RIGHT;
	if( tmp > 1.0 * n ) {
		n = n + 1;
	}
	this_view.layout.max_width = max_width;
	this_view.layout.tags_per_row = n;
}

static void OnTagViewStartLayout( LCUI_Widget w )
{
	this_view.layout.count = 0;
	this_view.layout.tags_per_row = 2;
	UpdateLayoutContext();
}

static void AddTagToSearch( LCUI_Widget w )
{
	int len;
	DB_Tag tag = NULL;
	LinkedListNode *node;
	wchar_t *tagname, text[512];
	LinkedList_ForEach( node, &this_view.tags ) {
		TagItem item = node->data;
		if( item->widget == w ) {
			tag = item->tag;
			break;
		}
	}
	if( !tag ) {
		return;
	}
	len = strlen( tag->name ) + 1;
	tagname = NEW( wchar_t, len );
	LCUI_DecodeString( tagname, tag->name, len, ENCODING_UTF8 );
	len = TextEdit_GetTextW( this_view.input, 0, 511, text );
	if( len > 0 && wcsstr( text, tagname ) ) {
		free( tagname );
		return;
	}
	if( len == 0 ) {
		wcsncpy( text, tagname, 511 );
	} else {
		swprintf( text, 511, L" %s", tagname );
	}
	TextEdit_AppendTextW( this_view.input, text );
	free( tagname );
}

static void DeleteTagFromSearch( LCUI_Widget w )
{
	int len, taglen;
	DB_Tag tag = NULL;
	LinkedListNode *node;
	wchar_t *tagname, text[512], *p, *pend;
	LinkedList_ForEach( node, &this_view.tags ) {
		TagItem item = node->data;
		if( item->widget == w ) {
			tag = item->tag;
			break;
		}
	}
	if( !tag ) {
		return;
	}
	len = strlen( tag->name ) + 1;
	tagname = NEW( wchar_t, len );
	taglen = LCUI_DecodeString( tagname, tag->name, len, ENCODING_UTF8 );
	len = TextEdit_GetTextW( this_view.input, 0, 511, text );
	if( len == 0 ) {
		return;
	}
	p = wcsstr( text, tagname );
	if( !p ) {
		return;
	}
	/* 将标签名后面的空格数量也算入标签长度内，以清除多余空格 */
	for( pend = p + taglen; *pend && *pend == L' '; ++pend, ++taglen );
	pend = &text[len - taglen];
	for( ; p < pend; ++p ) {
		*p = *(p + taglen);
	}
	*pend = 0;
	TextEdit_SetTextW( this_view.input, text );
	free( tagname );
}

static void OnTagClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( Widget_HasClass(w, "selected") ) {
		Widget_RemoveClass( w, "selected" );
		DeleteTagFromSearch( w );
	} else {
		Widget_AddClass( w, "selected" );
		AddTagToSearch( w );
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

static void UpdateTagSize( LCUI_Widget w )
{
	int width, n;
	++this_view.layout.count;
	if( this_view.layout.count == 1 ) {
		UpdateLayoutContext();
	}
	if( this_view.layout.max_width < TAG_MAX_WIDTH ) {
		return;
	}
	n = this_view.layout.tags_per_row;
	width = this_view.layout.max_width;
	width = (width - TAG_MARGIN_RIGHT*(n - 1)) / n;
	/* 设置每行最后一个文件夹的右边距为 0px */
	if( this_view.layout.count % n == 0 ) {
		SetStyle( w->custom_style, key_margin_right, 0, px );
	} else {
		w->custom_style->sheet[key_margin_right].is_valid = FALSE;
	}
	SetStyle( w->custom_style, key_width, width, px );
	Widget_UpdateStyle( w, FALSE );
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
	terms.for_tree = FALSE;
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
	ThumbViewItem_SetFunction( box, SetTagCover, 
				   UnsetTagCover, UpdateTagSize );
	return box;
}

static void OnTagUpdate( LCUI_Event e, void *arg )
{
	this_view.need_update = TRUE;
}

static void OnBtnClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.need_update ) {
		UI_UpdateSearchView();
	}
}

static void StartSearchFiles( LinkedList *tags )
{
	int n_tags = 0;
	DB_Tag *newtags;
	LinkedListNode *node;
	newtags = malloc( sizeof( DB_Tag ) * (tags->length + 1) );
	if( !newtags ) {
		return;
	}
	LinkedList_ForEach( node, tags ) {
		int i;
		for( i = 0; i < finder.n_tags; ++i ) {
			DB_Tag tag = finder.tags[i];
			if( strcmp( tag->name, node->data ) == 0 ) {
				newtags[n_tags++] = tag;
			}
		}
	}
	newtags[n_tags] = NULL;
	FileScanner_Reset( &this_view.scanner );
	if( this_view.scanner.tags ) {
		free( this_view.scanner.tags );
	}
	this_view.scanner.tags = newtags;
	this_view.scanner.n_tags = n_tags;
	LCUIMutex_Lock( &this_view.viewsync.mutex );
	FileBrowser_Empty( &this_view.browser );
	FileScanner_Start( &this_view.scanner );
	LCUIMutex_Unlock( &this_view.viewsync.mutex );
}

static void OnBtnSearchClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	int i, j, len;
	LinkedList tags;
	LCUI_BOOL saving;
	wchar_t wstr[512] = {0};
	char *str, buf[512], *tagname;

	len = TextEdit_GetTextW( this_view.input, 0, 511, wstr );
	if( len == 0 ) {
		return;
	}
	len = LCUI_EncodeString( NULL, wstr, 0, ENCODING_UTF8 ) + 1;
	str = malloc( sizeof(char) * len );
	if( !str ) {
		return;
	}
	len = LCUI_EncodeString( str, wstr, len, ENCODING_UTF8 );
	LinkedList_Init( &tags );
	for( saving = FALSE, i = 0, j = 0; i <= len; ++i ) {
		if( str[i] != ' ' && str[i] != 0 ) {
			saving = TRUE;
			buf[j++] = str[i];
			continue;
		}
		if( saving ) {
			buf[j++] = 0;
			tagname = malloc( sizeof( char )*j );
			strcpy( tagname, buf );
			LinkedList_Append( &tags, tagname );
			j = 0;
		}
		saving = FALSE;
	}
	if( tags.length == 0 ) {
		return;
	}
	Widget_RemoveClass( this_view.view_result, "hide" );
	Widget_Show( this_view.view_result );
	StartSearchFiles( &tags );
	LinkedList_Clear( &tags, free );
}

static void OnBtnHideReusltClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.need_update ) {
		UI_UpdateSearchView();
	}
	Widget_Hide( this_view.view_result );
	ThumbView_Lock( this_view.view_files );
	ThumbView_Empty( this_view.view_files );
	ThumbView_Unlock( this_view.view_files );
}

void UI_UpdateSearchView( void )
{
	int i, count;
	LCFinder_ReloadTags();
	this_view.layout.count = 0;
	ThumbView_Empty( this_view.view_tags );
	LinkedList_Clear( &this_view.tags, free );
	for( i = 0, count = 0; i < finder.n_tags; ++i ) {
		TagItem item;
		if( finder.tags[i]->count <= 0 ) {
			continue;
		}
		item = NEW( TagItemRec, 1 );
		item->tag = finder.tags[i];
		item->widget = CreateTagWidget( finder.tags[i] );
		ThumbView_Append( this_view.view_tags, item->widget );
		LinkedList_Append( &this_view.tags, item );
		++count;
	}
	if( count > 0 ) {
		Widget_Hide( this_view.tip_empty_tags );
		Widget_AddClass( this_view.tip_empty_tags, "hide" );
		Widget_SetDisabled( this_view.btn_search, FALSE );
		Widget_SetDisabled( this_view.input, FALSE );
	} else {
		TextEdit_ClearText( this_view.input );
		Widget_Show( this_view.tip_empty_tags );
		Widget_RemoveClass( this_view.tip_empty_tags, "hide" );
		Widget_SetDisabled( this_view.btn_search, TRUE );
		Widget_SetDisabled( this_view.input, TRUE );
	}
	this_view.need_update = FALSE;
}

void UI_InitSearchView( void )
{
	LCUI_Widget btn[6], title;
	LCUI_Widget tip1 = LCUIWidget_GetById( ID_TIP_SEARCH_TAGS_EMPTY );
	LCUI_Widget tip2 = LCUIWidget_GetById( ID_TIP_SEARCH_FILES_EMPTY );
	LCUI_Widget btn_search = LCUIWidget_GetById( ID_BTN_SEARCH_FILES );
	LCUI_Widget btn_hide = LCUIWidget_GetById( ID_BTN_HIDE_SEARCH_RESULT );
	LCUI_Widget input = LCUIWidget_GetById( ID_INPUT_SEARCH );
	title = LCUIWidget_GetById( ID_TXT_VIEW_SEARCH_RESULT_TITLE );
	btn[0] = LCUIWidget_GetById( ID_BTN_SIDEBAR_SEEARCH );
	btn[1] = LCUIWidget_GetById( ID_BTN_SELECT_SEARCH_FILES );
	btn[2] = LCUIWidget_GetById( ID_BTN_CANCEL_SEARCH_SELECT );
	btn[3] = LCUIWidget_GetById( ID_BTN_TAG_SEARCH_FILES );
	btn[4] = LCUIWidget_GetById( ID_BTN_DELETE_SEARCH_FILES );
	LinkedList_Init( &this_view.tags );
	FileScanner_Init( &this_view.scanner );
	LCUICond_Init( &this_view.viewsync.ready );
	LCUIMutex_Init( &this_view.viewsync.mutex );
	this_view.layout.tags_per_row = 2;
	this_view.layout.count = 0;
	this_view.input = input;
	this_view.tip_empty_tags = tip1;
	this_view.tip_empty_files = tip2;
	this_view.btn_search = btn_search;
	this_view.view_tags = LCUIWidget_GetById( ID_VIEW_SEARCH_TAGS );
	this_view.view_result = LCUIWidget_GetById( ID_VIEW_SEARCH_RESULT );
	this_view.view_files = LCUIWidget_GetById( ID_VIEW_SEARCH_FILES );
	this_view.browser.title = TEXT_TITLE;
	this_view.browser.btn_select = btn[1];
	this_view.browser.btn_cancel = btn[2];
	this_view.browser.btn_tag = btn[3];
	this_view.browser.btn_delete = btn[4];
	this_view.browser.txt_title = title;
	this_view.browser.items = this_view.view_files;
	this_view.browser.view = this_view.view_result;
	ThumbView_SetCache( this_view.view_tags, finder.thumb_cache );
	ThumbView_SetCache( this_view.view_files, finder.thumb_cache );
	Widget_BindEvent( btn[0], "click", OnBtnClick, NULL, NULL );
	Widget_BindEvent( btn_search, "click", OnBtnSearchClick, NULL, NULL );
	Widget_BindEvent( btn_hide, "click", OnBtnHideReusltClick, NULL, NULL );
	LCFinder_BindEvent( EVENT_TAG_UPDATE, OnTagUpdate, NULL );
	ThumbView_OnLayout( this_view.view_tags, OnTagViewStartLayout );
	FileBrowser_Create( &this_view.browser );
	LCUIThread_Create( &this_view.viewsync.tid, ViewSyncThread, NULL );
	UI_UpdateSearchView();
}

void UI_ExitSearchView( void )
{

}
