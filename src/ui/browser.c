/* ***************************************************************************
 * browser.c -- file browser
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
 * browser.c -- 文件浏览器
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

#include <time.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/widget/button.h>
#include "thumbview.h"
#include "progressbar.h"
#include "dialog.h"
#include "browser.h"

#define TEXT_CANCEL			L"取消"
#define TEXT_DELETING			L"删除中..."
#define TEXT_TAG_ADDING			L"添加标签中..."
#define TEXT_DELETION_PROGRESS		L"已删除 %d 个文件，共 %d 个文件"
#define TEXT_TAG_ADDTION_PROGRESS	L"已处理 %d 个文件，共 %d 个文件"
#define TEXT_NO_SELECTED_ITEMS		L"未选择任何项目"
#define TEXT_SELECTED_ITEMS		L"已选择 %d 项"
#define DIALOG_TITLE_DELETE		L"提示"
#define DIALOG_TEXT_DELETE		L"确定要删除选中的 %d 项文件？"
#define DIALOG_TITLE_ADD_TAG		L"为选中的文件添加标签"
#define DIALOG_PLACEHOLDER_ADD_TAG	L"标签名称，多个标签用空格隔开"
#define MAX_TAG_LEN			256

 /** 文件索引记录 */
typedef struct FileIndexRec_ {
	LCUI_BOOL is_file;
	union {
		DB_File file;
		char *path;
	};
	LCUI_Widget checkbox;
	LCUI_Widget item;
	LinkedListNode node;
} FileIndexRec, *FileIndex;

typedef struct DataPackRec_ {
	FileIndex fidx;
	FileBrowser browser;
} DataPackRec, *DataPack;

 /** 对话框数据包 */
typedef struct DialogDataPackRec_ {
	LCUI_BOOL active;
	LCUI_Thread thread;
	FileBrowser browser;
	const char **tagnames;
	LCUI_ProgressDialog dialog;
} DialogDataPackRec, *DialogDataPack;

static void FileIterator_Destroy( FileIterator iter )
{
	iter->privdata = NULL;
	iter->filepath = NULL;
	iter->next = NULL;
	iter->prev = NULL;
	free( iter );
}

static void FileIterator_Update( FileIterator iter )
{
	DataPack data = iter->privdata;
	iter->filepath = data->fidx->file->path;
	iter->length = data->browser->files.length;
}

static void FileIterator_Next( FileIterator iter )
{
	DataPack data = iter->privdata;
	if( data->fidx->node.next ) {
		iter->index += 1;
		data->fidx = data->fidx->node.next->data;
		FileIterator_Update( iter );
	}
}

static void FileIterator_Prev( FileIterator iter )
{
	DataPack data = iter->privdata;
	if( data->fidx->node.prev && 
	    &data->fidx->node != data->browser->files.head.next ) {
		iter->index -= 1;
		data->fidx = data->fidx->node.prev->data;
		FileIterator_Update( iter );
	}
}

static FileIterator FileIterator_Create( FileBrowser browser, FileIndex fidx )
{
	LinkedListNode *node = &fidx->node;
	DataPack data = NEW( DataPackRec, 1 );
	FileIterator iter = NEW( FileIteratorRec, 1 );

	data->fidx = fidx;
	data->browser = browser;
	iter->index = 0;
	iter->privdata = data;
	iter->next = FileIterator_Next;
	iter->prev = FileIterator_Prev;
	iter->destroy = FileIterator_Destroy;
	while( node != browser->files.head.next ) {
		node = node->prev;
		iter->index += 1;
	}
	FileIterator_Update( iter );
	return iter;
}

static void FileIndex_Delete( FileIndex fidx )
{
	if(  fidx->is_file ) {
		DBFile_Release( fidx->file );
		fidx->file = NULL;
	} else {
		free( fidx->path );
		fidx->path = NULL;
	}
	fidx->checkbox = NULL;
	fidx->item = NULL;
}

static void FileBrowser_UpdateSelectionUI( FileBrowser browser )
{
	wchar_t str[256];
	LCUI_Widget btn_del, btn_add_tags;
	btn_del = LCUIWidget_GetById( ID_BTN_DELETE_HOME_FILES );
	btn_add_tags = LCUIWidget_GetById( ID_BTN_TAG_HOME_FILES );
	if( browser->selected_files.length > 0 ) {
		Widget_SetDisabled( btn_del, FALSE );
		Widget_SetDisabled( btn_add_tags, FALSE );
		swprintf( str, 255, TEXT_SELECTED_ITEMS,
			  browser->selected_files.length );
		TextView_SetTextW( browser->txt_title, str );
	} else {
		Widget_SetDisabled( btn_del, TRUE );
		Widget_SetDisabled( btn_add_tags, TRUE );
		TextView_SetTextW( browser->txt_title, TEXT_NO_SELECTED_ITEMS );
	}
}

