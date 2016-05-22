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

#define THUBVIEW_MIN_WIDTH	400
#define FOLDER_MAX_WIDTH	388
#define FOLDER_FIXED_HEIGHT	134
#define PICTURE_FIXED_HEIGHT	226
/** 文件夹的右外边距，需要与 CSS 文件定义的样式一致 */
#define FOLDER_MARGIN_RIGHT	10
#define FOLDER_CLASS		"file-list-item-folder"
#define PICTURE_CLASS		"file-list-item-picture"
#define DIR_COVER_THUMB		"__dir_cover_thumb__"
#define THUMB_CACHE_SIZE	(20*1024*1024)
#define THUMB_MAX_WIDTH		240

/** 滚动加载功能的相关数据 */
typedef struct ScrollLoadingRec_ {
	int top;			/**< 当前可见区域上边界的 Y 轴坐标 */
	int event_id;			/**< 滚动加载功能的事件ID */
	int timer;			/**< 定时器，用于实现延迟加载 */
	LCUI_BOOL is_delaying;		/**< 是否处于延迟状态 */
	LCUI_BOOL enabled;		/**< 是否启用滚动加载功能 */
	LCUI_Widget scrolllayer;	/**< 滚动层 */
	LCUI_Widget top_child;		/**< 当前可见区域第一个子部件 */
} ScrollLoadingRec, *ScrollLoading;

/** 缩略图列表布局功能的相关数据 */
typedef struct LayoutContextRec_ {
	int x;				/** 当前对象的 X 轴坐标 */
	int count;			/** 当前处理的总对象数量 */
	int max_width;			/** 最大宽度 */
	LCUI_BOOL is_running;		/** 是否正在布局 */
	LCUI_BOOL is_delaying;		/** 是否处于延迟状态 */
	LCUI_Widget current;		/** 当前处理的部件 */
	LinkedList row;			/** 当前行的部件 */
	LCUI_Mutex row_mutex;		/** 当前行的互斥锁 */
	int timer;			/** 定时器 */
	int folder_count;		/**< 当前处理的文件夹数量 */
	int folders_per_row;		/**< 每行有多少个文件夹 */
} LayoutContextRec, *LayoutContext;

typedef struct ThumbViewRec_ {
	Dict **dbs;			/**< 缩略图数据库字典，以目录路径进行索引 */
	ThumbCache cache;		/**< 缩略图缓存 */
	LinkedList tasks;		/**< 当前任务队列 */
	LinkedList files;		/**< 当前视图下的文件列表 */
	LCUI_Cond tasks_cond;		/**< 任务队列条件变量 */
	LCUI_Mutex tasks_mutex;		/**< 任务队列互斥锁 */
	LCUI_Mutex mutex;		/**< 互斥锁 */
	LCUI_Thread thread;		/**< 任务处理线程 */
	LCUI_BOOL is_loading;		/**< 是否处于载入中状态 */
	LCUI_BOOL is_running;		/**< 是否处于运行状态 */
	ScrollLoading scrollload;	/**< 滚动加载功能所需的相关数据 */
	LayoutContextRec layout;	/**< 缩略图布局功能所需的相关数据 */
} ThumbViewRec, *ThumbView;

/** 文件信息 */
typedef struct ThumbFileInfoRec_ {
	LCUI_BOOL is_dir;	/**< 是否为目录 */
	char *path;		/**< 路径 */
} ThumbFileInfoRec, *ThumbFileInfo;

/** 缩略图加载任务 */
typedef struct ThumbViewTaskRec_ {
	LCUI_Widget widget;	/**< 需加载缩略图的部件 */
	ThumbFileInfo info;	/**< 缩略图源文件的信息 */
} ThumbViewTaskRec, *ThumbViewTask;

/** 缩略图列表项的数据 */
typedef struct ThumbItemDataRec_ {
	ThumbFileInfoRec info;	/**< 缩略图源文件的信息 */
	ThumbView view;		/**< 所属缩略图列表视图部件 */
	int width, height;	/**< 图片原始尺寸 */
} ThumbItemDataRec, *ThumbItemData;

static int scrollload_event = -1;

