/**
* view_folders.c -- “文件夹”视图
* 版权所有 (C) 2016 归属于 刘超 <root@lc-soft.io>
*/

#include "finder.h"
#include <string.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "tileitem.h"
#include "scrollload.h"

#define THUMB_CACHE_SIZE (20*1024*1024)

typedef struct FileEntryRec_ {
	LCUI_BOOL is_dir;
	DB_File file;
	char *path;
} FileEntryRec, *FileEntry;

static struct FoldersViewData {
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
	ThumbCache thumb_cache;
} this_view;

void OpenFolder( const char *dirpath );

static void OnAddDir( void *privdata, void *data )
{
	LCUI_Widget item = CreateFolderItem( data );
	Widget_Append( this_view.items, item );
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
		_DEBUG_MSG("%s\n", entry->file);
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
	int i, total;
	DB_File file;
	DB_Query query;
	FileEntry entry;
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
	total = DBQuery_GetTotalFiles( query );
	while( this_view.is_scaning && total > 0 ) {
		DB_DeleteQuery( query );
		query = DB_NewQuery( &terms );
		i = total < terms.limit ? total : terms.limit;
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
		total -= terms.limit;
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
	int i;
	FileEntry entry;
	DB_Dir dir = arg;
	LCUI_Widget item;
	LinkedList *list;
	ThumbDB tcache = NULL;
	LinkedListNode *node, *prev_node;

	if( dir ) {
		for( i = 0; i < finder.n_dirs; ++i ) {
			if( dir == finder.dirs[i] ) {
				tcache = finder.thumb_dbs[i];
				break;
			}
		}
	}
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
		if( entry->is_dir ) {
			item = CreateFolderItem( entry->path );
		} else {
			item = CreatePictureItem( this_view.thumb_cache, 
						  entry->path );
		}
		Widget_Append( this_view.items, item );
		Widget_BindEvent( item, "click", OnItemClick, entry, NULL );
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
		if( path[len - 1] != PATH_SEP ) {
			path[len++] = PATH_SEP;
			path[len] = 0;
		}
		for( i = 0; i < finder.n_dirs; ++i ) {
			if( strcmp( finder.dirs[i]->path, path ) == 0 ) {
				dir = finder.dirs[i];
				break;
			}
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
	this_view.is_scaning = TRUE;
	Widget_Empty( this_view.items );
	LinkedList_ClearData( &this_view.files, OnDeleteFileEntry );
	LCUIThread_Create( &this_view.scanner_tid, FileScannerThread, path );
	LCUITimer_Set( 200, SyncViewItems, dir, TRUE );
}

static void OnRemoveThumb( void *data )
{
	_DEBUG_MSG("remove thumb\n");
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
	this_view.thumb_cache = ThumbCache_New( THUMB_CACHE_SIZE, OnRemoveThumb );
	OpenFolder( NULL );
}
