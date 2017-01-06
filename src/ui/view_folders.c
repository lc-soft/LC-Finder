/* ***************************************************************************
 * view_home.c -- folders view
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
 * view_home.c -- "文件夹" 视图
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
#include "thumbview.h"
#include "textview_i18n.h"
#include "dropdown.h"
#include "browser.h"
#include "i18n.h"

#define KEY_SORT_HEADER		"sort.header"
#define KEY_TITLE		"folders.title"
#define THUMB_CACHE_SIZE	(20*1024*1024)

/** 视图同步功能的相关数据 */
typedef struct ViewSyncRec_ {
	LCUI_Thread tid;
	LCUI_BOOL is_running;
	LCUI_Mutex mutex;
	LCUI_Cond ready;
	int prev_item_type;
} ViewSyncRec, *ViewSync;

/** 文件扫描功能的相关数据 */
typedef struct FileScannerRec_ {
	LCUI_Thread tid;
	LCUI_Cond cond;
	LCUI_Cond cond_scan;
	LCUI_Mutex mutex;
	LCUI_Mutex mutex_scan;
	LCUI_BOOL is_running;
	LCUI_BOOL is_async_scaning;
	LinkedList files;
	wchar_t *dirpath;
} FileScannerRec, *FileScanner;

typedef struct FileEntryRec_ {
	LCUI_BOOL is_dir;
	DB_File file;
	char *path;
} FileEntryRec, *FileEntry;

static struct FoldersViewData {
	DB_Dir dir;
	LCUI_Widget view;
	LCUI_Widget items;
	LCUI_Widget info;
	LCUI_Widget info_name;
	LCUI_Widget info_path;
	LCUI_Widget tip_empty;
	LCUI_Widget selected_sort;
	LCUI_BOOL show_private_folders;
	LCUI_BOOL folders_changed;
	ViewSyncRec viewsync;
	FileScannerRec scanner;
	FileBrowserRec browser;
	DB_QueryTermsRec terms;
	char *dirpath;
} this_view = { 0 };

#define SORT_METHODS_LEN 6

static FileSortMethodRec sort_methods[SORT_METHODS_LEN] = {
	{"sort.ctime_desc", CREATE_TIME_DESC},
	{"sort.ctime_asc", CREATE_TIME_ASC},
	{"sort.mtime_desc", MODIFY_TIME_DESC},
	{"sort.mtime_asc", MODIFY_TIME_ASC},
	{"sort.score_desc", SCORE_DESC},
	{"sort.score_asc", SCORE_ASC}
};

static void OpenFolder( const char *dirpath );

static void OnAddDir( void *privdata, void *data )
{
	DB_Dir dir = data;
	_DEBUG_MSG("add dir: %s\n", dir->path);
	if( !this_view.dir ) {
		this_view.folders_changed = TRUE;
	}
}

static void OnFolderChange( void *privdata, void *data )
{
	this_view.folders_changed = TRUE;
}

static void OnViewShow( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.show_private_folders == finder.open_private_space &&
	    !this_view.folders_changed ) {
		return;
	}
	this_view.show_private_folders = finder.open_private_space;
	this_view.folders_changed = FALSE;
	OpenFolder( NULL );
}

static void OnBtnSyncClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	LCFinder_TriggerEvent( EVENT_SYNC, NULL );
}

static void OnBtnReturnClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	OpenFolder( NULL );
}

static void OnDeleteFileEntry( void *arg )
{
	FileEntry entry = arg;
	if( entry->is_dir ) {
		free( entry->path );
	} else {
		free( entry->file->path );
		free( entry->file );
	}
	free( entry );
}

