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

#define LCFINDER_THUMBVIEW_C
#include <assert.h>
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

#define REFRESH_INTERVAL	500
#define THUMB_TASK_MAX		32
#define SCROLLLOADING_DELAY	500
#define LAYOUT_DELAY		1000
#define ANIMATION_DELAY		750
#define ANIMATION_DURATION	1000
#define MAX_LAYOUT_TARGETS	25600
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
typedef struct AutoLoaderRec_ {
	float top;			/**< 当前可见区域上边界的 Y 轴坐标 */
	int event_id;			/**< 滚动加载功能的事件ID */
	int timer;			/**< 定时器，用于实现延迟加载 */
	LCUI_BOOL is_delaying;		/**< 是否处于延迟状态 */
	LCUI_BOOL need_update;		/**< 是否需要更新 */
	LCUI_BOOL enabled;		/**< 是否启用滚动加载功能 */
	LCUI_Widget scrolllayer;	/**< 滚动层 */
	LCUI_Widget top_child;		/**< 当前可见区域第一个子部件 */
} AutoLoaderRec, *AutoLoader;

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

typedef struct ThumbWorkerRec_ {
	LCUI_BOOL active;
	ThumbLoader loader;
	LinkedList tasks;			/**< 缩略图加载任务队列 */
	int timer;
} ThumbWorkerRec, *ThumbWorker;

