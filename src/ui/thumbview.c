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
#include <LCUI/graph.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "thumbview.h"

#define THUMB_TASK_MAX		50
#define SCROLLLOADING_DELAY	500
#define LAYOUT_DELAY		1000
#define ANIMATION_DELAY		750
#define ANIMATION_DURATION	500
#define THUBVIEW_MIN_WIDTH	300
#define FOLDER_MAX_WIDTH	388
#define FOLDER_FIXED_HEIGHT	134
#define PICTURE_FIXED_HEIGHT	226
/** 文件夹的右外边距，需要与 CSS 文件定义的样式一致 */
#define FOLDER_MARGIN_RIGHT	10
#define FOLDER_CLASS		"file-list-item-folder"
#define PICTURE_CLASS		"file-list-item-picture"
#define DIR_COVER_THUMB		"__dir_cover_thumb__"
#define THUMB_MAX_WIDTH		240

/** 滚动加载功能的相关数据 */
typedef struct ScrollLoadingRec_ {
	int top;			/**< 当前可见区域上边界的 Y 轴坐标 */
	int event_id;			/**< 滚动加载功能的事件ID */
	int timer;			/**< 定时器，用于实现延迟加载 */
	LCUI_BOOL is_delaying;		/**< 是否处于延迟状态 */
	LCUI_BOOL need_update;		/**< 是否需要更新 */
	LCUI_BOOL enabled;		/**< 是否启用滚动加载功能 */
	LCUI_Widget scrolllayer;	/**< 滚动层 */
	LCUI_Widget top_child;		/**< 当前可见区域第一个子部件 */
} ScrollLoadingRec, *ScrollLoading;

enum AnimationType {
	ANI_NONE,
	ANI_FADEOUT,
	ANI_FADEIN
};

/** 任务类型 */
enum ThumbViewTaskType {
	TASK_LOAD_THUMB,
	TASK_LAYOUT,
	TASK_TOTAL
};

/** 动画相关数据 */
typedef struct AnimationRec_ {
	int type;
	int timer;
	int interval;
	double opacity;
	double opacity_delta;
	LCUI_BOOL is_runing;
} AnimationRec, *Animation;

/** 缩略图列表布局功能的相关数据 */
typedef struct LayoutContextRec_ {
	int x;				/**< 当前对象的 X 轴坐标 */
	int count;			/**< 当前处理的总对象数量 */
	int max_width;			/**< 最大宽度 */
	LCUI_BOOL is_running;		/**< 是否正在布局 */
	LCUI_BOOL is_delaying;		/**< 是否处于延迟状态 */
	LCUI_Widget start;		/**< 起始处的部件 */
	LCUI_Widget current;		/**< 当前处理的部件 */
	LinkedList row;			/**< 当前行的部件 */
	LCUI_Mutex row_mutex;		/**< 当前行的互斥锁 */
	int timer;			/**< 定时器 */
	int folder_count;		/**< 当前处理的文件夹数量 */
	int folders_per_row;		/**< 每行有多少个文件夹 */
} LayoutContextRec, *LayoutContext;

typedef struct ThumbViewRec_ {
	Dict **dbs;			/**< 缩略图数据库字典，以目录路径进行索引 */
	ThumbCache cache;		/**< 缩略图缓存 */
	ThumbLinker linker;		/**< 缩略图链接器 */
	LinkedList files;		/**< 当前视图下的文件列表 */
	LinkedList thumb_tasks;		/**< 缩略图加载任务队列 */
	LCUI_Cond tasks_cond;		/**< 任务队列条件变量 */
	LCUI_Mutex tasks_mutex;		/**< 任务队列互斥锁 */
	LCUI_RBTree task_targets;	/**< 各个任务的目标部件 */
	LCUI_BOOL tasks[TASK_TOTAL];	/**< 当前任务 */
	LCUI_Mutex mutex;		/**< 互斥锁 */
	LCUI_Thread thread;		/**< 任务处理线程 */
	LCUI_BOOL is_loading;		/**< 是否处于载入中状态 */
	LCUI_BOOL is_running;		/**< 是否处于运行状态 */
	ScrollLoading scrollload;	/**< 滚动加载功能所需的相关数据 */
	LayoutContextRec layout;	/**< 缩略图布局功能所需的相关数据 */
	AnimationRec animation;		/**< 动画相关数据，用于实现淡入淡出的动画效果 */
	void (*onlayout)(LCUI_Widget);	/**< 回调函数，当布局开始时调用 */
} ThumbViewRec, *ThumbView;

