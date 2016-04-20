/* ***************************************************************************
* view_home.c -- folders view
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
* view_home.c -- 文件夹视图
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
#include <stdio.h>
#include <string.h>
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "thumbview.h"

#define THUMB_CACHE_SIZE (20*1024*1024)

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
	LCUI_BOOL is_scaning;
	LCUI_Thread scanner_tid;
	LinkedList files;
	LinkedList files_cache[2];
	LinkedList *cache;
} this_view;

void OpenFolder( const char *dirpath );

static void OnAddDir( void *privdata, void *data )
{
	if( !this_view.dir ) {
		ThumbView_AppendFolder( this_view.items, data, TRUE );
	}
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

static int ScanDirs( char *path )
{
	char *name;
	LCUI_Dir dir;
	FileEntry file;
	wchar_t *wpath;
	LCUI_DirEntry *entry;
	int count, len, dirpath_len;

	count = 0;
	len = strlen( path );
	dirpath_len = len;
	wpath = malloc( sizeof(wchar_t) * (len + 1) );
	LCUI_DecodeString( wpath, path, len, ENCODING_UTF8 );
	LCUI_OpenDirW( wpath, &dir );
	while( (entry = LCUI_ReadDir( &dir )) && this_view.is_scaning ) {
		wchar_t *wname = LCUI_GetFileNameW( entry );
		/* 忽略 . 和 .. 文件夹 */
		if( wname[0] == L'.' ) {
			if( wname[1] == 0 || 
			    (wname[1] == L'.' && wname[2] == 0) ) {
				continue;
			}
		}
		if( !LCUI_FileIsDirectory( entry ) ) {
			continue;
		}
		file = NEW( FileEntryRec, 1 );
		file->is_dir = TRUE;
		file->file = NULL;
		len = LCUI_EncodeString( NULL, wname, 0, ENCODING_UTF8 ) + 1;
		name = malloc( sizeof(char) * len );
		file->path = malloc( sizeof( char ) * (len + dirpath_len) );
		LCUI_EncodeString( name, wname, len, ENCODING_UTF8 );
		sprintf( file->path, "%s%s", path, name );
		LinkedList_Append( this_view.cache, file );
		++count;
	}
	LCUI_CloseDir( &dir );
	return count;
}

static int ScanFiles( char *path )
{
	DB_File file;
	DB_Query query;
	FileEntry entry;
	int i, total, count;
	DB_QueryTermsRec terms;
	terms.dirpath = path;
	terms.n_dirs = 0;
	terms.n_tags = 0;
	terms.limit = 100;
	terms.offset = 0;
	terms.score = NONE;
	terms.tags = NULL;
	terms.dirs = NULL;
	terms.create_time = NONE;
	query = DB_NewQuery( &terms );
	count = total = DBQuery_GetTotalFiles( query );
	while( this_view.is_scaning && count > 0 ) {
		DB_DeleteQuery( query );
		query = DB_NewQuery( &terms );
		i = count < terms.limit ? count : terms.limit;
		for( ; this_view.is_scaning && i > 0; --i ) {
			file = DBQuery_FetchFile( query );
			if( !file ) {
				break;
			}
			entry = NEW( FileEntryRec, 1 );
			entry->is_dir = FALSE;
			entry->file = file;
			entry->path = file->path;
			//_DEBUG_MSG("file: %s\n", file->path);
			LinkedList_Append( this_view.cache, entry );
		}
		count -= terms.limit;
		terms.offset += terms.limit;
	}
	return total;
}

static int LoadSourceDirs( void )
{
	char *path;
	int i, len;
	FileEntry entry;
	for( i = 0; i < finder.n_dirs; ++i ) {
		entry = NEW( FileEntryRec, 1 );
		len = strlen( finder.dirs[i]->path ) + 1;
		path = malloc( sizeof( char ) * len );
		strcpy( path, finder.dirs[i]->path );
		entry->path = path;
		entry->is_dir = TRUE;
		LinkedList_Append( this_view.cache, entry );
	}
	return i;
}

static void FileScannerThread( void *arg )
{
	int count;
	if( arg ) {
		count = ScanDirs( arg );
		count += ScanFiles( arg );
		free( arg );
	} else {
		count = LoadSourceDirs();
	}
	if( count > 0 ) {
		Widget_AddClass( this_view.tip_empty, "hide" );
		Widget_Hide( this_view.tip_empty );
	} else {
		Widget_RemoveClass( this_view.tip_empty, "hide" );
		Widget_Show( this_view.tip_empty );
	}
	this_view.is_scaning = FALSE;
	LCUIThread_Exit( NULL );
}

