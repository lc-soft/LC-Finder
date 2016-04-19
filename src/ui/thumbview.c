
#include <stdio.h>
#include <string.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "finder.h"
#include "thumbview.h"

typedef struct ThumbViewRec_ {
	ThumbCache cache;
	LCUI_BOOL is_loading;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	LCUI_Thread thread;
	LinkedList tasks;
	Dict *dbs;
} ThumbViewRec, *ThumbView;

typedef struct ThumbFileInfoRec_ {
	LCUI_BOOL is_dir;
	char *path;
} ThumbFileInfoRec, *ThumbFileInfo;

typedef struct ThumbViewTaskRec_ {
	LCUI_Widget widget;
	ThumbFileInfo info;
} ThumbViewTaskRec, *ThumbViewTask;

typedef struct ThumbItemDataRec_ {
	ThumbFileInfoRec info;
	ThumbView view;
} ThumbItemDataRec, *ThumbItemData;

#define THUMB_CACHE_SIZE (20*1024*1024)

#define THUMB_MAX_WIDTH 240

static int GetDirThumbFilePath( char *filepath, char *dirpath )
{
	int total;
	DB_File file;
	DB_Query query;
	DB_QueryTermsRec terms;
	terms.dirpath = dirpath;
	terms.n_dirs = 1;
	terms.n_tags = 0;
	terms.limit = 10;
	terms.offset = 0;
	terms.tags = NULL;
	terms.dirs = NULL;
	terms.create_time = DESC;
	query = DB_NewQuery( &terms );
	total = DBQuery_GetTotalFiles( query );
	if( total > 0 ) {
		file = DBQuery_FetchFile( query );
		strcpy( filepath, file->path );
	}
	return total;
}

static void OnRemoveThumb( void *data )
{
	_DEBUG_MSG("remove thumb\n");
}

static LCUI_Graph *LoadThumb( ThumbView view, ThumbFileInfo info,
			      LCUI_Widget w )
{
	int len;
	DB_Dir dir;
	ThumbDB db;
	ThumbDataRec tdata;
	LCUI_Graph *thumb;
	const char *filename;
	char apath[PATH_LEN];
	wchar_t wpath[PATH_LEN];
	dir = LCFinder_GetSourceDir( info->path );
	if( !dir ) {
		return NULL;
	}
	db = Dict_FetchValue( view->dbs, dir->path );
	if( !db ) {
		return NULL;
	}
	len = strlen( dir->path );
	filename = info->path + len;
	if( filename[0] == PATH_SEP ) {
		++filename;
	}
	if( info->is_dir ) {
		char path[PATH_LEN];
		pathjoin( path, info->path, "" );
		if( GetDirThumbFilePath( path, path ) == 0 ) {
			return NULL;
		}
		LCUI_DecodeString( wpath, path, PATH_LEN, ENCODING_UTF8 );
	} else {
		/* 将路径的编码方式由 UTF-8 转换成 ANSI */
		LCUI_DecodeString( wpath, info->path, PATH_LEN, ENCODING_UTF8 );
	}
	LCUI_EncodeString( apath, wpath, PATH_LEN, ENCODING_ANSI );
	if( ThumbDB_Load( db, filename, &tdata ) != 0 ) {
		LCUI_Graph img;
		Graph_Init( &img );
		Graph_Init( &tdata.graph );
		if( Graph_LoadImage( apath, &img ) != 0 ) {
			return NULL;
		}
		if( img.width > THUMB_MAX_WIDTH ) {
			Graph_Zoom( &img, &tdata.graph, TRUE,
				    THUMB_MAX_WIDTH, 0 );
		} else {
			tdata.graph = img;
		}
		ThumbDB_Save( db, filename, &tdata );
	}
	thumb = ThumbCache_Add( view->cache, info->path, &tdata.graph, w );
	return thumb;
}

static void ThumbView_ExecTask( ThumbView view, ThumbViewTask t )
{
	LCUI_Graph *thumb;
	thumb = ThumbCache_Get( view->cache, t->info->path );
	if( !thumb ) {
		thumb = LoadThumb( view, t->info, t->widget );
		if( !thumb ) {
			return;
		}
	}
	SetStyle( t->widget->custom_style, key_background_image, thumb, image );
	Widget_UpdateStyle( t->widget, FALSE );
}

