/* ***************************************************************************
 * picture_loader.c -- the picture laoder for picture view
 *
 * Copyright (C) 2018 by Liu Chao <lc-soft@live.cn>
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
 * picture_loader.c -- 图片视图的图片加载器
 *
 * 版权所有 (C) 2018 归属于 刘超 <lc-soft@live.cn>
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <LCUI_Build.h>
#include <LCUI/types.h>
#include <LCUI/util/dict.h>
#include <LCUI/util/linkedlist.h>
#include <LCUI/font/charset.h>
#include <LCUI/thread.h>
#include <LCUI/graph.h>

#include "build.h"
#include "types.h"
#include "common.h"
#include "file_storage.h"
#include "picture.h"

typedef struct PictureCacheRec_ {
	int key;
	LCUI_Graph data;
} PictureCacheRec, *PictureCache;

typedef struct PictureLoaderTaskRec_ {
	LCUI_BOOL active;
	wchar_t *file;			/**< 当前已加载的图片的路径 */
	LCUI_Mutex mutex;		/**< 互斥锁，用于异步加载 */
	LCUI_Cond cond;			/**< 条件变量，用于异步加载 */
	void *data;
	PictureCache cache;
	PictureLoaderConfigRec config;
} PictureLoaderTaskRec, *PictureLoaderTask;

typedef struct PictureLoaderRec_ {
	PictureLoaderConfigRec config;

	LCUI_BOOL is_running;			/**< 是否正在运行 */
	LCUI_Cond cond;				/**< 条件变量 */
	LCUI_Mutex mutex;			/**< 互斥锁 */
	LCUI_Thread thread;			/**< 图片加载器所在的线程 */
	LinkedList cache;
	LinkedList tasks;
} PictureLoaderRec, *PictureLoader;

PictureLoaderTask PictureLoaderTask_Create(PictureLoaderConfig config,
					   PictureCache cache,
					   const wchar_t *file,
					   void *data)
{
	size_t len;
	ASSIGN(task, PictureLoaderTask);

	task->data = data;
	task->active = TRUE;
	task->config = *config;
	task->cache = cache;
	LCUICond_Init(&task->cond);
	LCUIMutex_Init(&task->mutex);

	len = wcslen(file) + 1;
	task->file = malloc(sizeof(wchar_t) * len);
	wcsncpy(task->file, file, len);

	return task;
}

static void PictureLoaderTask_OnDone(LCUI_Graph *img, void *data)
{
	PictureLoaderTask task = data;

	LCUIMutex_Lock(&task->mutex);
	task->active = FALSE;
	LCUICond_Signal(&task->cond);
	LCUIMutex_Unlock(&task->mutex);
	if (img) {
		task->cache->data = *img;
		Graph_Init(img);
		task->config.on_load(task->data, &task->cache->data);
	} else {
		task->config.on_load(task->data, NULL);
	}
}

static void PictureLoaderTask_OnProgress(float progress, void *data)
{
	PictureLoaderTask task = data;
	task->config.on_progress(task->data, progress);
}

void PictureLoaderTask_Run(PictureLoaderTask task)
{
	assert(task->file != NULL);

	task->active = TRUE;
	task->config.on_start(task->data);
	_DEBUG_MSG("load: %ls\n", task->file);
	/* 异步请求加载图像内容 */
	FileStorage_GetImage(task->config.storage,
			     task->file,
			     PictureLoaderTask_OnDone,
			     PictureLoaderTask_OnProgress, task);
	LCUIMutex_Lock(&task->mutex);
	while (task->active) {
		LCUICond_TimedWait(&task->cond, &task->mutex, 1000);
	}
	LCUIMutex_Unlock(&task->mutex);
}

void PictureLoaderTask_Free(PictureLoaderTask task)
{
	LCUICond_Destroy(&task->cond);
	LCUIMutex_Destroy(&task->mutex);
	if (task->file) {
		free(task->file);
	}
	free(task);
}

 /** 图片加载器线程 */