static int ScrollLoading_OnUpdate( ScrollLoading ctx )
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
			if( w ) {
				if( w->box.border.y < bottom ) {
					ctx->top_child = w;
					break;
				}
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

static void ScrollLoading_OnDelayUpdate( void *arg )
{
	ScrollLoading ctx = arg;
	ScrollLoading_OnUpdate( ctx );
	ctx->is_delaying = FALSE;
	ctx->timer = -1;
	DEBUG_MSG("ctx->top: %d\n", ctx->top);
}

static void ScrollLoading_Update( ScrollLoading ctx )
{
	if( ctx->is_delaying && ctx->timer > 0 ) {
		LCUITimer_Reset( ctx->timer, 500 );
		return;
	}
	ctx->timer = LCUITimer_Set( 500, ScrollLoading_OnDelayUpdate, 
				    ctx, FALSE );
	ctx->is_delaying = TRUE;
}

static void ScrollLoading_Update2( ScrollLoading ctx )
{
	if( ctx->is_delaying ) {
		return;
	}
	ctx->is_delaying = TRUE;
	LCUITimer_Set( 200, ScrollLoading_OnDelayUpdate, ctx, FALSE );
}

static void ScrollLoading_OnScroll( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	int *scroll_pos = arg;
	ScrollLoading ctx = e->data;
	ctx->top = *scroll_pos;
	ScrollLoading_Update( ctx );
}

/** 新建一个滚动加载功能实例 */
static ScrollLoading ScrollLoading_New( LCUI_Widget scrolllayer )
{
	ScrollLoading ctx = NEW( ScrollLoadingRec, 1 );
	ctx->top = 0;
	ctx->timer = -1;
	ctx->top_child = NULL;
	ctx->is_delaying = FALSE;
	ctx->scrolllayer = scrolllayer;
	ctx->event_id = scrollload_event;
	Widget_BindEvent( scrolllayer, "scroll", 
			  ScrollLoading_OnScroll, ctx, NULL );
	return ctx;
}

/** 删除滚动加载功能实例 */
static void ScrollLoading_Delete( ScrollLoading ctx )
{
	ctx->top = 0;
	ctx->top_child = NULL;
	Widget_UnbindEvent( ctx->scrolllayer, "scroll", 
			    ScrollLoading_OnScroll );
	free( ctx );
}

/** 重置滚动加载 */
static void ScrollLoading_Reset( ScrollLoading ctx )
{
	ctx->top = 0;
	ctx->top_child = NULL;
}

/** 启用/禁用滚动加载 */
static void ScrollLoading_Enable( ScrollLoading ctx, LCUI_BOOL enable )
{
	ctx->enabled = enable;
}

/** 获取文件夹缩略图文件路径 */
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

/** 当移除缩略图的时候 */
static void OnRemoveThumb( void *data )
{
	LCUI_Widget w = data;
	Widget_Lock( w );
	DEBUG_MSG("remove thumb\n");
	Graph_Init( &w->computed_style.background.image );
	w->custom_style->sheet[key_background_image].is_valid = FALSE;
	Widget_UpdateStyle( w, FALSE );
	Widget_Unlock( w );
}

/** 载入缩略图 */
static LCUI_Graph *LoadThumb( ThumbView view, ThumbFileInfo info,
			      LCUI_Widget w )
{
	int len;
	Dict *dbs;
	DB_Dir dir;
	ThumbDB db;
	ThumbDataRec tdata;
	LCUI_Graph *thumb;
	const char *filename;
	wchar_t wpath[PATH_LEN];
	char apath[PATH_LEN], path[PATH_LEN];
	dir = LCFinder_GetSourceDir( info->path );
	if( !dir ) {
		return NULL;
	}
	dbs = *view->dbs;
	db = Dict_FetchValue( dbs, dir->path );
	if( !db ) {
		return NULL;
	}
	len = strlen( dir->path );
	if( info->is_dir ) {
		pathjoin( path, info->path, "" );
		if( GetDirThumbFilePath( path, path ) == 0 ) {
			return NULL;
		}
		/* 将路径的编码由 UTF-8 解码成 Unicode */
		LCUI_DecodeString( wpath, path, PATH_LEN, ENCODING_UTF8 );
		pathjoin( path, path, DIR_COVER_THUMB );
		filename = path + len;
	} else {
		filename = info->path + len;
		if( filename[0] == PATH_SEP ) {
			++filename;
		}
		LCUI_DecodeString( wpath, info->path, PATH_LEN, ENCODING_UTF8 );
	}
	LCUI_EncodeString( apath, wpath, PATH_LEN, ENCODING_ANSI );
	DEBUG_MSG( "load thumb from: %s\n", filename );
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
			Graph_Free( &img );
		} else {
			tdata.graph = img;
		}
		DEBUG_MSG( "save thumb, size: (%d,%d), name: %s, db: %p\n",
			   tdata.graph.width, tdata.graph.height, filename, db );
		ThumbDB_Save( db, filename, &tdata );
	}
	thumb = ThumbCache_Add( view->cache, info->path, &tdata.graph, w );
	return thumb;
}

