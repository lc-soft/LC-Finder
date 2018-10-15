/* ***************************************************************************
 * thumbview.c -- thumbnail list view
 *
 * Copyright (C) 2016-2018 by Liu Chao <lc-soft@live.cn>
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
 * 版权所有 (C) 2016-2018 归属于 刘超 <lc-soft@live.cn>
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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "finder.h"
#include "file_storage.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/graph.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "thumbview.h"
#include "animation.h"

/* clang-format off */

#define THUMB_TASK_MAX		32
#define SCROLLLOADING_DELAY	500
#define LAYOUT_DELAY		1000
#define ANIMATION_DELAY		750
#define ANIMATION_DURATION	1000
#define THUMBVIEW_MIN_WIDTH	300
#define FOLDER_MAX_WIDTH	388
#define FOLDER_FIXED_HEIGHT	134
#define PICTURE_FIXED_HEIGHT	226
/** 文件列表的外间距，需要与 CSS 文件定义的样式一致 */
#define FILE_ITEM_MARGIN	6
#define FOLDER_CLASS		"file-folder"
#define PICTURE_CLASS		"file-picture"
#define DIR_COVER_THUMB		"__dir_cover_thumb__"
#define THUMB_MAX_WIDTH		240

/** 滚动加载功能的相关数据 */
typedef struct ScrollLoadingRec_ {
	float top;			/**< 当前可见区域上边界的 Y 轴坐标 */
	int event_id;			/**< 滚动加载功能的事件ID */
	int timer;			/**< 定时器，用于实现延迟加载 */
	LCUI_BOOL is_delaying;		/**< 是否处于延迟状态 */
	LCUI_BOOL need_update;		/**< 是否需要更新 */
	LCUI_BOOL enabled;		/**< 是否启用滚动加载功能 */
	LCUI_Widget scrolllayer;	/**< 滚动层 */
	LCUI_Widget top_child;		/**< 当前可见区域第一个子部件 */
} ScrollLoadingRec, *ScrollLoading;

/** 任务类型 */
enum ThumbViewTaskType {
	TASK_LOAD_THUMB, /**< 加载缩略图 */
	TASK_LAYOUT,     /**< 处理布局 */
	TASK_TOTAL
};