static void PictureLoader_Thread(void *arg)
{
	PictureLoaderTask task;
	PictureLoader loader = arg;

	LCUIMutex_Lock(&loader->mutex);
	while (loader->is_running) {
		_DEBUG_MSG("wait load\n");
		while (loader->is_running && loader->tasks.length == 0) {
			LCUICond_Wait(&loader->cond, &loader->mutex);
		}
		_DEBUG_MSG("start load\n");
		if (!loader->is_running) {
			break;
		}
		while (loader->tasks.length > 3) {
			task = LinkedList_Get(&loader->tasks, 0);
			LinkedList_Delete(&loader->tasks, 0);
			PictureLoaderTask_Free(task);
		}
		task = LinkedList_Get(&loader->tasks, 0);
		LinkedList_Delete(&loader->tasks, 0);
		if (task) {
			LCUIMutex_Unlock(&loader->mutex);
			PictureLoaderTask_Run(task);
			PictureLoaderTask_Free(task);
			LCUIMutex_Lock(&loader->mutex);
		}
	}
	LCUIMutex_Unlock(&loader->mutex);
	LCUIThread_Exit(NULL);
}

/* 设置图片加载器的目标 */
static void PictureLoader_Reset(PictureLoader loader)
{
	LinkedListNode *node;
	PictureLoaderTask task;

	LCUIMutex_Lock(&loader->mutex);
	for (LinkedList_Each(node, &loader->tasks)) {
		task = node->data;
		if (!task->active) {
			PictureLoaderTask_Free(task);
		}
		task->active = FALSE;
	}
	LinkedList_Clear(&loader->tasks, NULL);
	LCUICond_Signal(&loader->cond);
	LCUIMutex_Unlock(&loader->mutex);
}

static PictureLoader PictureLoader_Create(PictureLoaderConfig config)
{
	ASSIGN(loader, PictureLoader);
	loader->config = *config;
	loader->is_running = TRUE;
	LCUICond_Init(&loader->cond);
	LCUIMutex_Init(&loader->mutex);
	LinkedList_Init(&loader->tasks);
	LinkedList_Init(&loader->cache);
	LCUIThread_Create(&loader->thread, PictureLoader_Thread, loader);
	return loader;
}

static void PictureLoader_Free(PictureLoader loader)
{
	LinkedListNode *node;
	PictureCache cache;
	PictureLoaderTask task;

	LCUIMutex_Lock(&loader->mutex);
	loader->is_running = FALSE;
	LCUICond_Signal(&loader->cond);
	LCUIMutex_Unlock(&loader->mutex);
	LCUIThread_Join(loader->thread, NULL);

	for (LinkedList_Each(node, &loader->tasks)) {
		task = node->data;
		PictureLoaderTask_Free(task);
	}
	LinkedList_Clear(&loader->tasks, NULL);

	for (LinkedList_Each(node, &loader->cache)) {
		cache = node->data;
		Graph_Free(&cache->data);
	}
	LinkedList_Clear(&loader->cache, NULL);

	LCUICond_Destroy(&loader->cond);
	LCUIMutex_Destroy(&loader->mutex);
	free(loader);
}

static void PictureLoader_Load(PictureLoader loader, int key, 
			       const wchar_t *file, void *data)
{
	LinkedListNode *node;
	PictureLoaderTask task;
	PictureCache cache = NULL;
	
	LCUIMutex_Lock(&loader->mutex);
	for (LinkedList_Each(node, &loader->cache)) {
		cache = node->data;
		if (cache->key == key) {
			break;
		}
	}
	if (!cache || cache->key != key) {
		cache = malloc(sizeof(PictureCacheRec));
		Graph_Init(&cache->data);
		cache->key = key;
		LinkedList_Append(&loader->cache, cache);
	}
	task = PictureLoaderTask_Create(&loader->config, cache,
					file, data);
	LinkedList_Append(&loader->tasks, task);
	LCUICond_Signal(&loader->cond);
	LCUIMutex_Unlock(&loader->mutex);
}

void PictureView_ResetLoader(void *loader)
{
	PictureLoader_Reset(loader);
}

void PictureView_SetLoaderTask(void *loader, int key,
			       const wchar_t *file, void *data)
{
	PictureLoader_Load(loader, key, file, data);
}

void *PictureView_CreateLoader(PictureLoaderConfig config)
{
	return PictureLoader_Create(config);
}

void PictureView_FreeLoader(void *loader)
{
	PictureLoader_Free(loader);
}