/** 执行缩略图加载任务 */
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
	Widget_Lock( t->widget );
	SetStyle( t->widget->custom_style, key_background_image, thumb, image );
	Widget_UpdateStyle( t->widget, FALSE );
	_DEBUG_MSG( "%s\n", t->info->path );
	Widget_Unlock( t->widget );
}

/** 缩略图加载任务处理线程 */
static void ThumbView_TaskThread( void *arg )
{
	ThumbView view = arg;
	LinkedListNode *node;
	while( view->is_running ) {
		view->is_loading = TRUE;
		LCUIMutex_Lock( &view->mutex );
		LCUIMutex_Lock( &view->tasks_mutex );
		node = LinkedList_GetNode( &view->tasks, 0 );
		if( !node ) {
			view->is_loading = FALSE;
			LCUIMutex_Unlock( &view->mutex );
			LCUICond_Wait( &view->tasks_cond, &view->tasks_mutex );
			LCUIMutex_Unlock( &view->tasks_mutex );
			continue;
		}
		LinkedList_Unlink( &view->tasks, node );
		LCUIMutex_Unlock( &view->tasks_mutex );
		ThumbView_ExecTask( view, node->data );
		free( node->data );
		node->data = NULL;
		free( node );
		LCUIMutex_Unlock( &view->mutex );
	}
}

void ThumbView_Lock( LCUI_Widget w )
{
	ThumbView view = w->private_data;
	ScrollLoading_Enable( view->scrollload, FALSE );
	LCUIMutex_Lock( &view->mutex );
}

void ThumbView_Unlock( LCUI_Widget w )
{
	ThumbView view = w->private_data;
	ScrollLoading_Enable( view->scrollload, TRUE );
	LCUIMutex_Unlock( &view->mutex );
}

static void ThumbView_UpdateLayoutContext( LCUI_Widget w )
{
	int max_width, n;
	ThumbView view = w->private_data;
	view->layout.max_width = w->box.content.width;
	max_width = view->layout.max_width;
	n = max_width / FOLDER_MAX_WIDTH;
	if( max_width % FOLDER_MAX_WIDTH > 0 ) {
		n = n + 1;
	}
	view->layout.folders_per_row = n;
}

void ThumbView_Empty( LCUI_Widget w )
{
	ThumbView view;
	LinkedListNode *node;
	view = w->private_data;
	view->is_loading = FALSE;
	LCUIMutex_Lock( &view->tasks_mutex );
	LCUIMutex_Lock( &view->layout.row_mutex );
	view->layout.x = 0;
	view->layout.count = 0;
	view->layout.current = NULL;
	view->layout.folder_count = 0;
	LinkedList_Clear( &view->tasks, free );
	LinkedList_Clear( &view->layout.row, NULL );
	ThumbView_UpdateLayoutContext( w );
	LinkedList_ForEach( node, &view->files ) {
		ThumbCache_Delete( view->cache, node->data );
	}
	LinkedList_Clear( &view->files, NULL );
	Widget_Empty( w );
	LCUICond_Signal( &view->tasks_cond );
	LCUIMutex_Unlock( &view->layout.row_mutex );
	LCUIMutex_Unlock( &view->tasks_mutex );
	ScrollLoading_Reset( view->scrollload );
}