/** 缩略图列表布局功能的相关数据 */
typedef struct LayoutContextRec_ {
	float x;			/**< 当前对象的 X 轴坐标 */
	int count;			/**< 当前处理的总对象数量 */
	float max_width;		/**< 最大宽度 */
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

typedef struct ThumbViewRec_ *ThumbView;
typedef struct ThumbLoaderRec_ *ThumbLoader;
typedef void (*ThumbLoaderCallback)(ThumbLoader);

/** 缩略图加载器的数据结构 */
typedef struct ThumbLoaderRec_ {
	LCUI_BOOL active;		/**< 是否处于活动状态 */
	ThumbDB db;			/**< 缩略图缓存数据库 */
	ThumbView view;			/**< 所属缩略图视图 */
	LCUI_Widget target;		/**< 需要缩略图的部件 */
	LCUI_Mutex mutex;		/**< 互斥锁 */
	LCUI_Cond cond;			/**< 条件变量 */
	char path[PATH_LEN];		/**< 图片文件路径，相对于源文件夹 */
	char fullpath[PATH_LEN];	/**< 图片文件的完整路径 */
	wchar_t *wfullpath;		/**< 图片文件路径（宽字符版） */
	void *data;			/**< 传给回调函数的附加参数 */
	ThumbLoaderCallback callback;	/**< 回调函数 */
} ThumbLoaderRec;

/** 任务状态 */
enum ThumbViewTaskState {
	TASK_STATE_FINISHED,
	TASK_STATE_RUNNING,
	TASK_STATE_READY
};

/** 任务 */
typedef struct ThumbViewTaskRec_ {
	int state;
	union {
		ThumbLoader loader;
		void *data;
	};
} ThumbViewTaskRec, *ThumbViewTask;

typedef struct ThumbViewRec_ {
	int storage;				/**< 文件存储服务的连接标识符 */
	Dict **dbs;				/**< 缩略图数据库字典，以目录路径进行索引 */
	ThumbCache cache;			/**< 缩略图缓存 */
	ThumbLinker linker;			/**< 缩略图链接器 */
	LinkedList thumb_tasks;			/**< 缩略图加载任务队列 */
	LCUI_Cond tasks_cond;			/**< 任务队列条件变量 */
	LCUI_Mutex tasks_mutex;			/**< 任务队列互斥锁 */
	ThumbViewTaskRec tasks[TASK_TOTAL];	/**< 当前任务 */
	LCUI_Mutex mutex;			/**< 互斥锁 */
	LCUI_Thread thread;			/**< 任务处理线程 */
	LCUI_BOOL is_loading;			/**< 是否处于载入中状态 */
	LCUI_BOOL is_running;			/**< 是否处于运行状态 */
	ScrollLoading scrollload;		/**< 滚动加载功能所需的相关数据 */
	LayoutContextRec layout;		/**< 缩略图布局功能所需的相关数据 */
	AnimationRec animation;			/**< 动画相关数据，用于实现淡入淡出的动画效果 */
	void (*onlayout)(LCUI_Widget);		/**< 回调函数，当布局开始时调用 */
} ThumbViewRec;

/** 缩略图列表项的数据 */
typedef struct ThumbViewItemRec_ {
	char *path;                     		/**< 路径 */
	DB_File file;                   		/**< 文件信息 */
	ThumbView view;                 		/**< 所属缩略图列表视图 */
	ThumbLoader loader;             		/**< 缩略图加载器实例 */
	LCUI_Widget cover;              		/**< 遮罩层部件 */
	LCUI_BOOL is_dir;               		/**< 是否为目录 */
	LCUI_BOOL is_valid;             		/**< 是否有效 */
	void (*unsetthumb)(LCUI_Widget); 		/**< 取消缩略图 */
	void (*setthumb)(LCUI_Widget, LCUI_Graph *);	/**< 设置缩略图 */
	void (*updatesize)(LCUI_Widget);          	/**< 更新自身尺寸 */
} ThumbViewItemRec, *ThumbViewItem;

static struct ThumbViewModule {
	LCUI_WidgetPrototype main; /**< 缩略图视图的原型 */
	LCUI_WidgetPrototype item; /**< 缩略图视图列表项的原型 */
	int event_scrollload;      /**< 滚动加载事件的标识号 */
} self = { 0 };

/* clang-format on */

static int ScrollLoading_OnUpdate(ScrollLoading ctx)
{
	LCUI_Widget w;
	LinkedList list;
	LinkedListNode *node;
	LCUI_WidgetEventRec e = { 0 };
	int count = 0;
	float top, bottom;

	/* 若未启用滚动加载，或滚动层的高度过低，则本次不处理滚动加载 */
	if (!ctx->enabled || ctx->scrolllayer->height < 64) {
		return 0;
	}
	e.type = ctx->event_id;
	e.cancel_bubble = TRUE;
	bottom = top = (float)ctx->top;
	bottom += ctx->scrolllayer->parent->box.padding.height;
	if (!ctx->top_child) {
		node = ctx->scrolllayer->children.head.next;
		if (node) {
			ctx->top_child = node->data;
		}
	}
	if (!ctx->top_child) {
		return 0;
	}
	if (ctx->top_child->box.border.y > bottom) {
		node = &ctx->top_child->node;
		ctx->top_child = NULL;
		while (node) {
			w = node->data;
			if (w && w->state == LCUI_WSTATE_NORMAL) {
				if (w->box.border.y < bottom) {
					ctx->top_child = w;
					break;
				}
			}
			node = node->prev;
		}
		if (!ctx->top_child) {
			return 0;
		}
	}
	LinkedList_Init(&list);
	node = &ctx->top_child->node;
	while (node) {
		w = node->data;
		if (w->box.border.y > bottom) {
			break;
		}
		if (w->box.border.y + w->box.border.height >= top) {
			LinkedList_Append(&list, w);
			++count;
		}
		node = node->next;
	}
	for (LinkedList_Each(node, &list)) {
		w = node->data;
		Widget_TriggerEvent(w, &e, &count);
	}
	LinkedList_Clear(&list, NULL);
	return count;
}

static void ScrollLoading_OnDelayUpdate(void *arg)
{
	ScrollLoading ctx = arg;
	ScrollLoading_OnUpdate(ctx);
	if (ctx->need_update) {
		ctx->timer =
		    LCUITimer_Set(SCROLLLOADING_DELAY,
				  ScrollLoading_OnDelayUpdate, ctx, FALSE);
		;
		ctx->need_update = FALSE;
	} else {
		ctx->timer = -1;
		ctx->is_delaying = FALSE;
	}
	DEBUG_MSG("ctx->top: %d\n", ctx->top);
}

static void ScrollLoading_Update(ScrollLoading ctx)
{
	ctx->need_update = TRUE;
	if (ctx->is_delaying && ctx->timer > 0) {
		return;
	}
	ctx->timer = LCUITimer_Set(SCROLLLOADING_DELAY,
				   ScrollLoading_OnDelayUpdate, ctx, FALSE);
	ctx->is_delaying = TRUE;
}

static void ScrollLoading_OnScroll(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	float *scroll_pos = arg;
	ScrollLoading ctx = e->data;
	ctx->top = *scroll_pos;
	ScrollLoading_Update(ctx);
}

/** 新建一个滚动加载功能实例 */
static ScrollLoading ScrollLoading_New(LCUI_Widget scrolllayer)
{
	ScrollLoading ctx = NEW(ScrollLoadingRec, 1);
	ctx->top = 0;
	ctx->timer = -1;
	ctx->enabled = TRUE;
	ctx->top_child = NULL;
	ctx->need_update = FALSE;
	ctx->is_delaying = FALSE;
	ctx->scrolllayer = scrolllayer;
	ctx->event_id = self.event_scrollload;
	Widget_BindEvent(scrolllayer, "scroll", ScrollLoading_OnScroll, ctx,
			 NULL);
	return ctx;
}

/** 删除滚动加载功能实例 */
static void ScrollLoading_Delete(ScrollLoading ctx)
{
	ctx->top = 0;
	ctx->top_child = NULL;
	Widget_UnbindEvent(ctx->scrolllayer, "scroll", ScrollLoading_OnScroll);
	free(ctx);
}

/** 重置滚动加载 */
static void ScrollLoading_Reset(ScrollLoading ctx)
{
	ctx->top = 0;
	ctx->top_child = NULL;
}

/** 启用/禁用滚动加载 */
static void ScrollLoading_Enable(ScrollLoading ctx, LCUI_BOOL enable)
{
	ctx->enabled = enable;
}

/** 获取文件夹缩略图文件路径 */
static int GetDirThumbFilePath(char *filepath, char *dirpath)
{
	int total;
	DB_File file;
	DB_Query query;
	DB_QueryTermsRec terms = { 0 };
	terms.dirpath = dirpath;
	terms.n_dirs = 1;
	terms.limit = 10;
	terms.for_tree = TRUE;
	terms.create_time = DESC;
	query = DB_NewQuery(&terms);
	total = DBQuery_GetTotalFiles(query);
	if (total > 0) {
		file = DBQuery_FetchFile(query);
		strcpy(filepath, file->path);
	}
	return total;
}

static void ThumbViewItem_SetThumb(LCUI_Widget item, LCUI_Graph *thumb)
{
	Widget_SetStyle(item, key_background_image, thumb, image);
	Widget_UpdateStyle(item, FALSE);
}

static void ThumbViewItem_UnsetThumb(LCUI_Widget item)
{
	DEBUG_MSG("remove thumb\n");
	Graph_Init(&item->computed_style.background.image);
	item->custom_style->sheet[key_background_image].is_valid = FALSE;
	Widget_UpdateStyle(item, FALSE);
}

void ThumbViewItem_BindFile(LCUI_Widget item, DB_File file)
{
	ThumbViewItem data = Widget_GetData(item, self.item);
	data->is_dir = FALSE;
	data->file = file;
	data->path = data->file->path;
}

DB_File ThumbViewItem_GetFile(LCUI_Widget item)
{
	ThumbViewItem data = Widget_GetData(item, self.item);
	if (data->is_dir) {
		return NULL;
	}
	return data->file;
}

void ThumbViewItem_AppendToCover(LCUI_Widget item, LCUI_Widget child)
{
	ThumbViewItem data = Widget_GetData(item, self.item);
	if (!data->is_dir) {
		Widget_Append(data->cover, child);
	}
}

void ThumbViewItem_SetFunction(LCUI_Widget item,
			       void (*setthumb)(LCUI_Widget, LCUI_Graph *),
			       void (*unsetthumb)(LCUI_Widget),
			       void (*updatesize)(LCUI_Widget))
{
	ThumbViewItem data = Widget_GetData(item, self.item);
	data->setthumb = setthumb;
	data->unsetthumb = unsetthumb;
	data->updatesize = updatesize;
}

/** 当移除缩略图的时候 */
static void OnRemoveThumb(void *data)
{
	LCUI_Widget w = data;
	ThumbViewItem item = Widget_GetData(w, self.item);
	if (item->unsetthumb) {
		item->unsetthumb(w);
	}
}

static void ThumbLoader_Destroy(ThumbLoader loader)
{
	loader->active = FALSE;
	if (loader->wfullpath) {
		free(loader->wfullpath);
	}
	LCUIMutex_Destroy(&loader->mutex);
	free(loader);
}

static void ThumbLoader_Callback(ThumbLoader loader)
{
	if (loader->callback) {
		loader->callback(loader);
	}
}

static void ThumbLoader_OnError(ThumbLoader loader)
{
	LCUI_Widget icon;
	ThumbViewItem item;

	LCUIMutex_Lock(&loader->mutex);
	if (!loader->active) {
		goto exit;
	}
	item = Widget_GetData(loader->target, self.item);
	item->loader = NULL;
	if (!item->is_valid || item->is_dir) {
		goto exit;
	}
	item->is_valid = FALSE;
	icon = LCUIWidget_New("textview");
	Widget_AddClass(icon, "tip icon icon-help");
	Widget_AddClass(icon, "floating center middle aligned");
	Widget_Append(item->cover, icon);
exit:
	LCUIMutex_Unlock(&loader->mutex);
	ThumbLoader_Callback(loader);
}

static void ThumbLoader_OnDone(ThumbLoader loader, ThumbData data,
			       FileStatus *status)
{
	LCUI_Graph *thumb;
	ThumbViewItem item;
	LCUIMutex_Lock(&loader->mutex);
	if (!loader->active || !loader->target) {
		LCUIMutex_Unlock(&loader->mutex);
		ThumbLoader_Callback(loader);
		return;
	}
	item = Widget_GetData(loader->target, self.item);
	if (!item->is_dir) {
		if (item->file->modify_time != (uint_t)status->mtime) {
			int ctime = (int)status->ctime;
			int mtime = (int)status->mtime;
			DBFile_SetTime(item->file, ctime, mtime);
		}
		if (data->origin_width > 0 && data->origin_height > 0 &&
		    (item->file->width != data->origin_width ||
		     item->file->height != data->origin_height)) {
			DBFile_SetSize(item->file, data->origin_width,
				       data->origin_height);
		}
	}
	item->loader = NULL;
	ThumbCache_Add(loader->view->cache, item->path, &data->graph);
	thumb = ThumbCache_Link(loader->view->cache, item->path,
				loader->view->linker, loader->target);
	if (thumb) {
		if (item->setthumb) {
			LCUI_PostSimpleTask(item->setthumb, loader->target,
					    thumb);
		}
		LCUIMutex_Unlock(&loader->mutex);
		ThumbLoader_Callback(loader);
		return;
	}
	LCUIMutex_Unlock(&loader->mutex);
	ThumbLoader_OnError(loader);
}

static void OnGetThumbnail(FileStatus *status, LCUI_Graph *thumb, void *data)
{
	ThumbDataRec tdata;
	ThumbLoader loader = data;
	LCUIMutex_Lock(&loader->mutex);
	if (!loader->active || !loader->target) {
		LCUIMutex_Unlock(&loader->mutex);
		ThumbLoader_Callback(loader);
		return;
	}
	LCUIMutex_Unlock(&loader->mutex);
	if (!status || !status->image || !thumb) {
		ThumbLoader_OnError(loader);
		return;
	}
	tdata.origin_width = status->image->width;
	tdata.origin_height = status->image->height;
	tdata.modify_time = (uint_t)status->mtime;
	tdata.graph = *thumb;
	ThumbDB_Save(loader->db, loader->path, &tdata);
	ThumbLoader_OnDone(loader, &tdata, status);
	/** 重置数据，避免被释放 */
	Graph_Init(thumb);
}

static void ThumbLoader_Load(ThumbLoader loader, FileStatus *status)
{
	int ret;
	ThumbDataRec tdata;
	ThumbViewItem item;
	LCUIMutex_Lock(&loader->mutex);
	if (!loader->active || !loader->target) {
		LCUIMutex_Unlock(&loader->mutex);
		ThumbLoader_Callback(loader);
		return;
	}
	ret = ThumbDB_Load(loader->db, loader->path, &tdata);
	item = Widget_GetData(loader->target, self.item);
	LCUIMutex_Unlock(&loader->mutex);
	if (ret == 0) {
		if (status && tdata.modify_time == status->mtime) {
			ThumbLoader_OnDone(loader, &tdata, status);
			return;
		}
	}
	if (item->is_dir) {
		FileStorage_GetThumbnail(loader->view->storage,
					 loader->wfullpath, FOLDER_MAX_WIDTH, 0,
					 OnGetThumbnail, loader);
		return;
	}
	FileStorage_GetThumbnail(loader->view->storage, loader->wfullpath, 0,
				 THUMB_MAX_WIDTH, OnGetThumbnail, loader);
}

static void OnGetFileStatus(FileStatus *status, void *data)
{
	if (!status) {
		ThumbLoader_OnError(data);
		return;
	}
	ThumbLoader_Load(data, status);
}

static ThumbLoader ThumbLoader_Create(ThumbView view, LCUI_Widget target)
{
	ThumbViewItem item;
	ThumbLoader loader;

	item = Widget_GetData(target, self.item);
	if (!item || item->loader) {
		return NULL;
	}
	loader = NEW(ThumbLoaderRec, 1);
	loader->view = view;
	loader->data = NULL;
	loader->active = TRUE;
	loader->target = target;
	loader->callback = NULL;
	LCUICond_Init(&loader->cond);
	LCUIMutex_Init(&loader->mutex);
	item->loader = loader;
	return loader;
}

static void ThumbLoader_SetCallback(ThumbLoader loader,
				    ThumbLoaderCallback callback, void *data)
{
	loader->callback = callback;
	loader->data = data;
}

/** 载入缩略图 */
static void ThumbLoader_Start(ThumbLoader loader)
{
	size_t len;
	DB_Dir dir;
	ThumbViewItem item;
	ThumbView view = loader->view;
	item = Widget_GetData(loader->target, self.item);
	dir = LCFinder_GetSourceDir(item->path);
	if (!dir) {
		ThumbLoader_OnError(loader);
		return;
	}
	loader->db = Dict_FetchValue(*view->dbs, dir->path);
	if (!loader->db) {
		ThumbLoader_OnError(loader);
		return;
	}
	len = strlen(dir->path);
	if (item->is_dir) {
		pathjoin(loader->fullpath, item->path, "");
		if (GetDirThumbFilePath(loader->fullpath, loader->fullpath) ==
		    0) {
			ThumbLoader_OnError(loader);
			return;
		}
		if (item->path[len] == PATH_SEP) {
			len += 1;
		}
		pathjoin(loader->path, item->path + len, DIR_COVER_THUMB);
	} else {
		pathjoin(loader->fullpath, item->path, "");
		if (item->path[len] == PATH_SEP) {
			len += 1;
		}
		pathjoin(loader->path, item->path + len, "");
	}
	loader->wfullpath = DecodeUTF8(loader->fullpath);
	FileStorage_GetStatus(loader->view->storage, loader->wfullpath, FALSE,
			      OnGetFileStatus, loader);
}

static void ThumbLoader_Stop(ThumbLoader loader)
{
	LCUIMutex_Lock(&loader->mutex);
	loader->active = FALSE;
	loader->target = NULL;
	LCUIMutex_Unlock(&loader->mutex);
}

static void ThumbView_OnThumbLoadDone(ThumbLoader loader)
{
	ThumbView view = loader->view;
	ThumbViewTask task = &view->tasks[TASK_LOAD_THUMB];
	LCUIMutex_Lock(&view->tasks_mutex);
	if (loader->view->thumb_tasks.length > 0) {
		task->state = TASK_STATE_READY;
	} else {
		task->state = TASK_STATE_FINISHED;
	}
	ThumbLoader_Destroy(loader);
	LCUICond_Signal(&view->tasks_cond);
	LCUIMutex_Unlock(&view->tasks_mutex);
	task->loader = NULL;
}

/** 执行加载缩略图的任务 */
static void ThumbView_ExecLoadThumb(LCUI_Widget w, LCUI_Widget target)
{
	LCUI_Graph *thumb;
	ThumbLoader loader;
	ThumbView view = Widget_GetData(w, self.main);
	ThumbViewItem item = Widget_GetData(target, self.item);
	thumb = ThumbCache_Link(view->cache, item->path, view->linker, target);
	if (thumb) {
		if (item->setthumb) {
			LCUI_PostSimpleTask(item->setthumb, target, thumb);
		}
		return;
	}
	loader = ThumbLoader_Create(view, target);
	if (!loader) {
		return;
	}
	ThumbLoader_SetCallback(loader, ThumbView_OnThumbLoadDone, NULL);
	view->tasks[TASK_LOAD_THUMB].state = TASK_STATE_RUNNING;
	view->tasks[TASK_LOAD_THUMB].loader = loader;
	ThumbLoader_Start(loader);
}

void ThumbView_Lock(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	ScrollLoading_Enable(view->scrollload, FALSE);
	LCUIMutex_Lock(&view->mutex);
}

void ThumbView_Unlock(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	ScrollLoading_Enable(view->scrollload, TRUE);
	LCUIMutex_Unlock(&view->mutex);
}

static float ComputeLayoutMaxWidth(LCUI_Widget parent)
{
	float width = max(THUMBVIEW_MIN_WIDTH, parent->box.content.width);
	return width + FILE_ITEM_MARGIN * 2;
}

/** 更新布局上下文，为接下来的子部件布局处理做准备 */
static void ThumbView_UpdateLayoutContext(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	float max_width = ComputeLayoutMaxWidth(w->parent);

	view->layout.folders_per_row = (int)ceil(max_width / FOLDER_MAX_WIDTH);
	view->layout.max_width = max_width;
	Widget_SetStyle(w, key_width, max_width, px);
}

void ThumbView_Empty(LCUI_Widget w)
{
	ThumbView view;
	LinkedListNode *node;
	LCUI_Widget child;

	view = Widget_GetData(w, self.main);
	view->is_loading = FALSE;
	LCUIMutex_Lock(&view->tasks_mutex);
	LCUIMutex_Lock(&view->layout.row_mutex);
	ScrollLoading_Enable(view->scrollload, FALSE);
	view->layout.x = 0;
	view->layout.count = 0;
	view->layout.start = NULL;
	view->layout.current = NULL;
	view->layout.folder_count = 0;
	LinkedList_Clear(&view->thumb_tasks, NULL);
	LinkedList_Clear(&view->layout.row, NULL);
	for (LinkedList_Each(node, &w->children)) {
		child = node->data;
		if (Widget_CheckPrototype(child, self.item)) {
			self.item->destroy(child);
		}
	}
	Widget_Empty(w);
	LCUICond_Signal(&view->tasks_cond);
	LCUIMutex_Unlock(&view->layout.row_mutex);
	LCUIMutex_Unlock(&view->tasks_mutex);
	ScrollLoading_Enable(view->scrollload, TRUE);
	ScrollLoading_Reset(view->scrollload);
}

/** 更新当前行内的各个缩略图尺寸 */
static void UpdateThumbRow(ThumbView view)
{
	LCUI_Widget item;
	ThumbViewItem data;
	LinkedListNode *node;
	float overflow_width, width, thumb_width, rest_width;
	if (view->layout.max_width < THUMBVIEW_MIN_WIDTH) {
		return;
	}
	/* 计算溢出的宽度 */
	overflow_width = view->layout.x - view->layout.max_width;
	/**
	 * 如果这一行缩略图的总宽度有溢出（超出最大宽度），则根据缩略图宽度占总宽度
	 * 的比例，分别缩减相应的宽度。
	 */
	if (overflow_width <= 0) {
		LinkedList_Clear(&view->layout.row, NULL);
		view->layout.x = 0;
		return;
	}
	rest_width = overflow_width;
	LCUIMutex_Lock(&view->layout.row_mutex);
	for (LinkedList_Each(node, &view->layout.row)) {
		int w, h;
		item = node->data;
		data = Widget_GetData(item, self.item);
		/* 确定合适的尺寸 */
		w = data->file->width > 0 ? data->file->width : 226;
		h = data->file->height > 0 ? data->file->height : 226;
		/* 按高的比例，计算合适的缩略图宽度 */
		thumb_width = 1.0f * PICTURE_FIXED_HEIGHT * w / h;
		/* 加上外间距占用的宽度 */
		thumb_width += FILE_ITEM_MARGIN * 2;
		/* 按照溢出后的宽度，计算当前图片需要分摊的溢出宽度 */
		width = overflow_width * thumb_width / view->layout.x;
		thumb_width -= FILE_ITEM_MARGIN * 2;
		/**
		 * 以上按比例分配的扣除宽度有误差，通常会少扣除几个像素的
		 * 宽度，这里用 rest_width 变量记录剩余待扣除的宽度，最
		 * 后一个缩略图的宽度直接减去 rest_width，以补全少扣除的
		 * 宽度。
		 */
		if (node->next) {
			rest_width -= width;
			width = thumb_width - width;
		} else {
			width = thumb_width - rest_width;
		}
		SetStyle(item->custom_style, key_width, width, px);
		Widget_UpdateStyle(item, FALSE);
	}
	LinkedList_Clear(&view->layout.row, NULL);
	LCUIMutex_Unlock(&view->layout.row_mutex);
	view->layout.x = 0;
}

/* 根据当前布局更新图片尺寸 */
static void UpdatePictureSize(LCUI_Widget item)
{
	float width, w = 226, h = 226;
	ThumbViewItem data = Widget_GetData(item, self.item);
	ThumbView view = data->view;

	if (data->file->width > 0) {
		w = (float)data->file->width;
		h = (float)data->file->height;
	}
	width = PICTURE_FIXED_HEIGHT * w / h;
	SetStyle(item->custom_style, key_width, width, px);
	Widget_UpdateStyle(item, FALSE);
	view->layout.x += width + FILE_ITEM_MARGIN * 2;
	LinkedList_Append(&view->layout.row, item);
	/* 如果当前行图片总宽度超出最大宽度，则更新各个图片的宽度 */
	if (view->layout.x >= view->layout.max_width) {
		UpdateThumbRow(view);
	}
}

/* 根据当前布局更新文件夹尺寸 */
static void UpdateFolderSize(LCUI_Widget item)
{
	float width;
	ThumbViewItem data = Widget_GetData(item, self.item);
	ThumbView view = data->view;

	++view->layout.folder_count;
	if (view->layout.max_width < THUMBVIEW_MIN_WIDTH) {
		item->custom_style->sheet[key_width].is_valid = FALSE;
		Widget_AddClass(item, "full-width");
		return;
	} else {
		Widget_RemoveClass(item, "full-width");
	}

	width = view->layout.max_width / view->layout.folders_per_row;
	width -= FILE_ITEM_MARGIN * 2;
	Widget_SetStyle(item, key_width, width, px);
	Widget_UpdateStyle(item, FALSE);
}

/** 更新用于布局的游标，以确保能够从正确的位置开始排列部件 */
static void ThumbView_UpdateLayoutCursor(LCUI_Widget w)
{
	LCUI_Widget child, prev;
	ThumbView view = Widget_GetData(w, self.main);
	if (view->layout.current) {
		return;
	}
	if (!view->layout.start) {
		if (w->children.length > 0) {
			view->layout.current = w->children.head.next->data;
		}
		return;
	}
	child = view->layout.start;
	prev = Widget_GetPrev(child);
	while (prev) {
		if (prev->computed_style.position == SV_ABSOLUTE ||
		    prev->computed_style.display == SV_NONE) {
			prev = Widget_GetPrev(prev);
			continue;
		}
		if ((child->x == 0 && child->y > prev->y) ||
		    prev->computed_style.display == SV_BLOCK) {
			break;
		}
		child = prev;
		prev = Widget_GetPrev(prev);
	}
	view->layout.start = child;
	view->layout.current = child;
}

/** 直接执行布局更新操作 */
static int ThumbView_OnUpdateLayout(LCUI_Widget w, int limit)
{
	int count = 0;
	LCUI_Widget child;
	ThumbView view = Widget_GetData(w, self.main);
	ThumbView_UpdateLayoutCursor(w);
	child = view->layout.current;
	for (; child && --limit >= 0; child = Widget_GetNext(child)) {
		ThumbViewItem item;
		view->layout.count += 1;
		item = Widget_GetData(child, self.item);
		if (!child->computed_style.visible) {
			++limit;
			continue;
		}
		if (Widget_CheckType(child, "thumbviewitem")) {
			++count;
			view->layout.current = child;
			item->updatesize(child);
			continue;
		}
		UpdateThumbRow(view);
		++limit;
	}
	if (view->layout.current) {
		view->layout.current = Widget_GetNext(view->layout.current);
	}
	return count;
}

/** 在布局完成后 */
static void ThumbView_OnAfterLayout(LCUI_Widget w, LCUI_WidgetEvent e,
				    void *arg)
{
	ThumbView view = Widget_GetData(w, self.main);
	ScrollLoading_Update(view->scrollload);
	Widget_UnbindEvent(w, "afterlayout", ThumbView_OnAfterLayout);
}

/** 执行缩略图列表的布局任务 */
static void ThumbView_ExecUpdateLayout(LCUI_Widget w)
{
	int n;
	ThumbView view = Widget_GetData(w, self.main);
	if (!view->layout.is_running) {
		return;
	}
	n = ThumbView_OnUpdateLayout(w, 100);
	if (n < 100) {
		UpdateThumbRow(view);
		view->layout.current = NULL;
		view->layout.is_running = FALSE;
		Widget_BindEvent(w, "afterlayout", ThumbView_OnAfterLayout,
				 NULL, NULL);
		return;
	}
	/* 如果还有未布局的缩略图则下次再继续 */
	view->tasks[TASK_LAYOUT].state = TASK_STATE_READY;
	LCUICond_Signal(&view->tasks_cond);
}

/** 延迟执行缩略图列表的布局操作 */
static void OnDelayLayout(void *arg)
{
	LCUI_Widget w = arg;
	ThumbView view = Widget_GetData(w, self.main);
	LCUIMutex_Lock(&view->layout.row_mutex);
	view->layout.x = 0;
	view->layout.count = 0;
	view->layout.current = NULL;
	view->layout.folder_count = 0;
	view->layout.is_delaying = FALSE;
	view->layout.is_running = TRUE;
	ThumbView_UpdateLayoutContext(w);
	LinkedList_Clear(&view->layout.row, NULL);
	LCUIMutex_Unlock(&view->layout.row_mutex);
	if (view->onlayout) {
		view->onlayout(w);
	}
	view->tasks[TASK_LAYOUT].state = TASK_STATE_READY;
	LCUICond_Signal(&view->tasks_cond);
}

static void ThumbView_OnAnimationFinished(Animation ani)
{
	LCUI_Widget w = ani->data;
	Widget_SetOpacity(w, 1.0f);
	}

static void ThumbView_OnAnimationFrame(Animation ani)
{
	double opacity;
	LCUI_Widget w = ani->data;

	if (ani->_progress <= 0.3) {
		opacity = 1.0 - ani->_progress / 0.3;
	} else if (ani->_progress <= 0.7) {
		opacity = 0;
	} else {
		opacity = (ani->_progress - 0.7) / 0.3;
}
	Widget_SetOpacity(w, (float)opacity);
}

void ThumbView_UpdateLayout(LCUI_Widget w, LCUI_Widget start_child)
{
	ThumbView view = Widget_GetData(w, self.main);
	if (view->layout.is_running) {
		return;
	}
	Animation_Play(&view->animation);
	view->layout.start = start_child;
	/* 如果已经有延迟布局任务，则重置该任务的定时 */
	if (view->layout.is_delaying) {
		LCUITimer_Reset(view->layout.timer, LAYOUT_DELAY);
		return;
	}
	view->layout.is_delaying = TRUE;
	view->layout.timer = LCUI_SetTimeout(LAYOUT_DELAY, OnDelayLayout, w);
}

/** 在缩略图列表视图的尺寸有变更时... */
static void OnResize(LCUI_Widget parent, LCUI_WidgetEvent e, void *arg)
{
	ThumbView view = Widget_GetData(e->data, self.main);
	/* 如果父级部件的内容区宽度没有变化则不更新布局 */
	if (ComputeLayoutMaxWidth(parent) == view->layout.max_width) {
		return;
	}
	ThumbView_UpdateLayout(e->data, NULL);
}

/** 移除与部件绑定的缩略图加载任务 */
static int RemoveThumbTask(LCUI_Widget w)
{
	LinkedListNode *node;
	ThumbViewItem data = Widget_GetData(w, self.item);
	LinkedList *tasks = &data->view->thumb_tasks;
	if (tasks->length < 1) {
		return -1;
	}
	LCUIMutex_Lock(&data->view->tasks_mutex);
	for (LinkedList_Each(node, tasks)) {
		if (node->data != w) {
			continue;
		}
		LinkedList_DeleteNode(tasks, node);
		LCUIMutex_Unlock(&data->view->tasks_mutex);
		return 0;
	}
	LCUIMutex_Unlock(&data->view->tasks_mutex);
	return -1;
}

static void OnScrollLoad(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	ThumbViewItem data = Widget_GetData(w, self.item);
	LinkedList *tasks = &data->view->thumb_tasks;
	if (w->custom_style->sheet[key_background_image].is_valid || !data ||
	    !data->view->cache || data->loader) {
		return;
	}
	LCUIMutex_Lock(&data->view->tasks_mutex);
	/* 如果待处理的任务数量超过最大限制，则移除最后一个任务 */
	if (tasks->length >= THUMB_TASK_MAX) {
		LinkedListNode *node = LinkedList_GetNodeAtTail(tasks, 0);
		LinkedList_Unlink(tasks, node);
	}
	LinkedList_Insert(&data->view->thumb_tasks, 0, w);
	data->view->tasks[TASK_LOAD_THUMB].state = TASK_STATE_READY;
	/* 通知任务线程处理该任务 */
	LCUICond_Signal(&data->view->tasks_cond);
	LCUIMutex_Unlock(&data->view->tasks_mutex);
}

void ThumbView_EnableScrollLoading(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	ScrollLoading_Reset(view->scrollload);
	ScrollLoading_Update(view->scrollload);
	ScrollLoading_Enable(view->scrollload, TRUE);
}

void ThumbView_DisableScrollLoading(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	ScrollLoading_Enable(view->scrollload, FALSE);
}

LCUI_Widget ThumbView_AppendFolder(LCUI_Widget w, const char *filepath,
				   LCUI_BOOL show_path)
{
	ThumbViewItem data;
	size_t len = strlen(filepath) + 1;
	LCUI_Widget item = LCUIWidget_New("thumbviewitem");
	LCUI_Widget name = LCUIWidget_New("textview");
	LCUI_Widget path = LCUIWidget_New("textview");
	LCUI_Widget icon = LCUIWidget_New("textview");
	LCUI_Widget infobar = LCUIWidget_New(NULL);
	data = Widget_GetData(item, self.item);
	data->path = malloc(sizeof(char) * len);
	strncpy(data->path, filepath, len);
	data->view = Widget_GetData(w, self.main);
	data->is_dir = TRUE;
	data->setthumb = ThumbViewItem_SetThumb;
	data->unsetthumb = ThumbViewItem_UnsetThumb;
	data->updatesize = UpdateFolderSize;
	Widget_AddClass(item, FOLDER_CLASS);
	if (!show_path) {
		Widget_AddClass(item, "hide-path");
	}
	Widget_AddClass(infobar, "info");
	Widget_AddClass(name, "name");
	Widget_AddClass(path, "path");
	Widget_AddClass(icon, "icon icon icon-folder-outline");
	TextView_SetText(name, getfilename(filepath));
	TextView_SetText(path, filepath);
	Widget_Append(item, infobar);
	Widget_Append(infobar, name);
	Widget_Append(infobar, path);
	Widget_Append(infobar, icon);
	Widget_Append(w, item);
	ScrollLoading_Update(data->view->scrollload);
	if (!data->view->layout.is_running) {
		data->view->layout.current = w;
		UpdateFolderSize(item);
	}
	return item;
}

LCUI_Widget ThumbView_AppendPicture(LCUI_Widget w, const DB_File file)
{
	LCUI_Widget item;
	ThumbViewItem data;
	item = LCUIWidget_New("thumbviewitem");
	data = Widget_GetData(item, self.item);
	data->is_dir = FALSE;
	data->view = Widget_GetData(w, self.main);
	data->file = DBFile_Dup(file);
	data->path = data->file->path;
	data->cover = LCUIWidget_New(NULL);
	data->setthumb = ThumbViewItem_SetThumb;
	data->unsetthumb = ThumbViewItem_UnsetThumb;
	data->updatesize = UpdatePictureSize;
	Widget_AddClass(item, PICTURE_CLASS);
	Widget_AddClass(data->cover, "picture-cover");
	Widget_Append(item, data->cover);
	Widget_Append(w, item);
	ScrollLoading_Update(data->view->scrollload);
	if (!data->view->layout.is_running) {
		data->view->layout.current = w;
		UpdatePictureSize(item);
	}
	return item;
}

void ThumbView_Append(LCUI_Widget w, LCUI_Widget child)
{
	ThumbView view = Widget_GetData(w, self.main);
	ScrollLoading_Update(view->scrollload);
	Widget_Append(w, child);
	if (Widget_CheckType(child, "thumbviewitem")) {
		ThumbViewItem item;
		item = Widget_GetData(child, self.item);
		item->view = view;
		if (item->updatesize && !view->layout.is_running) {
			item->updatesize(child);
		}
	} else if (!view->layout.is_running) {
		UpdateThumbRow(view);
	}
}

void ThumbView_SetCache(LCUI_Widget w, ThumbCache cache)
{
	ThumbView view = Widget_GetData(w, self.main);
	if (view->cache) {
		ThumbCache_DeleteLinker(view->cache, view->linker);
	}
	view->linker = ThumbCache_AddLinker(cache, OnRemoveThumb);
	view->cache = cache;
}

void ThumbView_SetStorage(LCUI_Widget w, int storage)
{
	ThumbView view = Widget_GetData(w, self.main);
	view->storage = storage;
}

static void ThumbView_ExecTask(LCUI_Widget w, int task)
{
	LCUI_Widget target;
	LinkedListNode *node;
	ThumbView view = Widget_GetData(w, self.main);
	switch (task) {
	case TASK_LOAD_THUMB:
		LCUIMutex_Lock(&view->tasks_mutex);
		node = LinkedList_GetNode(&view->thumb_tasks, 0);
		if (!node) {
			LCUIMutex_Unlock(&view->tasks_mutex);
			break;
		}
		target = node->data;
		LinkedList_Unlink(&view->thumb_tasks, node);
		LCUIMutex_Unlock(&view->tasks_mutex);
		ThumbView_ExecLoadThumb(w, target);
		LinkedListNode_Delete(node);
		break;
	case TASK_LAYOUT:
		ThumbView_ExecUpdateLayout(w);
	default:
		break;
	}
}

/** 任务处理线程 */
static void ThumbView_TaskThread(void *arg)
{
	int i, count;

	ThumbView view;
	ThumbViewTask task;

	LCUI_Widget parent, w = arg;
	LCUI_BOOL visible, current_visible = FALSE;

	view = Widget_GetData(w, self.main);
	while (view->is_running) {
		view->is_loading = TRUE;
		visible = current_visible;
		current_visible = Widget_IsVisible(w);
		LCUIMutex_Lock(&view->mutex);
		for (i = 0, count = 0; i < TASK_TOTAL; ++i) {
			task = &view->tasks[i];
			switch (task->state) {
			case TASK_STATE_READY:
				task->state = TASK_STATE_FINISHED;
				ThumbView_ExecTask(w, i);
				if (task->state == TASK_STATE_FINISHED) {
					++count;
				}
				break;
			case TASK_STATE_RUNNING:
				break;
			case TASK_STATE_FINISHED:
			default:
				++count;
				break;
			}
		}
		LCUIMutex_Unlock(&view->mutex);
		/* 如果所有任务都处理完成 */
		if (count == TASK_TOTAL) {
			view->is_loading = FALSE;
			LCUIMutex_Lock(&view->tasks_mutex);
			LCUICond_TimedWait(&view->tasks_cond,
					   &view->tasks_mutex, 500);
			LCUIMutex_Unlock(&view->tasks_mutex);
		}
		/* 检查自己及父级部件是否可见 */
		for (parent = w; parent; parent = parent->parent) {
			if (!Widget_IsVisible(parent)) {
				current_visible = FALSE;
				break;
			}
		}
		/* 如果当前可见但之前不可见，则刷新当前可见区域内加载的缩略图 */
		if (current_visible && !visible) {
			ScrollLoading_Update(view->scrollload);
		}
	}
}

static void ThumbViewItem_OnInit(LCUI_Widget w)
{
	ThumbViewItem item;
	const size_t data_size = sizeof(ThumbViewItemRec);
	item = Widget_AddData(w, self.item, data_size);
	item->file = NULL;
	item->is_dir = FALSE;
	item->is_valid = TRUE;
	item->path = NULL;
	item->view = NULL;
	item->cover = NULL;
	item->setthumb = NULL;
	item->unsetthumb = NULL;
	item->updatesize = NULL;
	item->loader = NULL;
	Widget_BindEvent(w, "scrollload", OnScrollLoad, NULL, NULL);
}

static void ThumbView_OnRemove(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	ThumbViewItem item;
	e->cancel_bubble = TRUE;
	if (!Widget_CheckPrototype(e->target, self.item)) {
		return;
	}
	item = Widget_GetData(e->target, self.item);
	if (item->loader) {
		ThumbLoader_Stop(item->loader);
		item->loader = NULL;
	}
}

static void ThumbViewItem_OnDestroy(LCUI_Widget w)
{
	ThumbViewItem item;
	item = Widget_GetData(w, self.item);
	if (item->view) {
	RemoveThumbTask(w);
	}
	if (item->loader) {
		ThumbLoader_Stop(item->loader);
		item->loader = NULL;
	}
	if (item->unsetthumb) {
	item->unsetthumb(w);
	}
	if (item->path) {
		ThumbCache_Unlink(item->view->cache, item->view->linker,
				  item->path);
	}
	if (item->is_dir) {
		if (item->path) {
			free(item->path);
		}
	} else if(item->file) {
		DBFile_Release(item->file);
	}
	item->file = NULL;
	item->view = NULL;
	item->path = NULL;
	item->unsetthumb = NULL;
	item->setthumb = NULL;
}

void ThumbView_OnLayout(LCUI_Widget w, void (*func)(LCUI_Widget))
{
	ThumbView view = Widget_GetData(w, self.main);
	view->onlayout = func;
}

static void ThumbView_OnReady(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	ThumbView_UpdateLayoutContext(w);
	Widget_BindEvent(w->parent, "resize", OnResize, w, NULL);
}

static void ThumbView_InitAnimation(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	Animation ani = &view->animation;

	memset(ani, 0, sizeof(AnimationRec));

	ani->data = w;
	ani->delay = ANIMATION_DELAY;
	ani->duration = ANIMATION_DURATION / 2;
	ani->on_frame = ThumbView_OnAnimationFrame;
	ani->on_finished = ThumbView_OnAnimationFinished;
}

static void ThumbView_InitLayout(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);

	view->onlayout = NULL;
	view->layout.x = 0;
	view->layout.count = 0;
	view->layout.max_width = 0;
	view->layout.start = NULL;
	view->layout.current = NULL;
	view->layout.folder_count = 0;
	view->layout.is_running = FALSE;
	view->layout.is_delaying = FALSE;
	view->layout.folders_per_row = 1;
	LinkedList_Init(&view->layout.row);
	LCUIMutex_Init(&view->layout.row_mutex);
}