static void FileScanner_OnGetDirs( FileStatus *status,
				   FileStream stream, void *data )
{
	char *dirpath;
	size_t len, dirpath_len;
	FileScanner scanner = data;
	FileEntry file_entry;

	LCUIMutex_Lock( &scanner->mutex_scan );
	if( !status || !stream ) {
		scanner->is_async_scaning = FALSE;
		LCUICond_Signal( &scanner->cond_scan );
		LCUIMutex_Unlock( &scanner->mutex_scan );
		return;
	}
	dirpath = EncodeUTF8( scanner->dirpath );
	dirpath_len = strlen( dirpath );
	while( 1 ) {
		char *p, buf[PATH_LEN];
		p = FileStream_ReadLine( stream, buf, PATH_LEN );
		if( !p ) {
			break;
		}
		if( buf[0] != 'd' ) {
			continue;
		}
		len = strlen( buf );
		if( len < 2 ) {
			continue;
		}
		buf[len - 1] = 0;
		file_entry = NEW( FileEntryRec, 1 );
		file_entry->is_dir = TRUE;
		file_entry->file = NULL;
		len = len + dirpath_len;
		file_entry->path = malloc( sizeof( char ) * len );
		pathjoin( file_entry->path, dirpath, buf + 1 );
		LCUIMutex_Lock( &scanner->mutex );
		LinkedList_Append( &scanner->files, file_entry );
		LCUICond_Signal( &scanner->cond );
		LCUIMutex_Unlock( &scanner->mutex );
	}
	scanner->is_async_scaning = FALSE;
	LCUICond_Signal( &scanner->cond_scan );
	LCUIMutex_Unlock( &scanner->mutex_scan );
}

static void FileScanner_ScanDirs( FileScanner scanner )
{
	LCUIMutex_Lock( &scanner->mutex_scan );
	scanner->is_async_scaning = TRUE;
	FileStorage_GetFolders( finder.storage_for_scan, scanner->dirpath,
				FileScanner_OnGetDirs, scanner );
	while( scanner->is_async_scaning ) {
		LCUICond_Wait( &scanner->cond_scan, &scanner->mutex_scan );
	}
	LCUIMutex_Unlock( &scanner->mutex_scan );
}

static void FileScanner_ScanFiles( FileScanner scanner )
{
	DB_File file;
	DB_Query query;
	FileEntry entry;
	int i, count;
	this_view.terms.limit = 50;
	this_view.terms.offset = 0;
	this_view.terms.dirpath = EncodeUTF8( scanner->dirpath );
	query = DB_NewQuery( &this_view.terms );
	count = DBQuery_GetTotalFiles( query );
	while( scanner->is_running && count > 0 ) {
		DB_DeleteQuery( query );
		query = DB_NewQuery( &this_view.terms );
		if(count < this_view.terms.limit) {
			i = count;
		} else {
			i = this_view.terms.limit;
		}
		for( ; scanner->is_running && i > 0; --i ) {
			file = DBQuery_FetchFile( query );
			if( !file ) {
				break;
			}
			entry = NEW( FileEntryRec, 1 );
			entry->is_dir = FALSE;
			entry->file = file;
			entry->path = file->path;
			DEBUG_MSG("file: %s\n", file->path);
			LCUIMutex_Lock( &scanner->mutex );
			LinkedList_Append( &scanner->files, entry );
			LCUICond_Signal( &scanner->cond );
			LCUIMutex_Unlock( &scanner->mutex );
		}
		count -= this_view.terms.limit;
		this_view.terms.offset += this_view.terms.limit;
	}
	free( this_view.terms.dirpath );
}

static int FileScanner_LoadSourceDirs( FileScanner scanner )
{
	int i, n;
	DB_Dir *dirs;
	LCUIMutex_Lock( &scanner->mutex );
	n = LCFinder_GetSourceDirList( &dirs );
	for( i = 0; i < n; ++i ) {
		FileEntry entry = NEW( FileEntryRec, 1 );
		int len = strlen( dirs[i]->path ) + 1;
		char *path = malloc( sizeof( char ) * len );
		strcpy( path, dirs[i]->path );
		entry->path = path;
		entry->is_dir = TRUE;
		LinkedList_Append( &scanner->files, entry );
	}
	LCUICond_Signal( &scanner->cond );
	LCUIMutex_Unlock( &scanner->mutex );
	free( dirs );
	return n;
}