/** 更新当前行内的各个缩略图尺寸 */
static void UpdateThumbRow( ThumbView view )
{
	LCUI_Widget w;
	ThumbItemData data;
	LinkedListNode *node;
	int overflow_width;

	if( view->layout.max_width <= THUBVIEW_MIN_WIDTH ) {
		return;
	}
	overflow_width = view->layout.x - view->layout.max_width;
	/**
	 * 如果这一行缩略图的总宽度有溢出（超出最大宽度），则根据缩略图宽度占总宽度
	 * 的比例，分别缩减相应的宽度。
	 */
	if( overflow_width > 0 ) {
		int i = 0, debug_width = 0, width, thumb_width, rest_width;
		rest_width = overflow_width;
		LCUIMutex_Lock( &view->layout.row_mutex );
		LinkedList_ForEach( node, &view->layout.row ) {
			w = node->data;
			data = w->private_data;
			thumb_width = PICTURE_FIXED_HEIGHT * data->width;
			thumb_width /= data->height;
			width = overflow_width * thumb_width;
			width /= view->layout.x;
			/** 
			 * 以上按比例分配的扣除宽度有误差，通常会少扣除几个像素的
			 * 宽度，这里用 rest_width 变量记录剩余待扣除的宽度，最
			 * 后一个缩略图的宽度直接减去 rest_width，以补全少扣除的
			 * 宽度。
			 */
			if( node->next ) {
				rest_width -= width;
				width = thumb_width - width;
			} else {
				width = thumb_width - rest_width;
			}
			debug_width += width;
			SetStyle( w->custom_style, key_width, width, px );
			Widget_UpdateStyle( w, FALSE );
		}
		LCUIMutex_Unlock( &view->layout.row_mutex );
	}
	LinkedList_Clear( &view->layout.row, NULL );
	view->layout.x = 0;
}

/* 追加图片，并处理布局 */
static void AppendPicture( ThumbView view, LCUI_Widget w )
{
	int width;
	ThumbItemData data;
	data = w->private_data;
	view->layout.current = w;
	width = PICTURE_FIXED_HEIGHT * data->width / data->height;
	SetStyle( w->custom_style, key_width, width, px );
	Widget_UpdateStyle( w, FALSE );
	view->layout.x += width;
	LinkedList_Append( &view->layout.row, w );
	if( view->layout.x >= view->layout.max_width ) {
		UpdateThumbRow( view );
	}
}

/** 追加文件夹，并处理布局 */
static void AppendFolder( ThumbView view, LCUI_Widget w )
{
	int width, n;
	++view->layout.folder_count;
	UpdateThumbRow( view );
	view->layout.current = w;
	if( view->layout.max_width < 480 ) {
		w->custom_style->sheet[key_width].is_valid = FALSE;
		Widget_AddClass( w, "full-width" );
		return;
	} else {
		Widget_RemoveClass( w, "full-width" );
	}
	n = view->layout.folders_per_row;
	width = view->layout.max_width;
	width = (width - FOLDER_MARGIN_RIGHT*(n - 1)) / n;
	/* 设置每行最后一个文件夹的右边距为 0px */
	if( view->layout.folder_count % n == 0 ) {
		SetStyle( w->custom_style, key_margin_right, 0, px );
	} else {
		w->custom_style->sheet[key_margin_right].is_valid = FALSE;
	}
	SetStyle( w->custom_style, key_width, width, px );
	Widget_UpdateStyle( w, FALSE );
}

/** 直接执行布局更新操作 */
static int ThumbView_ExecUpdateLayout( LCUI_Widget w, int limit )
{
	int count;
	LCUI_Widget child;
	LinkedListNode *node;
	ThumbView view = w->private_data;
	if( view->layout.current ) {
		node = Widget_GetNode( view->layout.current );
	} else {
		node = LinkedList_GetNode( &w->children, 0 );
	}
	for( count = 0; node && --limit >= 0; node = node->next ) {
		child = node->data;
		view->layout.count += 1;
		if( !child->computed_style.visible ) {
			++limit;
			continue;
		}
		if( Widget_HasClass( child, PICTURE_CLASS ) ) {
			++count;
			AppendPicture( view, child );
		} else if( Widget_HasClass( child, FOLDER_CLASS ) ) {
			++count;
			AppendFolder( view, child );
		} else {
			UpdateThumbRow( view );
			++limit;
		}
	}
	if( view->layout.current ) {
		node = Widget_GetNode( view->layout.current );
		if( node->next ) {
			view->layout.current = node->next->data;
		}
	}
	return count;
}