static void OnItemClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	FileEntry entry = e->data;
	_DEBUG_MSG( "open file: %s\n", entry->path );
	if( entry->is_dir ) {
		OpenFolder( entry->path );
	}
}

static void SyncViewItems( void *arg )
{
	FileEntry entry;
	DB_Dir dir = arg;
	LCUI_Widget item;
	LinkedList *list;
	int prev_item_type = -1;
	LinkedListNode *node, *prev_node;
	list = this_view.cache;
	if( this_view.cache == this_view.files_cache ) {
		this_view.cache = &this_view.files_cache[1];
	} else {
		this_view.cache = this_view.files_cache;
	}
	LinkedList_ForEach( node, list ) {
		entry = node->data;
		prev_node = node->prev;
		LinkedList_Unlink( list, node );
		LinkedList_AppendNode( &this_view.files, node );
		node = prev_node;
		if( prev_item_type != -1 && prev_item_type != entry->is_dir ) {
			LCUI_Widget separator = LCUIWidget_New(NULL);
			Widget_AddClass( separator, "divider" );
			Widget_Append( this_view.items, separator );
		}
		prev_item_type = entry->is_dir;
		if( entry->is_dir ) {
			item = ThumbView_AppendFolder( this_view.items, 
						       entry->path, 
						       dir == NULL );
		} else {
			item = ThumbView_AppendPicture( this_view.items,
							entry->path );
		}
		Widget_BindEvent( item, "click", OnItemClick, entry, NULL );
	}
	if( this_view.cache->length > 0 && this_view.is_scaning ) {
		LCUITimer_Set( 200, SyncViewItems, dir, FALSE );
	}
}

static void OpenFolder( const char *dirpath )
{
	int i, len;
	char *path = NULL;
	DB_Dir dir = NULL;
	if( dirpath ) {
		len = strlen( dirpath );
		path = malloc( sizeof( char )*(len + 2) );
		strcpy( path, dirpath );
		for( i = 0; i < finder.n_dirs; ++i ) {
			if( strcmp( finder.dirs[i]->path, path ) == 0 ) {
				dir = finder.dirs[i];
				break;
			}
		}
		if( path[len - 1] != PATH_SEP ) {
			path[len++] = PATH_SEP;
			path[len] = 0;
		}
		TextView_SetText( this_view.info_name, getdirname( dirpath ) );
		TextView_SetText( this_view.info_path, dirpath );
		Widget_AddClass( this_view.view, "show-folder-info-box" );
	} else {
		Widget_RemoveClass( this_view.view, "show-folder-info-box" );
	}
	if( this_view.is_scaning ) {
		this_view.is_scaning = FALSE;
		LCUIThread_Join( this_view.scanner_tid, NULL );
	}
	ThumbView_Lock( this_view.items );
	ThumbView_Reset( this_view.items );
	Widget_Empty( this_view.items );
	this_view.dir = dir;
	this_view.is_scaning = TRUE;
	LinkedList_ClearData( &this_view.files, OnDeleteFileEntry );
	LCUIThread_Create( &this_view.scanner_tid, FileScannerThread, path );
	LCUITimer_Set( 200, SyncViewItems, dir, FALSE );
	ThumbView_Unlock( this_view.items );
}

void UI_InitFoldersView( void )
{
	LCUI_Widget btn = LCUIWidget_GetById( "btn-sync-folder-files" );
	LCUI_Widget btn_return = LCUIWidget_GetById( "btn-return-root-folder" );
	LCUI_Widget list = LCUIWidget_GetById( "current-file-list" );
	this_view.items = list;
	LinkedList_Init( &this_view.files );
	LinkedList_Init( &this_view.files_cache[0] );
	LinkedList_Init( &this_view.files_cache[1] );
	this_view.cache = &this_view.files_cache[0];
	Widget_BindEvent( btn, "click", OnBtnSyncClick, NULL, NULL );
	Widget_BindEvent( btn_return, "click", OnBtnReturnClick, NULL, NULL );
	this_view.view = LCUIWidget_GetById( "view-folders");
	this_view.info = LCUIWidget_GetById( "view-folders-info-box" );
	this_view.info_name = LCUIWidget_GetById( "view-folders-info-box-name" );
	this_view.info_path = LCUIWidget_GetById( "view-folders-info-box-path" );
	this_view.tip_empty = LCUIWidget_GetById( "tip-empty-folder" );
	OpenFolder( NULL );
}
