/* ***************************************************************************
 * view_home.c -- home view
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
 * view_home.c -- 主页"集锦"视图
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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "finder.h"
#include "file_storage.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/widget/button.h>
#include "thumbview.h"
#include "progressbar.h"
#include "timeseparator.h"
#include "browser.h"

#define KEY_TITLE		"home.title"
#define TEXT_TIME_TITLE		L"%d年%d月"

/* 延时隐藏进度条 */
#define HideProgressBar() LCUITimer_Set( 1000, (FuncPtr)Widget_Hide, \
					 this_view.progressbar, FALSE )

/** 文件扫描功能的相关数据 */
typedef struct FileScannerRec_ {
	LCUI_Thread tid;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	LCUI_BOOL is_running;
	LinkedList files;
	int count, total;
} FileScannerRec, *FileScanner;

/** 视图同步功能的相关数据 */
typedef struct ViewSyncRec_ {
	LCUI_Thread tid;
	LCUI_BOOL is_running;
	LCUI_Mutex mutex;
	LCUI_Cond ready;
} ViewSyncRec, *ViewSync;

/** 主页集锦视图的相关数据 */
static struct HomeCollectionView {
	LCUI_BOOL is_activated;
	LCUI_Widget view;
	LCUI_Widget items;
	LCUI_Widget time_ranges;
	LCUI_Widget selected_time;
	LCUI_Widget info_path;
	LCUI_Widget tip_empty;
	LCUI_Widget progressbar;
	LCUI_BOOL show_private_files;
	ViewSyncRec viewsync;
	FileScannerRec scanner;
	LinkedList separators;
	FileBrowserRec browser;
} this_view;

static void OnDeleteDBFile( void *arg )
{
	DBFile_Release( arg );
}

static void OnBtnSyncClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	LCFinder_TriggerEvent( EVENT_SYNC, NULL );
}

static void DeleteTimeSeparator( LCUI_Widget sep )
{
	LinkedListNode *node;
	for( LinkedList_Each( node, &this_view.separators ) ) {
		if( node->data == sep ) {
			LinkedList_Unlink( &this_view.separators, node );
			LinkedListNode_Delete( node );
			break;
		}
	}
	Widget_Destroy( sep );
}

static void OnAfterDeleted( LCUI_Widget first )
{
	int count;
	LCUI_Widget sep = NULL, w = first;
	while( w ) {
		if( w->type && strcmp( w->type, "time-separator" ) == 0 ) {
			sep = w;
			break;
		}
		w = Widget_GetPrev( w );
	}
	if( !sep ) {
		sep = LinkedList_Get( &this_view.separators, 0 );
	}
	if( !sep ) {
		Widget_RemoveClass( this_view.tip_empty, "hide" );
		Widget_Show( this_view.tip_empty );
		return;
	}
	w = Widget_GetNext( sep );
	TimeSeparator_Reset( sep );
	for( count = 0; w; w = Widget_GetNext( w ) ) {
		if( !w->type ) {
			continue;
		}
		if( strcmp( w->type, "thumbviewitem" ) == 0 ) {
			time_t time;
			struct tm *t;
			DB_File file;
			file = ThumbViewItem_GetFile( w );
			time = file->create_time;
			t = localtime( &time );
			TimeSeparator_AddTime( sep, t );
			count += 1;
		} else if( strcmp( w->type, "time-separator" ) == 0 ) {
			/* 如果该时间分割器下一个文件都没有 */
			if( count == 0 ) {
				DeleteTimeSeparator( sep );
			}
			count = 0, sep = w;
			TimeSeparator_Reset( sep );
		}
	}
	/* 处理最后一个没有文件的时间分割器 */
	if( sep && count == 0 ) {
		DeleteTimeSeparator( sep );
	}
	/* 如果时间分割器数量为0，则说明当前缩略图列表为空 */
	if( this_view.separators.length < 1 ) {
		Widget_RemoveClass( this_view.tip_empty, "hide" );
		Widget_Show( this_view.tip_empty );
	}
}

