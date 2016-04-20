/* ***************************************************************************
* view_home.c -- home view
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
* view_home.c -- 主页集锦视图
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

typedef struct FileEntryRec_ {
	LCUI_BOOL is_dir;
	DB_File file;
	char *path;
} FileEntryRec, *FileEntry;

static struct FoldersViewData {
	LCUI_Widget view;
	LCUI_Widget items;
	LCUI_Widget info_path;
	LCUI_Widget tip_empty;
	LCUI_BOOL is_scaning;
	LCUI_Thread scanner_tid;
	LinkedList files;
	LinkedList files_cache[2];
	LinkedList *cache;
} this_view;

static void OnBtnSyncClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	LCFinder_TriggerEvent( EVENT_SYNC, NULL );
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

static int ScanAllFiles( void )
{
	DB_File file;
	DB_Query query;
	FileEntry entry;
	int i, total, count;
	DB_QueryTermsRec terms;
	terms.dirpath = NULL;
	terms.n_dirs = 0;
	terms.n_tags = 0;
	terms.limit = 100;
	terms.offset = 0;
	terms.score = NONE;
	terms.tags = NULL;
	terms.dirs = NULL;
	terms.create_time = DESC;
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
			LinkedList_Append( this_view.cache, entry );
		}
		count -= terms.limit;
		terms.offset += terms.limit;
	}
	return total;
}

static void FileScannerThread( void *arg )
{
	int count;
	count = ScanAllFiles();
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
}

static void SyncViewItems( void *arg )
{
	FileEntry entry;
	LCUI_Widget item;
	LinkedList *list;
	ThumbDB tcache = NULL;
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
		prev_item_type = entry->is_dir;
		if( entry->is_dir ) {
			continue;
		}
		item = ThumbView_AppendPicture( this_view.items, entry->path );
		Widget_BindEvent( item, "click", OnItemClick, entry, NULL );
	}
	if( this_view.cache->length > 0 && this_view.is_scaning ) {
		LCUITimer_Set( 200, SyncViewItems, NULL, FALSE );
	}
}

static void LoadcollectionFiles( void )
{
	char *path = NULL;
	if( this_view.is_scaning ) {
		this_view.is_scaning = FALSE;
		LCUIThread_Join( this_view.scanner_tid, NULL );
	}
	ThumbView_Lock( this_view.items );
	ThumbView_Reset( this_view.items );
	Widget_Empty( this_view.items );
	this_view.is_scaning = TRUE;
	LinkedList_ClearData( &this_view.files, OnDeleteFileEntry );
	LCUIThread_Create( &this_view.scanner_tid, FileScannerThread, path );
	LCUITimer_Set( 200, SyncViewItems, NULL, FALSE );
	ThumbView_Unlock( this_view.items );
}

void UI_InitHomeView( void )
{
	LCUI_Widget btn = LCUIWidget_GetById( "btn-sync-collection-files" );
	LCUI_Widget list = LCUIWidget_GetById( "home-collection-list" );
	this_view.items = list;
	LinkedList_Init( &this_view.files );
	LinkedList_Init( &this_view.files_cache[0] );
	LinkedList_Init( &this_view.files_cache[1] );
	this_view.cache = &this_view.files_cache[0];
	Widget_BindEvent( btn, "click", OnBtnSyncClick, NULL, NULL );
	this_view.view = LCUIWidget_GetById( "view-home" );
	this_view.tip_empty = LCUIWidget_GetById( "tip-empty-collection" );
	LoadcollectionFiles();
}