/** 初始化文件扫描 */
static void FileScanner_Init( FileScanner scanner )
{
	LCUICond_Init( &scanner->cond );
	LCUICond_Init( &scanner->cond_scan );
	LCUIMutex_Init( &scanner->mutex );
	LCUIMutex_Init( &scanner->mutex_scan );
	LinkedList_Init( &scanner->files );
	scanner->is_async_scaning = FALSE;
	scanner->is_running = FALSE;
	scanner->dirpath = NULL;
}

/** 重置文件扫描 */
static void FileScanner_Reset( FileScanner scanner )
{
	if( scanner->is_running ) {
		scanner->is_running = FALSE;
		LCUIThread_Join( scanner->tid, NULL );
	}
	LCUIMutex_Lock( &scanner->mutex );
	LinkedList_Clear( &scanner->files, OnDeleteFileEntry );
	LCUICond_Signal( &scanner->cond );
	LCUIMutex_Unlock( &scanner->mutex );
}

static void FileScanner_Destroy( FileScanner scanner )
{
	FileScanner_Reset( scanner );
	LCUICond_Destroy( &scanner->cond );
	LCUICond_Destroy( &scanner->cond_scan );
	LCUIMutex_Destroy( &scanner->mutex );
	LCUIMutex_Destroy( &scanner->mutex_scan );
}

static void FileScanner_Thread( void *arg )
{
	FileScanner scanner = arg;
	scanner->is_running = TRUE;
	if( scanner->dirpath ) {
		FileScanner_ScanDirs( scanner );
		FileScanner_ScanFiles( scanner );
	} else {
		FileScanner_LoadSourceDirs( scanner );
	}
	DEBUG_MSG("scan files: %d\n", count);
	if( scanner->files.length > 0 ) {
		Widget_AddClass( this_view.tip_empty, "hide" );
		Widget_Hide( this_view.tip_empty );
	} else {
		Widget_RemoveClass( this_view.tip_empty, "hide" );
		Widget_Show( this_view.tip_empty );
	}
	scanner->is_running = FALSE;
	LCUIThread_Exit( NULL );
}

/** 开始扫描 */
static void FileScanner_Start( FileScanner scanner, char *path )
{
	if( scanner->dirpath ) {
		free( scanner->dirpath );
	}
	if( path ) {
		scanner->dirpath = DecodeUTF8( path );
	} else {
		scanner->dirpath = NULL;
	}
	LCUIThread_Create( &scanner->tid, FileScanner_Thread, scanner );
}

static void OnItemClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	FileEntry entry = e->data;
	DEBUG_MSG( "open file: %s\n", path );
	if( entry->is_dir ) {
		OpenFolder( entry->path );
	}
}

/** 在缩略图列表准备完毕的时候 */
static void OnThumbViewReady( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	LCUICond_Signal( &this_view.viewsync.ready );
}

/** 视图同步线程 */
static void ViewSync_Thread( void *arg )
{
	ViewSync vs;
	LCUI_Widget item;
	FileEntry entry;
	FileScanner scanner;
	LinkedListNode *node;
	vs = &this_view.viewsync;
	scanner = &this_view.scanner;
	LCUIMutex_Lock( &vs->mutex );
	/* 等待缩略图列表部件准备完毕 */
	while( this_view.items->state < WSTATE_READY ) {
		LCUICond_TimedWait( &vs->ready, &vs->mutex, 100 );
	}
	LCUIMutex_Unlock( &vs->mutex );
	vs->prev_item_type = -1;
	vs->is_running = TRUE;
	while( vs->is_running ) {
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
		entry = node->data;
		LinkedList_Unlink( &scanner->files, node );
		LCUIMutex_Unlock( &scanner->mutex );
		if( vs->prev_item_type != -1 && 
		    vs->prev_item_type != entry->is_dir ) {
			LCUI_Widget separator = LCUIWidget_New(NULL);
			Widget_AddClass( separator, "divider" );
			FileBrowser_Append( &this_view.browser, separator );
		}
		vs->prev_item_type = entry->is_dir;
		if( entry->is_dir ) {
			item = FileBrowser_AppendFolder( 
				&this_view.browser, entry->path, 
				this_view.dir == NULL
			);
			Widget_BindEvent( item, "click", OnItemClick,
					  entry, NULL );
		} else {
			FileBrowser_AppendPicture( &this_view.browser, 
						   entry->file );
			free( node->data );
		}
		LinkedListNode_Delete( node );
		LCUIMutex_Unlock( &vs->mutex );
	}
	LCUIThread_Exit( NULL );
}