/** 扫描全部文件 */
static int FileScanner_ScanAll( FileScanner scanner )
{
	DB_File file;
	DB_Query query;
	int i, total, count;
	DB_QueryTermsRec terms = { 0 };

	terms.limit = 100;
	terms.modify_time = DESC;
	if( terms.dirs ) {
		free( terms.dirs );
		terms.dirs = NULL;
		terms.n_dirs = 0;
	}
	terms.n_dirs = LCFinder_GetSourceDirList( &terms.dirs );
	if( terms.n_dirs == finder.n_dirs ) {
		free( terms.dirs );
		terms.dirs = NULL;
		terms.n_dirs = 0;
	}
	query = DB_NewQuery( &terms );
	count = total = DBQuery_GetTotalFiles( query );
	scanner->total = total, scanner->count = 0;
	ProgressBar_SetValue( this_view.progressbar, 0 );
	ProgressBar_SetMaxValue( this_view.progressbar, count );
	Widget_Show( this_view.progressbar );
	_DEBUG_MSG("total: %d\n", count);
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
	if( terms.dirs ) {
		free( terms.dirs );
		terms.dirs = NULL;
	}
	return total;
}

/** 初始化文件扫描 */
static void FileScanner_Init( FileScanner scanner )
{
	LCUICond_Init( &scanner->cond );
	LCUIMutex_Init( &scanner->mutex );
	LinkedList_Init( &scanner->files );
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
		Widget_AddClass( this_view.tip_empty, "hide" );
		Widget_Hide( this_view.tip_empty );
	} else {
		Widget_RemoveClass( this_view.tip_empty, "hide" );
		Widget_Show( this_view.tip_empty );
	}
	this_view.scanner.is_running = FALSE;
	_DEBUG_MSG("total files: %d\n", count);
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

static void OnTimeRangeClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	LCUI_Widget title = e->data;
	FileBrowser_SetScroll( &this_view.browser, (int)title->box.canvas.y );
	FileBrowser_SetButtonsDisabled( &this_view.browser, FALSE );
	Widget_Hide( this_view.time_ranges->parent->parent );
}

static void OnTimeTitleClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.selected_time ) {
		Widget_RemoveClass( this_view.selected_time, "selected" );
	}
	this_view.selected_time = e->data;
	Widget_AddClass( e->data, "selected" );
	FileBrowser_SetButtonsDisabled( &this_view.browser, TRUE );
	Widget_Show( this_view.time_ranges->parent->parent );
}

/** 向视图追加文件 */
static void HomeView_AppendFile( DB_File file )
{
	time_t time;
	struct tm *t;
	wchar_t title[128];
	LCUI_Widget sep, range;

	time = file->modify_time;
	t = localtime( &time );
	sep = LinkedList_Get( &this_view.separators, -1 );
	/* 如果当前文件的创建时间超出当前时间段，则新建分割线 */
	if( !sep || !TimeSeparator_CheckTime(sep, t) ) {
		range = LCUIWidget_New( "textview" );
		sep = LCUIWidget_New( "time-separator" );
		swprintf( title, 128, TEXT_TIME_TITLE,
			  1900 + t->tm_year, t->tm_mon + 1 );
		TimeSeparator_SetTime( sep, t );
		TextView_SetTextW( range, title );
		FileBrowser_Append( &this_view.browser, sep );
		LinkedList_Append( &this_view.separators, sep );
		Widget_AddClass( range, "time-range btn btn-link" );
		Widget_Append( this_view.time_ranges, range );
		Widget_BindEvent( TimeSeparator_GetTitle( sep ), 
				  "click", OnTimeTitleClick, range, NULL );
		Widget_BindEvent( range, "click", OnTimeRangeClick,
				  sep, NULL );
	}
	TimeSeparator_AddTime( sep, t );
	FileBrowser_AppendPicture( &this_view.browser, file );
}

/** 视图同步线程 */
static void HomeView_SyncThread( void *arg )
{
	ViewSync vs;
	FileScanner scanner;
	LinkedListNode *node;
	vs = &this_view.viewsync;
	scanner = &this_view.scanner;
	LCUIMutex_Lock( &vs->mutex );
	/* 等待缩略图列表部件准备完毕 */
	while( this_view.items->state < LCUI_WSTATE_READY ) {
		LCUICond_TimedWait( &vs->ready, &vs->mutex, 100 );
	}
	LCUIMutex_Unlock( &vs->mutex );
	vs->is_running = TRUE;
	while( vs->is_running ) {
		LCUIMutex_Lock( &scanner->mutex );
		if( this_view.browser.files.length
		    >= this_view.scanner.total ) {
			HideProgressBar();
		}
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
		HomeView_AppendFile( node->data );
		LCUIMutex_Unlock( &vs->mutex );
		LinkedListNode_Delete( node );
		ProgressBar_SetValue( this_view.progressbar, 
				      this_view.browser.files.length );
	}
	LCUIMutex_Unlock( &scanner->mutex );
}