typedef struct ThumbViewRec_ {
	int timer;
	int storage;				/**< 文件存储服务的连接标识符 */
	Dict **dbs;				/**< 缩略图数据库字典，以目录路径进行索引 */
	ThumbCache cache;			/**< 缩略图缓存 */
	ThumbLinker linker;			/**< 缩略图链接器 */
	ThumbWorkerRec worker;
	AutoLoader loader;			/**< 缩略图自动加载器 */
	LCUI_Mutex mutex;			/**< 互斥锁 */
	LCUI_Thread thread;			/**< 任务处理线程 */
	LCUI_BOOL is_loading;			/**< 是否处于载入中状态 */
	LCUI_BOOL is_running;			/**< 是否处于运行状态 */
	unsigned hashes[32];
	size_t hashes_len;
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

/* FIXME: 代码太多改起来麻烦，先简单的把代码拆到其它文件里，等以后再考虑重构 */
#include "thumbview_autoloader.c"
#include "thumbview_loader.c"
#include "thumbview_worker.c"
#include "thumbview_layout.c"

static void ThumbViewItem_SetThumb(LCUI_Widget item, LCUI_Graph *thumb)
{
	DEBUG_MSG("item %d, set thumb, size: (%u, %u)\n", item->index,
		  thumb->width, thumb->height);
	Widget_SetStyle(item, key_background_image, thumb, image);
	Widget_UpdateStyle(item, FALSE);
}

static void ThumbViewItem_UnsetThumb(LCUI_Widget item)
{
	DEBUG_MSG("item %d, unset thumb\n", item->index);
	Graph_Init(&item->computed_style.background.image);
	Widget_UnsetStyle(item, key_background_image);
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

void ThumbView_Lock(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	AutoLoader_Enable(view->loader, FALSE);
	LCUIMutex_Lock(&view->mutex);
}

void ThumbView_Unlock(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	if (view->loader) {
		AutoLoader_Enable(view->loader, TRUE);
	}
	LCUIMutex_Unlock(&view->mutex);
}

void ThumbView_Empty(LCUI_Widget w)
{
	ThumbView view;
	LinkedListNode *node;
	LCUI_Widget child;

	view = Widget_GetData(w, self.main);
	view->is_loading = FALSE;
	LCUIMutex_Lock(&view->layout.row_mutex);
	AutoLoader_Enable(view->loader, FALSE);
	view->layout.x = 0;
	view->layout.count = 0;
	view->layout.start = NULL;
	view->layout.current = NULL;
	view->layout.folder_count = 0;
	LinkedList_Clear(&view->layout.row, NULL);
	ThumbWorker_Reset(&view->worker);
	for (LinkedList_Each(node, &w->children)) {
		child = node->data;
		if (Widget_CheckPrototype(child, self.item)) {
			self.item->destroy(child);
		}
	}
	Widget_Empty(w);
	LCUIMutex_Unlock(&view->layout.row_mutex);
	AutoLoader_Enable(view->loader, TRUE);
	AutoLoader_Reset(view->loader);
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

/** 在缩略图列表视图的尺寸有变更时... */
static void OnResize(LCUI_Widget parent, LCUI_WidgetEvent e, void *arg)
{
	ThumbView view = Widget_GetData(e->data, self.main);
	/* 如果父级部件的内容区宽度没有变化则不更新布局 */
	if (ComputeLayoutMaxWidth(parent) == view->layout.max_width) {
		return;
	}
	ThumbView_DelayUpdateLayout(e->data, NULL);
}

static void OnScrollLoad(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	ThumbViewItem data = Widget_GetData(w, self.item);
	LCUI_Style s = Widget_GetStyle(w, key_background_image);
	DEBUG_MSG("item[%u] on load\n", w->index);
	DEBUG_MSG("view[%p] tasks count: %lu\n", data->view, tasks->length);
	if (s->is_valid || !data || !data->view->cache || data->loader) {
		DEBUG_MSG("item[%u] no need load\n", w->index);
		return;
	}
	ThumbWorker_AddTask(&data->view->worker, w);
}

void ThumbView_EnableAutoLoader(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	AutoLoader_Reset(view->loader);
	AutoLoader_Update(view->loader);
	AutoLoader_Enable(view->loader, TRUE);
}

void ThumbView_DisableAutoLoader(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	AutoLoader_Enable(view->loader, FALSE);
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
	Widget_AddClass(infobar, "info");
	Widget_AddClass(name, "name");
	Widget_AddClass(path, "path");
	Widget_AddClass(icon, "icon icon icon-folder-outline");
	if (!show_path) {
		Widget_AddClass(item, "hide-path");
	}
	TextView_SetText(name, getfilename(filepath));
	TextView_SetText(path, filepath);
	Widget_Append(item, infobar);
	Widget_Append(infobar, name);
	Widget_Append(infobar, path);
	Widget_Append(infobar, icon);
	Widget_Append(w, item);

	AutoLoader_Update(data->view->loader);
	if (!data->view->layout.is_running && w->state >= LCUI_WSTATE_READY) {
		data->view->layout.current = w;
		UpdateFolderSize(item);
	}
	return item;
}

LCUI_Widget ThumbView_AppendPicture(LCUI_Widget w, const DB_File file)
{
	LCUI_Widget item;
	ThumbViewItem data;
	ThumbView view;

	view = Widget_GetData(w, self.main);
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

	// 为这个部件生成 hash 值，提升样式计算效率
	if (view->hashes_len > 0) {
		Widget_SetHashList(item, view->hashes, view->hashes_len);
	} else {
		Widget_GenerateHash(item);
		view->hashes_len = Widget_GetHashList(item, view->hashes, 32);
	}
	Widget_Append(w, item);
	AutoLoader_Update(data->view->loader);
	if (!data->view->layout.is_running && w->state >= LCUI_WSTATE_READY) {
		data->view->layout.current = w;
		UpdatePictureSize(item);
	}
	return item;
}

void ThumbView_Append(LCUI_Widget w, LCUI_Widget child)
{
	ThumbView view = Widget_GetData(w, self.main);
	AutoLoader_Update(view->loader);
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
		ThumbLinker_Destroy(view->linker);
	}
	view->linker = ThumbCache_AddLinker(cache, OnRemoveThumb);
	view->cache = cache;
}

void ThumbView_SetStorage(LCUI_Widget w, int storage)
{
	ThumbView view = Widget_GetData(w, self.main);
	view->storage = storage;
}

static void ThumbView_AutoLoadThumb(void *arg)
{
	LCUI_Widget parent, w = arg;
	LCUI_BOOL visible, current_visible = FALSE;
	ThumbView view = Widget_GetData(w, self.main);

	visible = current_visible;
	current_visible = Widget_IsVisible(w);

	/* 检查自己及父级部件是否可见 */
	for (parent = w; parent; parent = parent->parent) {
		if (!Widget_IsVisible(parent)) {
			current_visible = FALSE;
			break;
		}
	}
	/* 如果当前可见但之前不可见，则刷新当前可见区域内加载的缩略图 */
	if (current_visible && !visible && view->loader) {
		AutoLoader_Update(view->loader);
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
	Widget_BindEvent(w, "loader", OnScrollLoad, NULL, NULL);
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
		ThumbWorker_RemoveTask(&item->view->worker, w);
	}
	if (item->loader) {
		ThumbLoader_Stop(item->loader);
		item->loader = NULL;
	}
	if (item->unsetthumb) {
		item->unsetthumb(w);
	}
	if (item->path) {
		ThumbLinker_Unlink(item->view->linker, item->path);
	}
	if (item->is_dir) {
		if (item->path) {
			free(item->path);
		}
	} else if (item->file) {
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

void ThumbView_StartUpdateLayout(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	LayoutContext ctx = &view->layout;

	LCUIMutex_Lock(&ctx->row_mutex);
	ctx->x = 0;
	ctx->count = 0;
	ctx->current = NULL;
	ctx->folder_count = 0;
	ctx->is_delaying = FALSE;
	ctx->is_running = TRUE;
	ThumbView_UpdateLayoutContext(w);
	LinkedList_Clear(&ctx->row, NULL);
	LCUIMutex_Unlock(&ctx->row_mutex);

	if (view->onlayout) {
		view->onlayout(w);
	}
	ctx->timer = LCUI_NextFrame(ThumbView_UpdateLayout, w);
}

void ThumbView_DelayUpdateLayout(LCUI_Widget w, LCUI_Widget start_child)
{
	ThumbView view = Widget_GetData(w, self.main);
	LayoutContext ctx = &view->layout;

	if (view->layout.is_running) {
		return;
	}
	Animation_Play(&view->animation);
	ctx->start = start_child;
	if (ctx->timer) {
		LCUITimer_Free(ctx->timer);
	}
	ctx->is_delaying = TRUE;
	ctx->timer = LCUI_SetTimeout(LAYOUT_DELAY,
				     (FuncPtr)ThumbView_StartUpdateLayout, w);
}

static void ThumbView_OnReady(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	ThumbView_StartUpdateLayout(w);
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

static void ThumbView_InitWorker(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);

	ThumbWorker_Init(&view->worker);
}

static void ThumbView_FreeWorker(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);

	ThumbWorker_Reset(&view->worker);
}

static void ThumbView_OnUpdateProgress(LCUI_Widget w, size_t current)
{
	LCUI_WidgetEventRec ev;

	LCUI_InitWidgetEvent(&ev, "progress");
	Widget_TriggerEvent(w, &ev, &current);
}

static void ThumbView_OnInit(LCUI_Widget w)
{
	ThumbView view;
	LCUI_WidgetRulesRec rules;

	rules.only_on_visible = TRUE;
	rules.first_update_visible_children = TRUE;
	rules.max_render_children_count = 64;
	rules.max_update_children_count = 0;
	rules.cache_children_style = TRUE;
	rules.ignore_status_change = TRUE;
	rules.on_update_progress = ThumbView_OnUpdateProgress;
	Widget_SetRules(w, &rules);

	view = Widget_AddData(w, self.main, sizeof(ThumbViewRec));
	view->dbs = &finder.thumb_dbs;
	view->is_loading = FALSE;
	view->is_running = TRUE;
	view->hashes_len = 0;
	view->cache = NULL;
	view->linker = NULL;
	memset(view->hashes, 0, sizeof(view->hashes));
	LCUIMutex_Init(&view->mutex);
	view->loader = AutoLoader_New(w);
	view->timer =
	    LCUI_SetInterval(REFRESH_INTERVAL, ThumbView_AutoLoadThumb, w);
	Widget_BindEvent(w, "ready", ThumbView_OnReady, NULL, NULL);
	Widget_BindEvent(w, "remove", ThumbView_OnRemove, NULL, NULL);
	ThumbView_InitWorker(w);
	ThumbView_InitLayout(w);
	ThumbView_InitAnimation(w);
}

static void ThumbView_OnDestroy(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);

	ThumbView_Lock(w);
	ThumbView_Empty(w);
	view->is_running = FALSE;
	ThumbView_Unlock(w);
	ThumbView_FreeWorker(w);
	AutoLoader_Delete(view->loader);
	LCUITimer_Free(view->timer);
	view->timer = 0;
	view->loader = NULL;
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
	LCUIWidget_SetEventName(self.event_scrollload, "loader");
}