static void OpenFolder( const char *dirpath )
{
	DB_Dir dir = NULL, *dirs = NULL;
	char *path = NULL, *scan_path = NULL;

	if( dirpath ) {
		int i;
		int len = strlen( dirpath );
		int n = LCFinder_GetSourceDirList( &dirs );
		path = malloc( sizeof( char )*(len + 1) );
		scan_path = malloc( sizeof( char )*(len + 2) );
		strcpy( scan_path, dirpath );
		strcpy( path, dirpath );
		for( i = 0; i < n; ++i ) {
			if( dirs[i] && strstr( dirs[i]->path, path ) ) {
				dir = dirs[i];
				break;
			}
		}
		if( scan_path[len - 1] != PATH_SEP ) {
			scan_path[len++] = PATH_SEP;
			scan_path[len] = 0;
		} else {
			path[len] = 0;
		}
		TextView_SetText( this_view.info_name, getfilename( path ) );
		TextView_SetText( this_view.info_path, path );
		Widget_AddClass( this_view.view, "show-folder-info-box" );
		free( dirs );
	} else {
		Widget_RemoveClass( this_view.view, "show-folder-info-box" );
	}
	if( this_view.dirpath ) {
		free( this_view.dirpath );
		this_view.dirpath = NULL;
	}
	FileScanner_Reset( &this_view.scanner );
	LCUIMutex_Lock( &this_view.viewsync.mutex );
	this_view.dir = dir;
	this_view.dirpath = path;
	this_view.viewsync.prev_item_type = -1;
	FileBrowser_Empty( &this_view.browser );
	FileScanner_Start( &this_view.scanner, path );
	LCUIMutex_Unlock( &this_view.viewsync.mutex );
	DEBUG_MSG("done\n");
}

static void OnSyncDone( void *privdata, void *arg )
{
	OpenFolder( NULL );
}