/** 载入集锦中的文件列表 */
static void LoadCollectionFiles( void )
{
	FileScanner_Reset( &this_view.scanner );
	LCUIMutex_Lock( &this_view.viewsync.mutex );
	LinkedList_Clear( &this_view.separators, NULL );
	Widget_Empty( this_view.time_ranges );
	FileBrowser_Empty( &this_view.browser );
	FileScanner_Start( &this_view.scanner );
	LCUIMutex_Unlock( &this_view.viewsync.mutex );
}

static void OnViewShow( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.show_private_files == finder.open_private_space ) {
		return;
	}
	this_view.show_private_files = finder.open_private_space;
	LoadCollectionFiles();
}

static void OnSyncDone( void *privdata, void *arg )
{
	LoadCollectionFiles();
}

void UI_InitHomeView( void )
{
	LCUI_Thread tid;
	LCUI_Widget btn[5], title;
	FileScanner_Init( &this_view.scanner );
	LCUICond_Init( &this_view.viewsync.ready );
	LCUIMutex_Init( &this_view.viewsync.mutex );
	SelectWidget( title, ID_TXT_VIEW_HOME_TITLE );
	SelectWidget( this_view.view, ID_VIEW_HOME );
	SelectWidget( this_view.items, ID_VIEW_HOME_COLLECTIONS );
	SelectWidget( this_view.time_ranges, ID_VIEW_TIME_RANGE_LIST );
	SelectWidget( this_view.progressbar, ID_VIEW_HOME_PROGRESS );
	SelectWidget( this_view.tip_empty, ID_TIP_HOME_EMPTY );
	SelectWidget( btn[0], ID_BTN_SYNC_HOME_FILES );
	SelectWidget( btn[1], ID_BTN_SELECT_HOME_FILES );
	SelectWidget( btn[2], ID_BTN_CANCEL_HOME_SELECT );
	SelectWidget( btn[3], ID_BTN_TAG_HOME_FILES );
	SelectWidget( btn[4], ID_BTN_DELETE_HOME_FILES );
	this_view.browser.title_key = KEY_TITLE;
	this_view.browser.btn_select = btn[1];
	this_view.browser.btn_cancel = btn[2];
	this_view.browser.btn_delete = btn[4];
	this_view.browser.btn_tag = btn[3];
	this_view.browser.txt_title = title;
	this_view.browser.view = this_view.view;
	this_view.browser.items = this_view.items;
	this_view.browser.after_deleted = OnAfterDeleted;
	FileBrowser_Create( &this_view.browser );
	ThumbView_SetCache( this_view.items, finder.thumb_cache );
	ThumbView_SetStorage( this_view.items, finder.storage_for_thumb );
	Widget_Hide( this_view.time_ranges->parent->parent );
	Widget_AddClass( this_view.time_ranges, "time-range-list" );
	LCFinder_BindEvent( EVENT_SYNC_DONE, OnSyncDone, NULL );
	LCUIThread_Create( &tid, HomeView_SyncThread, NULL );
	BindEvent( this_view.view, "show.view", OnViewShow );
	BindEvent( btn[0], "click", OnBtnSyncClick );
	this_view.viewsync.tid = tid;
	this_view.is_activated = TRUE;
	LoadCollectionFiles();
}

void UI_ExitHomeView( void )
{
	if( !this_view.is_activated ) {
		return;
	}
	this_view.viewsync.is_running = FALSE;
	FileScanner_Reset( &this_view.scanner );
	LCUIThread_Join( this_view.viewsync.tid, NULL );
	FileScanner_Destroy( &this_view.scanner );
	LCUICond_Destroy( &this_view.viewsync.ready );
	LCUIMutex_Destroy( &this_view.viewsync.mutex );
}
