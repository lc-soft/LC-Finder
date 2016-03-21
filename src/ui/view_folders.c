/**
* view_folders.c -- “文件夹”视图
* 版权所有 (C) 2016 归属于 刘超 <root@lc-soft.io>
*/

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <string.h>
#include "finder.h"
#include "tile_item.h"

typedef struct FileEntryRec_ {
	LCUI_BOOL is_dir;
	union {
		DB_File file;
		char *path;
	};
} FileEntryRec, *FileEntry;

static struct FoldersViewData {
	LCUI_Widget items;
	LCUI_BOOL is_scaning;
	LCUI_Thread scanner_tid;
	LinkedList files;
	LinkedList files_cache[2];
	LinkedList *cache;
} this_view;

void OpenFolder( const char *dirpath );

static void OnAddDir( void *privdata, void *data )
{
	LCUI_Widget item = CreateFolderItem( data );
	Widget_Append( this_view.items, item );
}

static void OnBtnSyncClick( LCUI_Widget w, LCUI_WidgetEvent *e, void *arg )
{
	LCFinder_SendEvent( "sync", NULL );
}

static void ScanDirs( char *path )
{
	char *name;
	wchar_t *wpath;
	int len, dirpath_len;
	LCUI_Dir dir;
	LCUI_DirEntry *entry;
	FileEntry file;

	len = strlen( path );
	wpath = malloc( sizeof(wchar_t) * (len + 1) );
	dirpath_len = LCUI_DecodeString( wpath, path, len, ENCODING_UTF8 );
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
		file = NEW( FileEntryRec, 1 );
		len = LCUI_EncodeString( NULL, wname, 0, ENCODING_UTF8 ) + 1;
		name = malloc( sizeof(char) * len );
		LCUI_EncodeString( name, wname, len, ENCODING_UTF8 );
		file->is_dir = LCUI_FileIsDirectory( entry );
		file->path = malloc( sizeof( char ) * (len + dirpath_len) );
		sprintf( file->path, "%s%s", path, name );
		LinkedList_Append( this_view.cache, file );
	}
	LCUI_CloseDir( &dir );
}

static void ScanFiles( char *path )
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
	total = DBQuery_GetTotalFiles( query ) + terms.limit;
	while( terms.offset < total && this_view.is_scaning ) {
		DB_DeleteQuery( query );
		query = DB_NewQuery( &terms );
		for( i = 0; i < terms.limit; ++i ) {
			file = DBQuery_FetchFile( query );
			if( !file ) {
				break;
			}
			entry = NEW( FileEntryRec, 1 );
			entry->is_dir = FALSE;
			entry->file = file;
			entry->path = file->path;
			LinkedList_Append( this_view.cache, entry );
			printf( "fetch file: %s\n", file->path );
		}
		terms.offset += terms.limit;
	}
}

static void LoadSourceDirs( void )
{
	char *path;
	int i, len;
	FileEntry entry;
	for( i = 0; i < finder.n_dirs; ++i ) {
		entry = NEW( FileEntryRec, 1 );
		len = strlen( finder.dirs[i]->path ) + 1;
		path = malloc( sizeof( wchar_t ) * len );
		strcpy( path, finder.dirs[i]->path );
		entry->path = path;
		entry->is_dir = TRUE;
		LinkedList_Append( this_view.cache, entry );
	}
}

static void FileScannerThread( void *arg )
{
	if( arg ) {
		ScanDirs( arg );
		ScanFiles( arg );
		free( arg );
	} else {
		LoadSourceDirs();
	}
	this_view.is_scaning = FALSE;
	LCUIThread_Exit( NULL );
}

static void OnItemClick( LCUI_Widget w, LCUI_WidgetEvent *e, void *arg )
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
	DB_Dir dir = arg;
	FileEntry entry;
	LCUI_Widget item;
	LinkedList *list;
	LinkedListNode *node, *prev_node;
	ThumbCache tcache = NULL;

	if( dir ) {
		for( i = 0; i < finder.n_dirs; ++i ) {
			if( dir == finder.dirs[i] ) {
				tcache = finder.thumb_caches[i];
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
		if( entry->is_dir ) {
			item = CreateFolderItem( entry->path );
		} else {
			continue;
			item = CreatePictureItem( entry->path, NULL );
		}
		LinkedList_Unlink( list, node );
		LinkedList_AppendNode( &this_view.files, node );
		Widget_Append( this_view.items, item );
		Widget_BindEvent( item, "click", OnItemClick, entry, NULL );
		node = prev_node;
	}
}

static void OpenFolder( const char *dirpath )
{
	int i, len;
	char *path = NULL;
	DB_Dir dir = NULL;
	if( dirpath ) {
		len = strlen( dirpath );
		path = malloc( sizeof( char )*len );
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
	}
	if( this_view.is_scaning ) {
		this_view.is_scaning = FALSE;
		LCUIThread_Join( this_view.scanner_tid, NULL );
	}
	this_view.is_scaning = TRUE;
	LCUIThread_Create( &this_view.scanner_tid, FileScannerThread, path );
	LCUITimer_Set( 200, SyncViewItems, dir, TRUE );
}

void UI_InitFoldersView( void )
{
	LCUI_Widget btn = LCUIWidget_GetById( "btn-sync-folder-files" );
	LCUI_Widget list = LCUIWidget_GetById( "current-folder-list" );
	this_view.items = list;
	LinkedList_Init( &this_view.files_cache[0] );
	LinkedList_Init( &this_view.files_cache[1] );
	this_view.cache = &this_view.files_cache[0];
	Widget_BindEvent( btn, "click", OnBtnSyncClick, NULL, NULL );
	OpenFolder( NULL );
}
