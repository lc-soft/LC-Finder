/* ***************************************************************************
 * view_search.c -- search view
 *
 * Copyright (C) 2016-2017 by Liu Chao <lc-soft@live.cn>
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
 * 版权所有 (C) 2016-2017 归属于 刘超 <lc-soft@live.cn>
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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "finder.h"
#include "file_storage.h"
#include "ui.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/widget/textedit.h>
#include "thumbview.h"
#include "textview_i18n.h"
#include "browser.h"
#include "dropdown.h"
#include "i18n.h"

#define KEY_SORT_HEADER		"sort.header"
#define KEY_TITLE		"search.results.title"
#define KEY_INPUT_PLACEHOLDER	"search.placeholder.search_input"
#define TAG_MAX_WIDTH		180
#define TAG_MARGIN_RIGHT	10
#define SORT_METHODS_LEN	6

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
	LCUI_Widget txt_count;
	LCUI_Widget btn_search;
	LCUI_Widget tip_empty_tags;
	LCUI_Widget tip_empty_files;
	LCUI_Widget selected_sort;
	LCUI_BOOL need_update;
	struct {
		int count;
		int tags_per_row;
		float max_width;
	} layout;
	int sort_mode;
	LinkedList tags;
	ViewSyncRec viewsync;
	FileScannerRec scanner;
	FileBrowserRec browser;
	DB_QueryTermsRec terms;
} this_view = {0};

static FileSortMethodRec sort_methods[SORT_METHODS_LEN] = {
	{"sort.ctime_desc", CREATE_TIME_DESC},
	{"sort.ctime_asc", CREATE_TIME_ASC},
	{"sort.mtime_desc", MODIFY_TIME_DESC},
	{"sort.mtime_asc", MODIFY_TIME_ASC},
	{"sort.score_desc", SCORE_DESC},
	{"sort.score_asc", SCORE_ASC}
};

typedef struct TagItemRec_ {
	LCUI_Widget widget;
	DB_Tag tag;
} TagItemRec, *TagItem;

static void OnDeleteDBFile( void *arg )
{
	DBFile_Release( arg );
}

static void SetSearchResultsCount( int count )
{
	wchar_t str[128], count_str[64];
	wgetnumberstr( count_str, 63, count );
	swprintf( str, 255, L" (%ls)", count_str );
	TextView_SetTextW( this_view.txt_count, str );
	if( count > 0 ) {
		Widget_Show( this_view.txt_count );
	} else {
		Widget_Hide( this_view.txt_count );
	}
}

/** 扫描全部文件 */
static int FileScanner_ScanAll( FileScanner scanner )
{
	DB_File file;
	DB_Query query;
	DB_QueryTerms terms;
	int i, total, count;
	terms = &this_view.terms;
	terms->offset = 0;
	terms->limit = 100;
	terms->tags = scanner->tags;
	terms->n_tags = scanner->n_tags;
	if( terms->dirs ) {
		int n_dirs;
		free( terms->dirs );
		terms->dirs = NULL;
		n_dirs = LCFinder_GetSourceDirList( &terms->dirs );
		if( n_dirs == finder.n_dirs ) {
			free( terms->dirs );
			terms->dirs = NULL;
		}
	}
	query = DB_NewQuery( &this_view.terms );
	count = total = DBQuery_GetTotalFiles( query );
	scanner->total = total, scanner->count = 0;
	SetSearchResultsCount( total );
	while( scanner->is_running && count > 0 ) {
		DB_DeleteQuery( query );
		query = DB_NewQuery( &this_view.terms );
		if(count < terms->limit) {
			i = count;
		} else {
			i = terms->limit;
		}
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
		count -= terms->limit;
		terms->offset += terms->limit;
	}
	if( terms->dirs ) {
		free( terms->dirs );
		terms->dirs = NULL;
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
		LinkedListNode_Delete( node );
	}
	LCUIThread_Exit( NULL );
}