/** 缩略图列表项的数据 */
typedef struct ThumbViewItemRec_ {
	char *path;					/**< 路径 */
	DB_File file;					/**< 文件信息 */
	ThumbView view;					/**< 所属缩略图列表视图 */
	LCUI_Widget cover;				/**< 遮罩层部件 */
	LCUI_BOOL is_dir;				/**< 是否为目录 */
	void (*unsetthumb)(LCUI_Widget);		/**< 取消缩略图 */
	void (*setthumb)(LCUI_Widget, LCUI_Graph*);	/**< 设置缩略图 */
	void(*updatesize)(LCUI_Widget);			/**< 更新自身尺寸 */
} ThumbViewItemRec, *ThumbViewItem;

static int scrollload_event = -1;

static int ScrollLoading_OnUpdate( ScrollLoading ctx )
{
	LCUI_Widget w;
	LinkedList list;
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
	LinkedList_Init( &list );
	node = Widget_GetNode( ctx->top_child );
	while( node ) {
		w = node->data;
		if( w->box.border.y > bottom ) {
			break;
		}
		if( w->box.border.y + w->box.border.height >= top ) {
			LinkedList_Append( &list, w );
			++count;
		}
		node = node->next;
	}
	for( LinkedList_Each( node, &list ) ) {
		w = node->data;
		Widget_TriggerEvent( w, &e, &count );
	}
	return list.length;
}

static void ScrollLoading_OnDelayUpdate( void *arg )
{
	ScrollLoading ctx = arg;
	ScrollLoading_OnUpdate( ctx );
	if( ctx->need_update ) {
		ctx->timer = LCUITimer_Set( SCROLLLOADING_DELAY, 
					    ScrollLoading_OnDelayUpdate, 
					    ctx, FALSE );;
		ctx->need_update = FALSE;
	} else {
		ctx->timer = -1;
		ctx->is_delaying = FALSE;
	}
	DEBUG_MSG("ctx->top: %d\n", ctx->top);
}

static void ScrollLoading_Update( ScrollLoading ctx )
{
	ctx->need_update = TRUE;
	if( ctx->is_delaying && ctx->timer > 0 ) {
		return;
	}
	ctx->timer = LCUITimer_Set( SCROLLLOADING_DELAY, 
				    ScrollLoading_OnDelayUpdate, 
				    ctx, FALSE );
	ctx->is_delaying = TRUE;
}

static void ScrollLoading_OnScroll( LCUI_Widget w, LCUI_WidgetEvent e,
				    void *arg )
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
	ctx->enabled = TRUE;
	ctx->top_child = NULL;
	ctx->need_update = FALSE;
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
	terms.for_tree = TRUE;
	terms.create_time = DESC;
	query = DB_NewQuery( &terms );
	total = DBQuery_GetTotalFiles( query );
	if( total > 0 ) {
		file = DBQuery_FetchFile( query );
		strcpy( filepath, file->path );
	}
	return total;
}

static void ThumbViewItem_SetThumb( LCUI_Widget item, LCUI_Graph *thumb )
{
	Widget_Lock( item );
	SetStyle( item->custom_style, key_background_image, thumb, image );
	Widget_UpdateStyle( item, FALSE );
	Widget_Unlock( item );
}

static void ThumbViewItem_UnsetThumb( LCUI_Widget item )
{
	Widget_Lock( item );
	DEBUG_MSG("remove thumb\n");
	Graph_Init( &item->computed_style.background.image );
	item->custom_style->sheet[key_background_image].is_valid = FALSE;
	Widget_UpdateStyle( item, FALSE );
	Widget_Unlock( item );
}

void ThumbViewItem_BindFile( LCUI_Widget item, DB_File file )
{
	ThumbViewItem data = item->private_data;
	data->is_dir = FALSE;
	data->file = file;
	data->path = data->file->path;
}