static void FileBrowser_DisableSelectionMode( FileBrowser browser )
{
	FileBrowser_UpdateSelectionUI( browser );
	Widget_RemoveClass( browser->view, "selection-mode" );
	TextView_SetTextW( browser->txt_title, browser->title );
	browser->is_selection_mode = FALSE;
}

static void FileBrowser_EnableSelectionMode( FileBrowser browser )
{
	FileBrowser_UpdateSelectionUI( browser );
	browser->is_selection_mode = TRUE;
	Widget_AddClass( browser->view, "selection-mode" );
}

static void FileBrowser_SelectItem( FileBrowser browser, FileIndex fidx )
{
	if( !browser->is_selection_mode ) {
		FileBrowser_EnableSelectionMode( browser );
	}
	Widget_AddClass( fidx->item, "selected" );
	Widget_AddClass( fidx->checkbox, "mdi-check" );
	LinkedList_Append( &browser->selected_files, fidx );
	FileBrowser_UpdateSelectionUI( browser );
}

static void FileBrowser_UnselectItem( FileBrowser browser, FileIndex fidx )
{
	LinkedListNode *node;
	for( LinkedList_Each(node, &browser->selected_files) ) {
		if( node->data != fidx ) {
			continue;
		}
		Widget_RemoveClass( fidx->item, "selected" );
		Widget_RemoveClass( fidx->checkbox, "mdi-check" );
		LinkedList_DeleteNode( &browser->selected_files, node );
		break;
	}
	FileBrowser_UpdateSelectionUI( browser );
}

static void FileBrowser_UnselectAllItems( FileBrowser browser )
{
	LinkedListNode *node;
	for( LinkedList_Each(node, &browser->selected_files) ) {
		FileIndex fidx = node->data;
		Widget_RemoveClass( fidx->item, "selected" );
		Widget_RemoveClass( fidx->checkbox, "mdi-check" );
	}
	LinkedList_Clear( &browser->selected_files, NULL );
	FileBrowser_UpdateSelectionUI( browser );
}

static LCUI_BOOL CheckTagName( const wchar_t *tagname )
{
	if( wgetcharcount( tagname, L",;\n\r\t" ) > 0 ) {
		return FALSE;
	}
	if( wcslen( tagname ) >= MAX_TAG_LEN ) {
		return FALSE;
	}
	return TRUE;
}

static int OnFileDeleted( void *privdata, int i, int n )
{
	wchar_t text[256];
	DialogDataPack pack = privdata;
	swprintf( text, 255, TEXT_DELETION_PROGRESS, i, n );
	TextView_SetTextW( pack->dialog->content, text );
	ProgressBar_SetValue( pack->dialog->progress, i );
	return  pack->active ? 0 : -1;
}

static void FileDeletionThread( void *arg )
{
	int i, n;
	FileIndex fidx;
	char **filepaths;
	LinkedList *files;
	LinkedListNode *node;
	LinkedList deleted_files;
	LCUI_Widget cursor = NULL;
	DialogDataPack pack = arg;
	LinkedList_Init( &deleted_files );
	files = &pack->browser->selected_files;
	n = pack->browser->selected_files.length;
	filepaths = malloc( sizeof( char* ) * n );
	ProgressBar_SetMaxValue( pack->dialog->progress, n );
	/* 先禁用缩略图滚动加载，避免滚动加载功能访问已删除的部件 */
	ThumbView_DisableScrollLoading( pack->browser->items );
	for( i = 0; pack->active && i < n; ++i ) {
		node = LinkedList_GetNode( files, 0 );
		fidx = node->data;
		LinkedList_Unlink( files, node );
		LinkedList_AppendNode( &deleted_files, node );
		filepaths[i] = fidx->file->path;
		/* 找到排列在最前面的部件，缩略图列表视图的重新布局需要用到 */
		if( !cursor || cursor->index > fidx->item->index  ) {
			cursor = fidx->item;
		}
	}
	LCFinder_DeleteFiles( filepaths, n, OnFileDeleted, pack );
	free( filepaths );
	if( cursor->index == 0 ) {
		cursor = NULL;
	} else {
		node = Widget_GetNode( cursor );
		cursor = node->prev->data;
	}
	Widget_SetDisabled( pack->dialog->btn_cancel, TRUE );
	for( LinkedList_Each( node, &deleted_files ) ) {
		fidx = node->data;
		Widget_Destroy( fidx->item );
		LinkedList_Unlink( &pack->browser->files, &fidx->node );
		FileIndex_Delete( fidx );
	}
	ThumbView_EnableScrollLoading( pack->browser->items );
	/** 以 cursor 为基点，对它后面的部件重新布局 */
	ThumbView_UpdateLayout( pack->browser->items, cursor );
	FileBrowser_UnselectAllItems( pack->browser );
	FileBrowser_DisableSelectionMode( pack->browser );
	CloseProgressDialog( pack->dialog );
	LCUIThread_Exit( NULL );
}