static void ThumbView_TaskThread( void *arg )
{
	DictEntry *entry;
	DictIterator *iter;
	LinkedListNode *node, *prev;
	ThumbView view = arg;
	while( 1 ) {
		LCUICond_Wait( &view->cond, &view->mutex );
		view->is_loading = TRUE;
		LinkedList_ForEach( node, &view->tasks ) {
			prev = node->prev;
			LinkedList_Unlink( &view->tasks, node );
			ThumbView_ExecTask( view, node->data );
			free( node->data );
			node->data = NULL;
			free( node );
			node = prev;
			if( !view->is_loading ) {
				break;
			}
		}
		view->is_loading = FALSE;
		LCUIMutex_Unlock( &view->mutex );
		iter = Dict_GetIterator( view->dbs );
		entry = Dict_Next( iter );
		while( entry ) {
			ThumbDB db = DictEntry_GetVal( entry );
			ThumbDB_Commit( db );
			entry = Dict_Next( iter );
		}
		Dict_ReleaseIterator( iter );
	}
}

void ThumbView_Reset( LCUI_Widget w )
{
	ThumbView view;
	view = w->private_data;
	view->is_loading = FALSE;
	LCUIMutex_Lock( &view->mutex );
	LinkedList_Clear( &view->tasks, free );
	LCUIMutex_Unlock( &view->mutex );
}

static void OnScrollLoad( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	ThumbViewTask task;
	ThumbItemData data = w->private_data;
	if( !data || w->custom_style->sheet[key_background_image].is_valid ) {
		return;
	}
	task = NEW( ThumbViewTaskRec, 1 );
	task->widget = w;
	task->info = &data->info;
	LinkedList_Append( &data->view->tasks, task );
	LCUICond_Signal( &data->view->cond );
}

LCUI_Widget ThumbView_AppendFolder( LCUI_Widget w, const char *filepath, 
				    LCUI_BOOL show_path )
{
	ThumbItemData data;
	int len = strlen( filepath ) + 1;
	LCUI_Widget item = LCUIWidget_New( NULL );
	LCUI_Widget infobar = LCUIWidget_New( NULL );
	LCUI_Widget name = LCUIWidget_New( "textview" );
	LCUI_Widget path = LCUIWidget_New( "textview" );
	LCUI_Widget icon = LCUIWidget_New( "textview" );
	data = Widget_NewPrivateData( item, ThumbItemDataRec );
	data->info.path = malloc( sizeof( char )*len );
	strncpy( data->info.path, filepath, len );
	data->view = w->private_data;
	data->info.is_dir = TRUE;
	Widget_AddClass( item, "file-list-item-folder" );
	if( !show_path ) {
		Widget_AddClass( item, "hide-path" );
	}
	Widget_AddClass( infobar, "info" );
	Widget_AddClass( name, "name" );
	Widget_AddClass( path, "path" );
	Widget_AddClass( icon, "icon ion ion-android-folder-open" );
	TextView_SetText( name, getdirname( filepath ) );
	TextView_SetText( path, filepath );
	Widget_Append( item, infobar );
	Widget_Append( infobar, name );
	Widget_Append( infobar, path );
	Widget_Append( infobar, icon );
	Widget_BindEvent( item, "scrollload", OnScrollLoad, NULL, NULL );
	Widget_Append( w, item );
	return item;
}

LCUI_Widget ThumbView_AppendPicture( LCUI_Widget w, const char *path )
{
	ThumbItemData data;
	int len = strlen( path ) + 1;
	LCUI_Widget item = LCUIWidget_New( NULL );
	data = Widget_NewPrivateData( item, ThumbItemDataRec );
	data->info.path = malloc( sizeof( char )*len );
	strncpy( data->info.path, path, len );
	data->view = w->private_data;
	data->info.is_dir = FALSE;
	Widget_AddClass( item, "file-list-item-picture" );
	Widget_BindEvent( item, "scrollload", OnScrollLoad, NULL, NULL );
	Widget_Append( w, item );
	return item;
}

static void ThumbView_OnInit( LCUI_Widget w )
{
	ThumbView self;
	self = Widget_NewPrivateData( w, ThumbViewRec );
	self->is_loading = FALSE;
	self->dbs = finder.thumb_dbs;
	LCUICond_Init( &self->cond );
	LCUIMutex_Init( &self->mutex );
	LinkedList_Init( &self->tasks );
	self->cache = ThumbCache_New( THUMB_CACHE_SIZE, OnRemoveThumb );
	LCUIThread_Create( &self->thread, ThumbView_TaskThread, self );
}

static void ThumbView_OnDestroy( LCUI_Widget w )
{

}

void LCUIWidget_AddTThumbView( void )
{
	LCUI_WidgetClass *wc = LCUIWidget_AddClass( "thumbview" );
	wc->methods.init = ThumbView_OnInit;
	wc->methods.destroy = ThumbView_OnDestroy;
}