DB_File ThumbViewItem_GetFile( LCUI_Widget item )
{
	ThumbViewItem data = item->private_data;
	if( data->is_dir ) {
		return NULL;
	}
	return data->file;
}

void ThumbViewItem_AppendToCover(LCUI_Widget item, LCUI_Widget child )
{
	ThumbViewItem data = item->private_data;
	if( !data->is_dir ) {
		Widget_Append( data->cover, child );
	}
}

void ThumbViewItem_SetFunction( LCUI_Widget item,
				void( *setthumb )(LCUI_Widget, LCUI_Graph*),
				void( *unsetthumb )(LCUI_Widget),
				void( *updatesize )(LCUI_Widget) )
{
	ThumbViewItem data = item->private_data;
	data->setthumb = setthumb;
	data->unsetthumb = unsetthumb;
	data->updatesize = updatesize;
}

/** 当移除缩略图的时候 */
static void OnRemoveThumb( void *data )
{
	LCUI_Widget w = data;
	ThumbViewItem item = w->private_data;
	if( item->unsetthumb ) {
		item->unsetthumb( w );
	}
}

/** 载入缩略图 */
static LCUI_Graph *LoadThumb( ThumbView view, LCUI_Widget target )
{
	Dict *dbs;
	DB_Dir dir;
	ThumbDB db;
	uint_t mtime;
	int len, ret;
	ThumbDataRec tdata;
	ThumbViewItem item;
	const char *filename;
	wchar_t wpath[PATH_LEN];
	char apath[PATH_LEN], path[PATH_LEN];
	item = target->private_data;
	dir = LCFinder_GetSourceDir( item->path );
	if( !dir ) {
		return NULL;
	}
	dbs = *view->dbs;
	db = Dict_FetchValue( dbs, dir->path );
	if( !db ) {
		return NULL;
	}
	len = strlen( dir->path );
	if( item->is_dir ) {
		pathjoin( path, item->path, "" );
		if( GetDirThumbFilePath( path, path ) == 0 ) {
			return NULL;
		}
		/* 将路径的编码由 UTF-8 解码成 Unicode */
		LCUI_DecodeString( wpath, path, PATH_LEN, ENCODING_UTF8 );
		pathjoin( path, path, DIR_COVER_THUMB );
		filename = path + len;
	} else {
		filename = item->path + len;
		if( filename[0] == PATH_SEP ) {
			++filename;
		}
		LCUI_DecodeString( wpath, item->path, 
				   PATH_LEN, ENCODING_UTF8 );
	}
	mtime = (uint_t)wgetfilemtime( wpath );
	LCUI_EncodeString( apath, wpath, PATH_LEN, ENCODING_ANSI );
	ret = ThumbDB_Load( db, filename, &tdata );
	DEBUG_MSG( "load thumb from: %s\n", filename );
	if( ret != 0 || tdata.modify_time != mtime ) {
		LCUI_Graph img;
		Graph_Init( &img );
		Graph_Init( &tdata.graph );
		if( Graph_LoadImage( apath, &img ) != 0 ) {
			return NULL;
		}
		tdata.origin_width = img.width;
		tdata.origin_height = img.height;
		tdata.modify_time = mtime;
		if( img.width > THUMB_MAX_WIDTH ) {
			Graph_Zoom( &img, &tdata.graph, TRUE,
				    THUMB_MAX_WIDTH, 0 );
			Graph_Free( &img );
		} else {
			tdata.graph = img;
		}
		DEBUG_MSG( "save thumb, size: (%d,%d), name: %s, db: %p\n",
			   tdata.graph.width, tdata.graph.height, 
			   filename, db );
		ThumbDB_Save( db, filename, &tdata );
	}
	if( !item->is_dir ) {
		if( tdata.origin_width > 0 && tdata.origin_height > 0 &&
			(item->file->width != tdata.origin_width ||
			  item->file->height != tdata.origin_height) ) {
			DBFile_SetSize( item->file, tdata.origin_width,
					tdata.origin_height );
		}
	}
	ThumbCache_Add( view->cache, item->path, &tdata.graph );
	return ThumbCache_Link( view->cache, item->path, 
				view->linker, target );
}