static void FileTagAddtionThread( void *arg )
{
	int i, j, n;
	wchar_t text[256];
	LinkedList *files;
	LinkedListNode *node;
	DialogDataPack pack = arg;
	files = &pack->browser->selected_files;
	n = pack->browser->selected_files.length;
	ProgressBar_SetMaxValue( pack->dialog->progress, n );
	node = LinkedList_GetNode( files, 0 );
	for( i = 0; pack->active && i < n; ++i ) {
		FileIndex fidx = node->data;
		for( j = 0; pack->tagnames[j]; ++j ) {
			const char *tagname = pack->tagnames[j];
			LCFinder_AddTagForFile( fidx->file, tagname );
		}
		swprintf( text, 255, TEXT_TAG_ADDTION_PROGRESS, i, n );
		TextView_SetTextW( pack->dialog->content, text );
		ProgressBar_SetValue( pack->dialog->progress, i );
		node = node->next;
	}
	Widget_SetDisabled( pack->dialog->btn_cancel, TRUE );
	FileBrowser_UnselectAllItems( pack->browser );
	FileBrowser_DisableSelectionMode( pack->browser );
	CloseProgressDialog( pack->dialog );
	LCUIThread_Exit( NULL );
}

static void OnCancelProcessing( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	DialogDataPack pack = e->data;
	pack->active = FALSE;
	Widget_SetDisabled( w, TRUE );
	LCUIThread_Join( pack->thread, NULL );
}

static void OnBtnTagClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	int len;
	char *buf, **tagnames;
	DialogDataPackRec pack;
	wchar_t text[MAX_TAG_LEN];
	LCUI_Widget window = LCUIWidget_GetById( ID_WINDOW_MAIN );
	if( 0 != LCUIDialog_Prompt( window, DIALOG_TITLE_ADD_TAG,
				    DIALOG_PLACEHOLDER_ADD_TAG, NULL,
				    text, MAX_TAG_LEN - 1, CheckTagName ) ) {
		return;
	}
	len = LCUI_EncodeString( NULL, text, 0, ENCODING_UTF8 ) + 1;
	buf = NEW( char, len );
	LCUI_EncodeString( buf, text, len, ENCODING_UTF8 );
	strsplit( buf, " ", &tagnames );
	pack.active = TRUE;
	pack.browser = e->data;
	pack.tagnames = tagnames;
	pack.dialog = NewProgressDialog();
	Button_SetTextW( pack.dialog->btn_cancel, TEXT_CANCEL );
	TextView_SetTextW( pack.dialog->title, TEXT_TAG_ADDING );
	Widget_BindEvent( pack.dialog->btn_cancel, "click", 
			  OnCancelProcessing, &pack, NULL );
	LCUIThread_Create( &pack.thread, FileTagAddtionThread, &pack );
	OpenProgressDialog( pack.dialog, window );
	freestrs( tagnames );
	free( buf );
}

static void OnBtnDeleteClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	wchar_t text[512];
	DialogDataPackRec pack;
	FileBrowser fb = e->data;
	LCUI_Widget window = LCUIWidget_GetById( ID_WINDOW_MAIN );
	swprintf( text, 511, DIALOG_TEXT_DELETE, fb->selected_files.length );
	if( !LCUIDialog_Confirm(window, DIALOG_TITLE_DELETE, text) ) {
		return;
	}
	pack.browser = fb;
	pack.active = TRUE;
	pack.dialog = NewProgressDialog();
	Button_SetTextW( pack.dialog->btn_cancel, TEXT_CANCEL );
	TextView_SetTextW( pack.dialog->title, TEXT_DELETING );
	Widget_BindEvent( pack.dialog->btn_cancel, "click", 
			  OnCancelProcessing, &pack, NULL );
	LCUIThread_Create( &pack.thread, FileDeletionThread, &pack );
	OpenProgressDialog( pack.dialog, window );
}

