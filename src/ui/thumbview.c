/* ***************************************************************************
* thumbview.c -- thumbnail list view
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
* thumbview.c -- 缩略图列表视图部件，主要用于以缩略图形式显示文件夹和文件列表
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

#include <stdio.h>
#include <string.h>
#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "thumbview.h"

typedef struct ScrollLoadingRec_ {
	int top;
	int event_id;
	LCUI_BOOL enabled;
	LCUI_Widget scrolllayer;
	LCUI_Widget top_child;
} ScrollLoadingRec, *ScrollLoading;

typedef struct ThumbViewRec_ {
	Dict *dbs;
	ThumbCache cache;
	LinkedList tasks;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	LCUI_Thread thread;
	LCUI_BOOL is_loading;
	LCUI_BOOL need_update;
	ScrollLoading scrollload;
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

static int scrollload_event = -1;

static int ScrollLoading_Update( ScrollLoading ctx )
{
	LCUI_Widget w;
	LinkedListNode *node;
	LCUI_WidgetEventRec e;
	int count = 0, top, bottom;

	if( !ctx->enabled ) {
		return 0;
	}
	e.type = ctx->event_id;
	e.cancel_bubble = TRUE;
	bottom = top = ctx->top;
	bottom += ctx->scrolllayer->parent->box.padding.height;
	if( !ctx->top_child ) {
		node = ctx->scrolllayer->children.head.next;
		if( node ) {
			ctx->top_child = node->data;
		}
	}
	if( !ctx->top_child ) {
		return 0;
	}
	if( ctx->top_child->box.border.top > bottom ) {
		node = Widget_GetNode( ctx->top_child );
		ctx->top_child = NULL;
		while( node ) {
			w = node->data;
			if( w->box.border.y < bottom ) {
				ctx->top_child = w;
				break;
			}
			node = node->prev;
		}
		if( !ctx->top_child ) {
			return 0;
		}
	}
	node = Widget_GetNode( ctx->top_child );
	while( node ) {
		w = node->data;
		if( w->box.border.y > bottom ) {
			break;
		}
		if( w->box.border.y + w->box.border.height >= top ) {
			Widget_TriggerEvent( w, &e, NULL );
			++count;
		}
		node = node->next;
	}
	return count;
}

static void OnScroll( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	int *scroll_pos = arg;
	ScrollLoading ctx = e->data;
	ctx->top = *scroll_pos;
	ScrollLoading_Update( ctx );
}

ScrollLoading ScrollLoading_New( LCUI_Widget scrolllayer )
{
	ScrollLoading ctx = NEW( ScrollLoadingRec, 1 );
	ctx->top = 0;
	ctx->top_child = NULL;
	ctx->event_id = scrollload_event;
	ctx->scrolllayer = scrolllayer;
	Widget_BindEvent( scrolllayer, "scroll", OnScroll, ctx, NULL );
	return ctx;
}

static void ScrollLoading_Delete( ScrollLoading ctx )
{
	ctx->top = 0;
	ctx->top_child = NULL;
	Widget_UnbindEvent( ctx->scrolllayer, "scroll", OnScroll );
	free( ctx );
}

static void ScrollLoading_Reset( ScrollLoading ctx )
{
	ctx->top = 0;
	ctx->top_child = NULL;
}

static void ScrollLoading_Enable( ScrollLoading ctx, LCUI_BOOL enable )
{
	ctx->enabled = enable;
}

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
		_DEBUG_MSG("[%p] loading thumb...\n", view);
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

void ThumbView_Lock( LCUI_Widget w )
{
	ThumbView view = w->private_data;
	ScrollLoading_Enable( view->scrollload, FALSE );
}

void ThumbView_Unlock( LCUI_Widget w )
{
	ThumbView view = w->private_data;
	ScrollLoading_Enable( view->scrollload, TRUE );
}

void ThumbView_Reset( LCUI_Widget w )
{
	ThumbView view;
	view = w->private_data;
	view->is_loading = FALSE;
	LCUIMutex_Lock( &view->mutex );
	LinkedList_Clear( &view->tasks, free );
	LCUIMutex_Unlock( &view->mutex );
	ScrollLoading_Reset( view->scrollload );
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
	_DEBUG_MSG("on scroll load: %s\n", task->info->path);
}

static void UpdateView( void *arg )
{
	LCUI_Widget w = arg;
	ThumbView view = w->private_data;
	if( !view->need_update ) {
		return;
	}
	view->need_update = FALSE;
	ScrollLoading_Update( view->scrollload );
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
	data->view->need_update = TRUE;
	LCUITimer_Set( 200, UpdateView, w, FALSE );
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
	data->view->need_update = TRUE;
	LCUITimer_Set( 200, UpdateView, w, FALSE );
	return item;
}

static void ThumbView_OnInit( LCUI_Widget w )
{
	ThumbView self;
	self = Widget_NewPrivateData( w, ThumbViewRec );
	self->is_loading = FALSE;
	self->need_update = FALSE;
	self->dbs = finder.thumb_dbs;
	LCUICond_Init( &self->cond );
	LCUIMutex_Init( &self->mutex );
	LinkedList_Init( &self->tasks );
	self->scrollload = ScrollLoading_New( w );
	self->cache = ThumbCache_New( THUMB_CACHE_SIZE, OnRemoveThumb );
	LCUIThread_Create( &self->thread, ThumbView_TaskThread, self );
}

static void ThumbView_OnDestroy( LCUI_Widget w )
{
	ThumbView self = w->private_data;
	ScrollLoading_Delete( self->scrollload );
}

void LCUIWidget_AddTThumbView( void )
{
	LCUI_WidgetClass *wc = LCUIWidget_AddClass( "thumbview" );
	wc->methods.init = ThumbView_OnInit;
	wc->methods.destroy = ThumbView_OnDestroy;
	scrollload_event = LCUIWidget_AllocEventId();
	LCUIWidget_SetEventName( scrollload_event, "scrollload" );
}