static void UpdateQueryTerms( void )
{
	this_view.terms.modify_time = NONE;
	this_view.terms.create_time = NONE;
	this_view.terms.score = NONE;
	switch( finder.config.files_sort ) {
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
	if( finder.config.files_sort == sort->value ) {
		return;
	}
	finder.config.files_sort = sort->value;
	if( this_view.selected_sort ) {
		Widget_RemoveClass( this_view.selected_sort, "active" );
	}
	this_view.selected_sort = e->target;
	Widget_AddClass( e->target, "active" );
	UpdateQueryTerms();
	OpenFolder( this_view.dirpath );
	LCFinder_SaveConfig();
}

static void InitFolderFilesSort( void )
{
	int i;
	LCUI_Widget menu, item, icon, text;
	const wchar_t *header = I18n_GetText( KEY_SORT_HEADER );
	SelectWidget( menu, ID_DROPDOWN_FOLDER_FILES_SORT );
	Widget_Empty( menu );
	Dropdown_SetHeaderW( menu, header );
	for( i = 0; i < SORT_METHODS_LEN; ++i ) {
		FileSortMethod sort = &sort_methods[i];
		item = Dropdown_AddItem( menu, sort );
		icon = LCUIWidget_New( "textview" );
		text = LCUIWidget_New( "textview-i18n" );
		TextViewI18n_SetKey( text, sort->name_key );
		Widget_AddClass( icon, "icon mdi mdi-check" );
		Widget_AddClass( text, "text" );
		Widget_Append( item, icon );
		Widget_Append( item, text );
		if( sort->value == finder.config.files_sort ) {
			Widget_AddClass( item, "active" );
			this_view.selected_sort = item;
		}
	}
	
	BindEvent( menu, "change.dropdown", OnSelectSortMethod );
	UpdateQueryTerms();
}

static void OnLanguageChanged( void *privdata, void *data )
{
	LCUI_Widget menu;
	const wchar_t *header;
	header = I18n_GetText( KEY_SORT_HEADER );
	SelectWidget( menu, ID_DROPDOWN_FOLDER_FILES_SORT );
	Dropdown_SetHeaderW( menu, header );
}

void UI_InitFoldersView( void )
{
	LCUI_Widget btn[6], btn_return, title;
	FileScanner_Init( &this_view.scanner );
	LCUICond_Init( &this_view.viewsync.ready );
	LCUIMutex_Init( &this_view.viewsync.mutex );
	SelectWidget( this_view.items, ID_VIEW_FILE_LIST );
	SelectWidget( title, ID_TXT_VIEW_FOLDERS_TITLE );
	SelectWidget( btn[0], ID_BTN_SYNC_FOLDER_FILES );
	SelectWidget( btn[1], ID_BTN_SELECT_FOLDER_FILES );
	SelectWidget( btn[2], ID_BTN_CANCEL_FOLDER_SELECT );
	SelectWidget( btn[3], ID_BTN_TAG_FOLDER_FILES );
	SelectWidget( btn[4], ID_BTN_DELETE_FOLDER_FILES );
	SelectWidget( btn[5], ID_BTN_FOLDER_FILES_SORT );
	SelectWidget( btn_return, ID_BTN_RETURN_ROOT_FOLDER );
	SelectWidget( this_view.view, ID_VIEW_FOLDERS );
	SelectWidget( this_view.info, ID_VIEW_FOLDER_INFO );
	SelectWidget( this_view.info_name, ID_VIEW_FOLDER_INFO_NAME );
	SelectWidget( this_view.info_path, ID_VIEW_FOLDER_INFO_PATH );
	SelectWidget( this_view.tip_empty, ID_TIP_FOLDERS_EMPTY );
	this_view.browser.btn_tag = btn[3];
	this_view.browser.btn_select = btn[1];
	this_view.browser.btn_cancel = btn[2];
	this_view.browser.btn_delete = btn[4];
	this_view.browser.btn_sort = btn[5];
	this_view.browser.txt_title = title;
	this_view.browser.title_key = KEY_TITLE;
	this_view.browser.view = this_view.view;
	this_view.browser.items = this_view.items;
	BindEvent( btn[0], "click", OnBtnSyncClick );
	BindEvent( btn_return, "click", OnBtnReturnClick );
	BindEvent( this_view.items, "ready", OnThumbViewReady );
	BindEvent( this_view.view, "show.view", OnViewShow );
	ThumbView_SetCache( this_view.items, finder.thumb_cache );
	ThumbView_SetStorage( this_view.items, finder.storage_for_thumb );
	LCUIThread_Create( &this_view.viewsync.tid, ViewSync_Thread, NULL );
	LCFinder_BindEvent( EVENT_SYNC_DONE, OnSyncDone, NULL );
	LCFinder_BindEvent( EVENT_DIR_ADD, OnAddDir, NULL );
	LCFinder_BindEvent( EVENT_DIR_DEL, OnFolderChange, NULL );
	LCFinder_BindEvent( EVENT_LANG_CHG, OnLanguageChanged, NULL );
	FileBrowser_Create( &this_view.browser );
	InitFolderFilesSort();
	OpenFolder( NULL );
}

void UI_ExitFolderView( void )
{
	this_view.viewsync.is_running = FALSE;
	FileScanner_Destroy( &this_view.scanner );
	LCUIThread_Join( this_view.viewsync.tid, NULL );
	LCUIMutex_Unlock( &this_view.viewsync.mutex );
}