static void ThumbView_InitTasks(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);

	LCUICond_Init(&view->tasks_cond);
	LCUIMutex_Init(&view->tasks_mutex);
	LinkedList_Init(&view->thumb_tasks);
	memset(view->tasks, 0, sizeof(view->tasks));
	LCUIThread_Create(&view->thread, ThumbView_TaskThread, w);
}

static void ThumbView_OnInit(LCUI_Widget w)
{
	const size_t data_size = sizeof(ThumbViewRec);
	ThumbView view = Widget_AddData(w, self.main, data_size);
	view->dbs = &finder.thumb_dbs;
	view->is_loading = FALSE;
	view->is_running = TRUE;
	view->cache = NULL;
	view->linker = NULL;
	LCUIMutex_Init(&view->mutex);
	view->scrollload = ScrollLoading_New(w);
	Widget_BindEvent(w, "ready", ThumbView_OnReady, NULL, NULL);
	Widget_BindEvent(w, "remove", ThumbView_OnRemove, NULL, NULL);
	ThumbView_InitTasks(w);
	ThumbView_InitLayout(w);
	ThumbView_InitAnimation(w);
}

static void ThumbView_OnDestroy(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	ThumbView_Lock(w);
	ThumbView_Empty(w);
	ScrollLoading_Delete(view->scrollload);
	ThumbView_Unlock(w);
	view->is_running = FALSE;
	LCUIMutex_Lock(&view->tasks_mutex);
	LCUICond_Signal(&view->tasks_cond);
	LCUIMutex_Unlock(&view->tasks_mutex);
	LCUIThread_Join(view->thread, NULL);
}

void LCUIWidget_AddThumbView(void)
{
	self.main = LCUIWidget_NewPrototype("thumbview", NULL);
	self.item = LCUIWidget_NewPrototype("thumbviewitem", NULL);
	self.main->init = ThumbView_OnInit;
	self.main->destroy = ThumbView_OnDestroy;
	self.item->init = ThumbViewItem_OnInit;
	self.item->destroy = ThumbViewItem_OnDestroy;
	self.event_scrollload = LCUIWidget_AllocEventId();
	LCUIWidget_SetEventName(self.event_scrollload, "scrollload");
}