/** 分步执行缩略图列表的布局任务 */
static void OnLayoutStep( void *arg1, void *arg2 )
{
	int n;
	LCUI_Widget w = arg1;
	ThumbView view = w->private_data;
	if( !view->layout.is_running ) {
		return;
	}
	n = ThumbView_ExecUpdateLayout( w, 16 );
	/* 如果还有未布局的缩略图则下次再继续 */
	if( n == 16 ) {
		LCUI_AppTaskRec task;
		task.arg[0] = w;
		task.arg[1] = NULL;
		task.func = OnLayoutStep;
		task.destroy_arg[0] = NULL;
		task.destroy_arg[1] = NULL;
		LCUI_PostTask( &task );
	} else {
		UpdateThumbRow( view );
		view->layout.is_running = FALSE;
		view->layout.current = NULL;
		Widget_UnlockLayout( w );
		Widget_UpdateLayout( w );
	}
}

/** 延迟执行缩略图列表的布局操作 */
static void OnDelayLayout( void *arg )
{
	LCUI_Widget w = arg;
	ThumbView view = w->private_data;
	LCUIMutex_Lock( &view->layout.row_mutex );
	view->layout.x = 0;
	view->layout.count = 0;
	view->layout.current = NULL;
	view->layout.folder_count = 0;
	view->layout.is_delaying = FALSE;
	view->layout.is_running = TRUE;
	ThumbView_UpdateLayoutContext( w );
	LinkedList_Clear( &view->layout.row, NULL );
	LCUIMutex_Unlock( &view->layout.row_mutex );
	OnLayoutStep( arg, NULL );
}

/** 更新缩略图列表的布局 */
static void ThumbView_UpdateLayout( LCUI_Widget w )
{
	ThumbView view = w->private_data;
	if( view->layout.is_running ) {
		return;
	}
	Widget_LockLayout( w );
	/* 如果已经有延迟布局任务，则重置该任务的定时 */
	if( view->layout.is_delaying ) {
		LCUITimer_Reset( view->layout.timer, 1000 );
		return;
	}
	view->layout.is_delaying = TRUE;
	view->layout.timer = LCUITimer_Set( 1000, OnDelayLayout, w, FALSE );
}

/** 在缩略图列表视图的尺寸有变更时... */
static void OnResize( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	ThumbView view = w->private_data;
	/* 如果宽度没有变化则不更新布局 */
	if( w->box.content.width == view->layout.max_width ) {
		return;
	}
	ThumbView_UpdateLayout( w );
}

/** 在布局完成后 */
static void OnAfterLayout( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	ThumbView view = w->private_data;
	ScrollLoading_Update( view->scrollload );
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
	LCUIMutex_Lock( &data->view->tasks_mutex );
	LinkedList_Append( &data->view->tasks, task );
	LCUICond_Signal( &data->view->tasks_cond );
	DEBUG_MSG("on scroll load: %s\n", task->info->path);
	LCUIMutex_Unlock( &data->view->tasks_mutex );
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
	Widget_AddClass( item, FOLDER_CLASS );
	if( !show_path ) {
		Widget_AddClass( item, "hide-path" );
	}
	Widget_AddClass( infobar, "info" );
	Widget_AddClass( name, "name" );
	Widget_AddClass( path, "path" );
	Widget_AddClass( icon, "icon mdi mdi-folder-outline" );
	TextView_SetText( name, getdirname( filepath ) );
	TextView_SetText( path, filepath );
	Widget_Append( item, infobar );
	Widget_Append( infobar, name );
	Widget_Append( infobar, path );
	Widget_Append( infobar, icon );
	Widget_BindEvent( item, "scrollload", OnScrollLoad, NULL, NULL );
	Widget_Append( w, item );
	ScrollLoading_Update2( data->view->scrollload );
	ThumbView_UpdateLayoutContext( w );
	AppendFolder( data->view, item );
	LinkedList_Append( &data->view->files, data->info.path );
	return item;
}