static void UpdateLayoutContext( void )
{
	double n;
	float max_width;
	max_width = this_view.view_tags->parent->box.content.width;
	n = max_width + TAG_MARGIN_RIGHT;
	n = ceil( n / (TAG_MAX_WIDTH + TAG_MARGIN_RIGHT) );
	this_view.layout.max_width = max_width;
	this_view.layout.tags_per_row = (int)n;
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
	SetStyle( w->custom_style, key_background_image, thumb, image );
	Widget_UpdateStyle( w, FALSE );
}

static void UnsetTagCover( LCUI_Widget w )
{
	DEBUG_MSG("remove thumb\n");
	Graph_Init( &w->computed_style.background.image );
	w->custom_style->sheet[key_background_image].is_valid = FALSE;
	Widget_UpdateStyle( w, FALSE );
}

static void UpdateTagSize( LCUI_Widget w )
{
	int n;
	float width;
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
		Widget_SetStyle( w, key_margin_right, 0, px );
	} else {
		Widget_UnsetStyle( w, key_margin_right );
	}
	Widget_SetStyle( w, key_width, width, px );
	Widget_UpdateStyle( w, FALSE );
}

static DB_File GetFileByTag( DB_Tag tag )
{
	DB_File file;
	DB_Query query;
	DB_QueryTermsRec terms = {0};
	terms.tags = malloc( sizeof( DB_Tag ) );
	terms.tags[0] = tag;
	terms.n_dirs = 1;
	terms.n_tags = 1;
	terms.limit = 1;
	terms.modify_time = DESC;
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
	Widget_AddClass( check, "icon icon-check tag-thumb-list-item-checkbox" );
	Widget_AddClass( txt_name, "text name" );
	Widget_AddClass( txt_count, "text count" );
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

static void UpdateSearchResults( void )
{
	FileScanner_Reset( &this_view.scanner );
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

static void OnBtnSearchResize( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	Widget_SetStyle( w->parent, key_padding_right, 9 + w->width, px );
	Widget_UpdateStyle( w->parent, FALSE );
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

static void UpdateQueryTerms( void )
{
	this_view.terms.modify_time = NONE;
	this_view.terms.create_time = NONE;
	this_view.terms.score = NONE;
	switch( this_view.sort_mode ) {
	case CREATE_TIME_DESC:
		this_view.terms.create_time = DESC;
		break;
	case CREATE_TIME_ASC:
		this_view.terms.create_time = ASC;
		break;
	case SCORE_DESC:
		this_view.terms.score = DESC;
		break;
	case SCORE_ASC:
		this_view.terms.score = ASC;
		break;
	case MODIFY_TIME_ASC:
		this_view.terms.modify_time = ASC;
		break;
	case MODIFY_TIME_DESC:
	default:
		this_view.terms.modify_time = DESC;
		break;
	}
}

static void OnSelectSortMethod( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	FileSortMethod sort = arg;
	if( this_view.sort_mode == sort->value ) {
		return;
	}
	this_view.sort_mode = sort->value;
	if( this_view.selected_sort ) {
		Widget_RemoveClass( this_view.selected_sort, "active" );
	}
	this_view.selected_sort = e->target;
	Widget_AddClass( e->target, "active" );
	UpdateQueryTerms();
	UpdateSearchResults();
}

static void InitSearchResultsSort( void )
{
	int i;
	LCUI_Widget menu, item, icon, text;
	const wchar_t *header = I18n_GetText( KEY_SORT_HEADER );
	SelectWidget( menu, ID_DROPDOWN_SEARCH_FILES_SORT );
	Widget_Empty( menu );
	Dropdown_SetHeaderW( menu, header );
	for( i = 0; i < SORT_METHODS_LEN; ++i ) {
		FileSortMethod sort = &sort_methods[i];
		item = Dropdown_AddItem( menu, sort );
		icon = LCUIWidget_New( "textview" );
		text = LCUIWidget_New( "textview-i18n" );
		TextViewI18n_SetKey( text, sort->name_key );
		Widget_AddClass( icon, "icon icon icon-check" );
		Widget_AddClass( text, "text" );
		Widget_Append( item, icon );
		Widget_Append( item, text );
		if( sort->value == this_view.sort_mode ) {
			Widget_AddClass( item, "active" );
			this_view.selected_sort = item;
		}
	}
	BindEvent( menu, "change.dropdown", OnSelectSortMethod );
	UpdateQueryTerms();
}

static void UpdateSearchInput( void )
{
	const wchar_t *text = I18n_GetText( KEY_INPUT_PLACEHOLDER );
	TextEdit_SetPlaceHolderW( this_view.input, text );
}

static void OnLanguageChanged( void *privdata, void *data )
{
	LCUI_Widget menu;
	const wchar_t *header;
	header = I18n_GetText( KEY_SORT_HEADER );
	SelectWidget( menu, ID_DROPDOWN_FOLDER_FILES_SORT );
	Dropdown_SetHeaderW( menu, header );
	UpdateSearchInput();
}

static void OnTagThumbViewReady( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	UI_UpdateSearchView();
}

void UI_InitSearchView( void )
{
	LCUI_Widget btn[6], title, btn_hide;
	this_view.layout.count = 0;
	this_view.layout.tags_per_row = 2;
	LinkedList_Init( &this_view.tags );
	FileScanner_Init( &this_view.scanner );
	LCUICond_Init( &this_view.viewsync.ready );
	LCUIMutex_Init( &this_view.viewsync.mutex );
	SelectWidget( btn[0], ID_BTN_SIDEBAR_SEEARCH );
	SelectWidget( btn[1], ID_BTN_SELECT_SEARCH_FILES );
	SelectWidget( btn[2], ID_BTN_CANCEL_SEARCH_SELECT );
	SelectWidget( btn[3], ID_BTN_TAG_SEARCH_FILES );
	SelectWidget( btn[4], ID_BTN_DELETE_SEARCH_FILES );
	SelectWidget( this_view.input, ID_INPUT_SEARCH );
	SelectWidget( btn_hide, ID_BTN_HIDE_SEARCH_RESULT );
	SelectWidget( title, ID_TXT_VIEW_SEARCH_RESULT_TITLE );
	SelectWidget( this_view.txt_count, ID_TXT_VIEW_SEARCH_RESULT_COUNT );
	SelectWidget( this_view.tip_empty_tags, ID_TIP_SEARCH_TAGS_EMPTY );
	SelectWidget( this_view.tip_empty_files, ID_TIP_SEARCH_FILES_EMPTY );
	SelectWidget( this_view.btn_search, ID_BTN_SEARCH_FILES );
	SelectWidget( this_view.view_tags, ID_VIEW_SEARCH_TAGS );
	SelectWidget( this_view.view_result, ID_VIEW_SEARCH_RESULT );
	SelectWidget( this_view.view_files, ID_VIEW_SEARCH_FILES );
	this_view.browser.title_key = KEY_TITLE;
	this_view.browser.btn_select = btn[1];
	this_view.browser.btn_cancel = btn[2];
	this_view.browser.btn_tag = btn[3];
	this_view.browser.btn_delete = btn[4];
	this_view.browser.txt_title = title;
	this_view.browser.items = this_view.view_files;
	this_view.browser.view = this_view.view_result;
	ThumbView_SetCache( this_view.view_tags, finder.thumb_cache );
	ThumbView_SetCache( this_view.view_files, finder.thumb_cache );
	ThumbView_SetStorage( this_view.view_tags, finder.storage_for_thumb );
	ThumbView_SetStorage( this_view.view_files, finder.storage_for_thumb );
	BindEvent( btn[0], "click", OnBtnClick );
	BindEvent( btn_hide, "click", OnBtnHideReusltClick );
	BindEvent( this_view.btn_search, "click", OnBtnSearchClick );
	BindEvent( this_view.btn_search, "resize", OnBtnSearchResize );
	BindEvent( this_view.view_tags, "ready", OnTagThumbViewReady );
	LCFinder_BindEvent( EVENT_TAG_UPDATE, OnTagUpdate, NULL );
	LCFinder_BindEvent( EVENT_LANG_CHG, OnLanguageChanged, NULL );
	ThumbView_OnLayout( this_view.view_tags, OnTagViewStartLayout );
	LCUIThread_Create( &this_view.viewsync.tid, ViewSyncThread, NULL );
	FileBrowser_Create( &this_view.browser );
	InitSearchResultsSort();
	UpdateSearchInput();
}

void UI_ExitSearchView( void )
{
	// ...
}