static void OnBtnSelectionClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	FileBrowser_EnableSelectionMode( e->data );
}

static void OnBtnCancelClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	FileBrowser_UnselectAllItems( e->data );
	FileBrowser_DisableSelectionMode( e->data );
}

static void OnItemClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	FileIterator iter;
	DataPack data = e->data;
	if( !data->fidx->is_file ) {
		return;
	}
	if( !data->browser->is_selection_mode &&
	    e->target != data->fidx->checkbox ) {
		iter = FileIterator_Create( data->browser, data->fidx );
		UI_OpenPictureView( data->fidx->file->path );
		UI_SetPictureView( iter );
		return;
	}
	if( Widget_HasClass( data->fidx->item, "selected" ) ) {
		FileBrowser_UnselectItem( data->browser, data->fidx );
	} else {
		FileBrowser_SelectItem( data->browser, data->fidx );
	}
}

void FileBrowser_Empty( FileBrowser browser )
{
	Widget_Hide( browser->btn_select );
	FileBrowser_UnselectAllItems( browser );
	FileBrowser_DisableSelectionMode( browser );
	ThumbView_Lock( browser->items );
	ThumbView_Empty( browser->items );
	Dict_Empty( browser->file_indexes );
	LinkedList_ClearData( &browser->dirs, (FuncPtr)FileIndex_Delete );
	LinkedList_ClearData( &browser->files, (FuncPtr)FileIndex_Delete );
	ThumbView_Unlock( browser->items );
}

void FileBrowser_Append( FileBrowser browser, LCUI_Widget widget )
{
	ThumbView_Append( browser->items, widget );
}

LCUI_Widget FileBrowser_AppendPicture( FileBrowser browser, DB_File file )
{
	DataPack data;
	LCUI_Widget item;

	data = NEW( DataPackRec, 1 );
	data->fidx = NEW( FileIndexRec, 1 );
	item = ThumbView_AppendPicture( browser->items, file );
	data->fidx->is_file = TRUE;
	data->browser = browser;
	data->fidx->item = item;
	data->fidx->file = file;
	data->fidx->node.data = data->fidx;
	data->fidx->checkbox = LCUIWidget_New( "textview" );
	Widget_AddClass( data->fidx->checkbox, "checkbox mdi" );
	ThumbViewItem_AppendToCover( item, data->fidx->checkbox );
	Dict_Add( browser->file_indexes, data->fidx->file->path, data->fidx );
	LinkedList_AppendNode( &browser->files, &data->fidx->node );
	Widget_BindEvent( item, "click", OnItemClick, data, NULL );
	Widget_Show( browser->btn_select );
	return item;
}

LCUI_Widget FileBrowser_AppendFolder( FileBrowser browser, const char *path, 
				      LCUI_BOOL show_path )
{
	size_t len;
	DataPack data;
	LCUI_Widget item;
	len = strlen( path ) + 1;

	data = NEW( DataPackRec, 1 );
	data->fidx = NEW( FileIndexRec, 1 );
	item = ThumbView_AppendFolder( browser->items, path, show_path );
	data->fidx->is_file = FALSE;
	data->browser = browser;
	data->fidx->item = item;
	data->fidx->checkbox = NULL;
	data->fidx->node.data = data->fidx;
	data->fidx->path = malloc( sizeof( char )*len );
	strncpy( data->fidx->path, path, len );
	Dict_Add( browser->file_indexes, data->fidx->path, data->fidx );
	LinkedList_AppendNode( &browser->dirs, &data->fidx->node );
	Widget_BindEvent( item, "click", OnItemClick, data, NULL );
	return item;
}

#define BindEvent(BTN, EVENT, CALLBACK) \
	Widget_BindEvent( browser->btn_##BTN, EVENT, CALLBACK, browser, NULL )

void FileBrowser_Create( FileBrowser browser )
{
	browser->is_selection_mode = FALSE;
	BindEvent( tag, "click", OnBtnTagClick );
	BindEvent( cancel, "click", OnBtnCancelClick );
	BindEvent( delete, "click", OnBtnDeleteClick );
	BindEvent( select, "click", OnBtnSelectionClick );
	LinkedList_Init( &browser->dirs );
	LinkedList_Init( &browser->files );
	LinkedList_Init( &browser->selected_files );
	browser->file_indexes = StrDict_Create( NULL, NULL );
	Widget_Hide( browser->btn_select );
}