void ThumbView_Append( LCUI_Widget w, LCUI_Widget child )
{
	Widget_Append( w, child );
	UpdateThumbRow( w->private_data );
}

LCUI_Widget ThumbView_AppendPicture( LCUI_Widget w, const char *path )
{
	char *apath;
	wchar_t *wpath;
	ThumbItemData data;
	LCUI_Widget item, cover;
	int len, width, height;
	len = strlen( path ) + 1;
	apath = malloc( sizeof(char) * len );
	wpath = malloc( sizeof(wchar_t) * len );
	LCUI_DecodeString( wpath, path, len, ENCODING_UTF8 );
	LCUI_EncodeString( apath, wpath, len, ENCODING_ANSI );
	if( Graph_GetImageSize( apath, &width, &height ) != 0 ) {
		return NULL;
	}
	item = LCUIWidget_New( NULL );
	cover = LCUIWidget_New( NULL );
	data = Widget_NewPrivateData( item, ThumbItemDataRec );
	data->width = width;
	data->height = height;
	data->info.is_dir = FALSE;
	data->view = w->private_data;
	data->info.path = malloc( sizeof( char )*len );
	strncpy( data->info.path, path, len );
	Widget_AddClass( item, PICTURE_CLASS );
	Widget_AddClass( cover, "picture-cover" );
	Widget_BindEvent( item, "scrollload", OnScrollLoad, NULL, NULL );
	Widget_Append( item, cover );
	Widget_Append( w, item );
	ScrollLoading_Update2( data->view->scrollload );
	ThumbView_UpdateLayoutContext( w );
	AppendPicture( data->view, item );
	LinkedList_Append( &data->view->files, data->info.path );
	return item;
}

static void ThumbView_OnInit( LCUI_Widget w )
{
	ThumbView self;
	self = Widget_NewPrivateData( w, ThumbViewRec );
	self->dbs = &finder.thumb_dbs;
	self->is_loading = FALSE;
	self->is_running = TRUE;
	self->layout.x = 0;
	self->layout.count = 0;
	self->layout.max_width = 0;
	self->layout.current = NULL;
	self->layout.folder_count = 0;
	self->layout.is_running = FALSE;
	self->layout.is_delaying = FALSE;
	self->layout.folders_per_row = 1;
	LCUICond_Init( &self->tasks_cond );
	LCUIMutex_Init( &self->mutex );
	LCUIMutex_Init( &self->tasks_mutex );
	LinkedList_Init( &self->tasks );
	LinkedList_Init( &self->files );
	LinkedList_Init( &self->layout.row );
	LCUIMutex_Init( &self->layout.row_mutex );
	self->scrollload = ScrollLoading_New( w );
	self->cache = ThumbCache_New( THUMB_CACHE_SIZE, OnRemoveThumb );
	LCUIThread_Create( &self->thread, ThumbView_TaskThread, self );
	Widget_BindEvent( w, "resize", OnResize, NULL, NULL );
	Widget_BindEvent( w, "afterlayout", OnAfterLayout, NULL, NULL );
}

static void ThumbView_OnDestroy( LCUI_Widget w )
{
	ThumbView self = w->private_data;
	ThumbView_Lock( w );
	ThumbView_Empty( w );
	ScrollLoading_Delete( self->scrollload );
	self->is_running = FALSE;
	ThumbView_Unlock( w );
	LCUIThread_Join( self->thread, NULL );
}

void LCUIWidget_AddThumbView( void )
{
	LCUI_WidgetClass *wc = LCUIWidget_AddClass( "thumbview" );
	wc->methods.init = ThumbView_OnInit;
	wc->methods.destroy = ThumbView_OnDestroy;
	scrollload_event = LCUIWidget_AllocEventId();
	LCUIWidget_SetEventName( scrollload_event, "scrollload" );
}