/** 执行加载缩略图的任务 */
static void ThumbView_ExecLoadThumb( LCUI_Widget w, LCUI_Widget target )
{
	LCUI_Graph *thumb;
	ThumbView view = w->private_data;
	ThumbViewItem data = target->private_data;
	thumb = ThumbCache_Link( view->cache, data->path,
				 view->linker, target );
	if( !thumb ) {
		thumb = LoadThumb( view, target );
		if( !thumb ) {
			return;
		}
	}
	if( data->setthumb ) {
		data->setthumb( target, thumb );
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
	view->layout.start = NULL;
	view->layout.current = NULL;
	view->layout.folder_count = 0;
	LinkedList_Clear( &view->thumb_tasks, NULL );
	LinkedList_Clear( &view->layout.row, NULL );
	ThumbView_UpdateLayoutContext( w );
	for( LinkedList_Each( node, &view->files ) ) {
		ThumbCache_Unlink( view->cache, view->linker, node->data );
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
	LCUI_Widget item;
	ThumbViewItem data;
	LinkedListNode *node;
	int overflow_width, width, thumb_width, rest_width;
	if( view->layout.max_width <= THUBVIEW_MIN_WIDTH ) {
		return;
	}
	overflow_width = view->layout.x - view->layout.max_width;
	/**
	 * 如果这一行缩略图的总宽度有溢出（超出最大宽度），则根据缩略图宽度占总宽度
	 * 的比例，分别缩减相应的宽度。
	 */
	if( overflow_width <= 0 ) {
		LinkedList_Clear( &view->layout.row, NULL );
		view->layout.x = 0;
		return;
	}
	rest_width = overflow_width;
	LCUIMutex_Lock( &view->layout.row_mutex );
	for( LinkedList_Each( node, &view->layout.row ) ) {
		int w, h;
		item = node->data;
		data = item->private_data;
		w = data->file->width > 0 ? data->file->width : 226;
		h = data->file->height > 0 ? data->file->height : 226;
		thumb_width = PICTURE_FIXED_HEIGHT;
		thumb_width = thumb_width * w / h;
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
		SetStyle( item->custom_style, key_width, width, px );
		Widget_UpdateStyle( item, FALSE );
	}
	LinkedList_Clear( &view->layout.row, NULL );
	LCUIMutex_Unlock( &view->layout.row_mutex );
	view->layout.x = 0;
}

/* 根据当前布局更新图片尺寸 */
static void UpdatePictureSize( LCUI_Widget item )
{
	int width, w, h;
	ThumbViewItem data = item->private_data;
	ThumbView view = data->view;
	data = item->private_data;
	w = data->file->width > 0 ? data->file->width : 226;
	h = data->file->height > 0 ? data->file->height : 226;
	width = PICTURE_FIXED_HEIGHT * w / h;
	SetStyle( item->custom_style, key_width, width, px );
	Widget_UpdateStyle( item, FALSE );
	view->layout.x += width;
	LinkedList_Append( &view->layout.row, item );
	if( view->layout.x >= view->layout.max_width ) {
		UpdateThumbRow( view );
	}
}

/* 根据当前布局更新文件夹尺寸 */
static void UpdateFolderSize( LCUI_Widget item )
{
	int width, n;
	ThumbViewItem data = item->private_data;
	ThumbView view = data->view;
	++view->layout.folder_count;
	UpdateThumbRow( view );
	if( view->layout.max_width < THUBVIEW_MIN_WIDTH ) {
		item->custom_style->sheet[key_width].is_valid = FALSE;
		Widget_AddClass( item, "full-width" );
		return;
	} else {
		Widget_RemoveClass( item, "full-width" );
	}
	n = view->layout.folders_per_row;
	width = view->layout.max_width;
	width = (width - FOLDER_MARGIN_RIGHT*(n - 1)) / n;
	/* 设置每行最后一个文件夹的右边距为 0px */
	if( view->layout.folder_count % n == 0 ) {
		SetStyle( item->custom_style, key_margin_right, 0, px );
	} else {
		item->custom_style->sheet[key_margin_right].is_valid = FALSE;
	}
	SetStyle( item->custom_style, key_width, width, px );
	Widget_UpdateStyle( item, FALSE );
}

/** 更新用于布局的游标，以确保能够从正确的位置开始排列部件 */
static void ThumbView_UpdateLayoutCursor( LCUI_Widget w )
{
	LCUI_Widget child, prev;
	ThumbView view = w->private_data;
	if( view->layout.current ) {
		return;
	}
	if( !view->layout.start ) {
		if( w->children.length > 0 ) {
			view->layout.current = w->children.head.next->data;
		}
		return;
	}
	child = view->layout.start;
	prev= Widget_GetPrev( child );
	while( prev ) {
		if( prev->computed_style.position == SV_ABSOLUTE ||
		    prev->computed_style.display == SV_NONE ) {
			prev = Widget_GetPrev( prev );
			continue;
		}
		if( (child->x == 0 && child->y > prev->y) ||
		    prev->computed_style.display == SV_BLOCK ) {
			break;
		}
		child = prev;
		prev = Widget_GetPrev( prev );
	}
	view->layout.start = child;
	view->layout.current = child;
}

/** 直接执行布局更新操作 */
static int ThumbView_OnUpdateLayout( LCUI_Widget w, int limit )
{
	int count;
	LCUI_Widget child;
	ThumbView view = w->private_data;
	ThumbView_UpdateLayoutCursor( w );
	child = view->layout.current;
	for( count = 0; child && --limit >= 0; child = Widget_GetNext(child) ) {
		ThumbViewItem item;
		item = child->private_data;
		view->layout.count += 1;
		if( !child->computed_style.visible ) {
			++limit;
			continue;
		}
		if( child->type && !strcmp(child->type, "thumbviewitem") ) {
			++count;
			view->layout.current = child;
			item->updatesize( child );
			continue;
		}
		UpdateThumbRow( view );
		++limit;
	}
	if( view->layout.current ) {
		view->layout.current = Widget_GetNext( view->layout.current );
	}
	return count;
}

/** 在布局完成后 */
static void OnAfterLayout( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	ThumbView view = w->private_data;
	ScrollLoading_Update( view->scrollload );
	Widget_UnbindEvent( w, "afterlayout", OnAfterLayout );
}

/** 执行缩略图列表的布局任务 */
static void ThumbView_ExecUpdateLayout( LCUI_Widget w )
{
	int n;
	ThumbView view = w->private_data;
	if( !view->layout.is_running ) {
		return;
	}
	n = ThumbView_OnUpdateLayout( w, 16 );
	/* 如果还有未布局的缩略图则下次再继续 */
	if( n == 16 ) {
		view->tasks[TASK_LAYOUT] = TRUE;
		LCUICond_Signal( &view->tasks_cond );
	} else {
		UpdateThumbRow( view );
		view->layout.is_running = FALSE;
		view->layout.current = NULL;
		Widget_UnlockLayout( w );
		Widget_UpdateLayout( w );
		Widget_BindEvent( w, "afterlayout", 
				  OnAfterLayout, NULL, NULL );
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
	if( view->onlayout ) {
		view->onlayout( w );
	}
	view->tasks[TASK_LAYOUT] = TRUE;
	LCUICond_Signal( &view->tasks_cond );
}

static void OnSetOpacity( void *arg )
{
	LCUI_Widget w = arg;
	ThumbView view = w->private_data;
	Animation ani = &view->animation;
	switch( ani->type ) {
	case ANI_FADEOUT:
		ani->opacity -= ani->opacity_delta;
		if( ani->opacity <= 0.0 ) {
			ani->opacity = 0.0;
			ani->type = ANI_FADEIN;
		}
		break;
	case ANI_FADEIN:
		ani->opacity += ani->opacity_delta;
		if( ani->opacity >= 1.0 ) {
			ani->opacity = 1.0;
			ani->type = ANI_NONE;
		}
		break;
	case ANI_NONE:
	default:
		ani->opacity = 1.0;
		ani->is_runing = FALSE;
		LCUITimer_Free( ani->timer );
		ani->timer = -1;
		break;
	}
	SetStyle( w->custom_style, key_opacity, (float)ani->opacity, scale );
	Widget_UpdateStyle( w, FALSE );
}

static void OnStartAnimation( void *arg )
{
	LCUI_Widget w = arg;
	ThumbView view = w->private_data;
	view->animation.type = ANI_FADEOUT;
	view->animation.is_runing = TRUE;
	view->animation.timer = LCUITimer_Set( view->animation.interval, 
					       OnSetOpacity, w, TRUE );
	
}

void ThumbView_UpdateLayout( LCUI_Widget w, LCUI_Widget start_child )
{
	ThumbView view = w->private_data;
	if( view->layout.is_running ) {
		return;
	}
	Widget_LockLayout( w );
	view->animation.type = ANI_NONE;
	if( view->animation.timer > 0 ) {
		LCUITimer_Reset( view->animation.timer, ANIMATION_DELAY );
	} else {
		view->animation.timer = LCUITimer_Set( ANIMATION_DELAY, 
						       OnStartAnimation, 
						       w, FALSE );
	}
	view->layout.start = start_child;
	/* 如果已经有延迟布局任务，则重置该任务的定时 */
	if( view->layout.is_delaying ) {
		LCUITimer_Reset( view->layout.timer, LAYOUT_DELAY );
		return;
	}
	view->layout.is_delaying = TRUE;
	view->layout.timer = LCUITimer_Set( LAYOUT_DELAY, 
					    OnDelayLayout, w, FALSE );
}

/** 在缩略图列表视图的尺寸有变更时... */
static void OnResize( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	ThumbView view = w->private_data;
	/* 如果宽度没有变化则不更新布局 */
	if( w->box.content.width == view->layout.max_width ) {
		return;
	}
	ThumbView_UpdateLayout( w, NULL );
}

static void OnScrollLoad( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	LCUI_Widget target;
	ThumbViewItem data = w->private_data;
	LinkedList *tasks = &data->view->thumb_tasks;
	if( w->custom_style->sheet[key_background_image].is_valid ||
	    !data || !data->view->cache ) {
		return;
	}
	LCUIMutex_Lock( &data->view->tasks_mutex );
	/* 如果待处理的任务数量超过最大限制，则移除最后一个任务 */
	if( tasks->length >= THUMB_TASK_MAX ) {
		LinkedListNode *node = LinkedList_GetNode( tasks, -1 );
		LinkedList_Unlink( tasks, node );
		target = node->data;
		RBTree_CustomErase( &data->view->task_targets, target );
	} else {
		/* 如果已经存在目标为该部件的任务，则不重复添加 */
		if( RBTree_CustomSearch( &data->view->task_targets, w ) ) {
			LCUIMutex_Unlock( &data->view->tasks_mutex );
			return;
		}
	}
	LinkedList_Insert( &data->view->thumb_tasks, 0, w );
	RBTree_CustomInsert( &data->view->task_targets, w, w );
	data->view->tasks[TASK_LOAD_THUMB] = TRUE;
	/* 通知任务线程处理该任务 */
	LCUICond_Signal( &data->view->tasks_cond );
	LCUIMutex_Unlock( &data->view->tasks_mutex );
	DEBUG_MSG( "on scroll load: %s\n", task->info->path );
}

void ThumbView_EnableScrollLoading( LCUI_Widget w )
{
	ThumbView view = w->private_data;
	ScrollLoading_Reset( view->scrollload );
	ScrollLoading_Update( view->scrollload );
	ScrollLoading_Enable( view->scrollload, TRUE );
}

void ThumbView_DisableScrollLoading( LCUI_Widget w )
{
	ThumbView view = w->private_data;
	ScrollLoading_Enable( view->scrollload, FALSE );
}

LCUI_Widget ThumbView_AppendFolder( LCUI_Widget w, const char *filepath, 
				    LCUI_BOOL show_path )
{
	ThumbViewItem data;
	int len = strlen( filepath ) + 1;
	LCUI_Widget item = LCUIWidget_New( "thumbviewitem" );
	LCUI_Widget name = LCUIWidget_New( "textview" );
	LCUI_Widget path = LCUIWidget_New( "textview" );
	LCUI_Widget icon = LCUIWidget_New( "textview" );
	LCUI_Widget infobar = LCUIWidget_New( NULL );
	data = item->private_data;
	data->path = malloc( sizeof( char )*len );
	strncpy( data->path, filepath, len );
	data->view = w->private_data;
	data->is_dir = TRUE;
	data->setthumb = ThumbViewItem_SetThumb;
	data->unsetthumb = ThumbViewItem_UnsetThumb;
	data->updatesize = UpdateFolderSize;
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
	Widget_Append( w, item );
	ScrollLoading_Update( data->view->scrollload );
	ThumbView_UpdateLayoutContext( w );
	data->view->layout.current = w;
	UpdateFolderSize( item );
	LinkedList_Append( &data->view->files, data->path );
	return item;
}

LCUI_Widget ThumbView_AppendPicture( LCUI_Widget w, DB_File file )
{
	LCUI_Widget item;
	ThumbViewItem data;
	item = LCUIWidget_New( "thumbviewitem" );
	data = item->private_data;
	data->is_dir = FALSE;
	data->view = w->private_data;
	data->file = DBFile_Dup( file );
	data->path = data->file->path;
	data->cover = LCUIWidget_New( NULL );
	data->setthumb = ThumbViewItem_SetThumb;
	data->unsetthumb = ThumbViewItem_UnsetThumb;
	data->updatesize = UpdatePictureSize;
	Widget_AddClass( item, PICTURE_CLASS );
	Widget_AddClass( data->cover, "picture-cover" );
	Widget_Append( item, data->cover );
	Widget_Append( w, item );
	ScrollLoading_Update( data->view->scrollload );
	ThumbView_UpdateLayoutContext( w );
	data->view->layout.current = w;
	UpdatePictureSize( item );
	LinkedList_Append( &data->view->files, data->path );
	return item;
}

void ThumbView_Append( LCUI_Widget w, LCUI_Widget child )
{
	ThumbView view = w->private_data;
	Widget_Append( w, child );
	if( child->type && strcmp( child->type, "thumbviewitem" ) == 0 ) {
		ThumbViewItem item = child->private_data;
		item->view = view;
		if( item->updatesize ) {
			item->updatesize( child );
		}
		ScrollLoading_Update( view->scrollload );
	} else {
		UpdateThumbRow( view );
	}
}

void ThumbView_SetCache( LCUI_Widget w, ThumbCache cache )
{
	ThumbView view = w->private_data;
	if( view->cache ) {
		ThumbCache_DeleteLinker( view->cache, view->linker );
	}
	view->linker = ThumbCache_AddLinker( cache, OnRemoveThumb );
	view->cache = cache;
}

static int OnCompareTaskTarget( void *data, const void *keydata )
{
	if( data < keydata ) {
		return -1;
	} else if( data == keydata ) {
		return 0;
	} else {
		return 1;
	}
}

static void ThumbView_ExecTask( LCUI_Widget w, int task )
{
	ThumbView view;
	LCUI_Widget target;
	LinkedListNode *node;
	view = w->private_data;
	switch( task ) {
	case TASK_LOAD_THUMB:
		LCUIMutex_Lock( &view->tasks_mutex );
		node = LinkedList_GetNode( &view->thumb_tasks, 0 );
		if( !node ) {
			LCUIMutex_Unlock( &view->tasks_mutex );
			break;
		}
		target = node->data;
		LinkedList_Unlink( &view->thumb_tasks, node );
		RBTree_CustomErase( &view->task_targets, target );
		LCUIMutex_Unlock( &view->tasks_mutex );
		ThumbView_ExecLoadThumb( w, target );
		if( view->thumb_tasks.length > 0 ) {
			view->tasks[TASK_LOAD_THUMB] = TRUE;
		}
		free( node );
		break;
	case TASK_LAYOUT:
		ThumbView_ExecUpdateLayout( w );
	default:break;
	}
}

/** 缩略图加载任务处理线程 */
static void ThumbView_TaskThread( void *arg )
{
	ThumbView view;
	LCUI_BOOL shown = TRUE;
	LCUI_Widget parent, w = arg;
	view = w->private_data;
	while( view->is_running ) {
		int i, count;
		view->is_loading = TRUE;
		LCUIMutex_Lock( &view->mutex );
		for( i = 0, count = 0; i < TASK_TOTAL; ++i ) {
			if( view->tasks[i] ) {
				view->tasks[i] = FALSE;
				ThumbView_ExecTask( w, i );
				if( !view->tasks[i] ) {
					++count;
				}
			} else {
				++count;
			}
		}
		LCUIMutex_Unlock( &view->mutex );
		if( count == TASK_TOTAL ) {
			view->is_loading = FALSE;
			LCUIMutex_Lock( &view->tasks_mutex );
			LCUICond_TimedWait( &view->tasks_cond, 
					    &view->tasks_mutex, 500 );
			LCUIMutex_Unlock( &view->tasks_mutex );
		}
		/* 检查自己及父级部件是否可见 */
		for( parent = w; parent; parent->parent ) {
			if( !parent->computed_style.visible ) {
				shown = FALSE;
				break;
			}
			parent = parent->parent;
		}
		/* 如果当前可见但之前不可见，则刷新当前可见区域内加载的缩略图 */
		if( !parent && !shown ) {
			ScrollLoading_Update( view->scrollload );
			shown = TRUE;
		}
	}
}

static void ThumbViewItem_OnInit( LCUI_Widget w )
{
	ThumbViewItem self;
	self = Widget_NewPrivateData( w, ThumbViewItemRec );
	self->file = NULL;
	self->is_dir = FALSE;
	self->path = NULL;
	self->view = NULL;
	self->setthumb = NULL;
	self->unsetthumb = NULL;
	self->updatesize = NULL;
	Widget_BindEvent( w, "scrollload", OnScrollLoad, NULL, NULL );
}

static void ThumbViewItem_OnDestroy( LCUI_Widget w )
{
	ThumbViewItem self;
	self = w->private_data;
	self->unsetthumb( w );
	ThumbCache_Unlink( self->view->cache, 
			   self->view->linker, self->path );
	if( self->is_dir ) {
		if( self->path ) {
			free( self->path );
		}
	} else {
		DBFile_Release( self->file );
	}
	self->file = NULL;
	self->view = NULL;
	self->path = NULL;
	self->unsetthumb = NULL;
	self->setthumb = NULL;
}

void ThumbView_OnLayout( LCUI_Widget w, void (*func)(LCUI_Widget) )
{
	ThumbView view = w->private_data;
	view->onlayout = func;
}

static void ThumbView_OnInit( LCUI_Widget w )
{
	ThumbView self;
	self = Widget_NewPrivateData( w, ThumbViewRec );
	self->dbs = &finder.thumb_dbs;
	self->is_loading = FALSE;
	self->is_running = TRUE;
	self->onlayout = NULL;
	self->layout.x = 0;
	self->cache = NULL;
	self->linker = NULL;
	self->layout.count = 0;
	self->layout.max_width = 0;
	self->layout.start = NULL;
	self->layout.current = NULL;
	self->layout.folder_count = 0;
	self->layout.is_running = FALSE;
	self->layout.is_delaying = FALSE;
	self->layout.folders_per_row = 1;
	self->animation.timer = -1;
	self->animation.type = ANI_NONE;
	self->animation.interval = 20;
	self->animation.opacity = 1.0;
	self->animation.is_runing = FALSE;
	self->animation.opacity_delta = 1.0 / (ANIMATION_DURATION / 2 / 20);
	memset( self->tasks, 0, sizeof( self->tasks) );
	LCUICond_Init( &self->tasks_cond );
	LCUIMutex_Init( &self->tasks_mutex );
	LCUIMutex_Init( &self->mutex );
	LinkedList_Init( &self->files );
	LinkedList_Init( &self->thumb_tasks );
	LinkedList_Init( &self->layout.row );
	RBTree_Init( &self->task_targets );
	RBTree_OnCompare( &self->task_targets, OnCompareTaskTarget );
	LCUIMutex_Init( &self->layout.row_mutex );
	self->scrollload = ScrollLoading_New( w );
	Widget_BindEvent( w, "resize", OnResize, NULL, NULL );
	LCUIThread_Create( &self->thread, ThumbView_TaskThread, w );
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
	wc = LCUIWidget_AddClass( "thumbviewitem" );
	wc->methods.init = ThumbViewItem_OnInit;
	wc->methods.destroy = ThumbViewItem_OnDestroy;
}
