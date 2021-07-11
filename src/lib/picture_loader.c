/* ***************************************************************************
 * picture_loader.h -- picture loader
 *
 * Copyright (C) 2020 by Liu Chao <lc-soft@live.cn>
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
 * picture_loader.h -- 图片加载与缓存管理
 *
 * 版权所有 (C) 2020 归属于 刘超 <lc-soft@live.cn>
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

#include <string.h>

#include <LCUI.h>
#include <LCUI/thread.h>
#include <LCUI/graph.h>

#include "build.h"
#include "file_storage.h"
#include "picture_loader.h"

typedef enum PictureLoaderTaskState {
	PICTURE_LOADER_TASK_STATE_PENDING,
	PICTURE_LOADER_TASK_STATE_LOADING,
	PICTURE_LOADER_TASK_STATE_SUCCESS,
	PICTURE_LOADER_TASK_STATE_ERROR
} PictureLoaderTaskState;

typedef struct PictureLoaderRec_ {
	LCUI_BOOL active;
	LCUI_BOOL loading;
	LCUI_Thread thread;
	LCUI_Cond cond;
	LCUI_Mutex mutex;

	/* LinkedList<PictureLoaderTask> */
	LinkedList pictures;
	unsigned pictures_max_len;

	int storage;
	void (*on_progress)(const wchar_t *, float);
	void (*on_success)(const wchar_t *, LCUI_Graph *);
	void (*on_error)(const wchar_t *);
	void (*on_free)(const wchar_t *);
} PictureLoaderRec;

typedef struct PictureLoaderTaskRec_ {
	LCUI_Graph data;
	LCUI_BOOL active;
	LCUI_BOOL changed;
	PictureLoader loader;
	PictureLoaderTaskState state;

	float progress;
	wchar_t *filepath;
} PictureLoaderTaskRec, *PictureLoaderTask;

static void PictureLoaderTask_Destroy(void *data)
{
	PictureLoaderTask task = data;

	task->loader->on_free(task->filepath);
	task->state = PICTURE_LOADER_TASK_STATE_PENDING;
	Graph_Free(&task->data);
	free(task->filepath);
	task->filepath = NULL;
	task->loader = NULL;
	free(task);
}

static void PictureLoaderTask_Dispatch(PictureLoaderTask task)
{
	switch (task->state) {
	case PICTURE_LOADER_TASK_STATE_SUCCESS:
		task->active = FALSE;
		task->loader->on_success(task->filepath, &task->data);
		Logger_Debug("[picture-loader] [%ls] picture size: %u, %u\n",
			     task->filepath, task->data.width,
			     task->data.height);
		break;
	case PICTURE_LOADER_TASK_STATE_ERROR:
		task->active = FALSE;
		task->loader->on_error(task->filepath);
		Logger_Debug("[picture-loader] [%ls] error\n", task->filepath);
		break;
	default:
		task->loader->on_progress(task->filepath, task->progress);
		break;
	}
	task->changed = FALSE;
}

static void PictureLoader_OnEnd(LCUI_Graph *data, PictureLoaderTask task)
{
	if (data) {
		task->data = *data;
		task->progress = 100;
		task->state = PICTURE_LOADER_TASK_STATE_SUCCESS;
		/* Reset the data so that filestorage does not free it after
		 * this function call */
		Graph_Init(data);
	} else {
		Graph_Init(&task->data);
		task->state = PICTURE_LOADER_TASK_STATE_ERROR;
	}
	LCUI_PostSimpleTask(PictureLoaderTask_Dispatch, task, NULL);
	LCUIMutex_Lock(&task->loader->mutex);
	task->loader->loading = FALSE;
	LCUICond_Signal(&task->loader->cond);
	LCUIMutex_Unlock(&task->loader->mutex);
}

static void PictureLoader_OnProgress(float progress, PictureLoaderTask task)
{
	task->progress = progress;
	if (!task->changed) {
		task->changed = TRUE;
		LCUI_PostSimpleTask(PictureLoaderTask_Dispatch, task, NULL);
	}
}

static void PictureLoader_Thread(void *arg)
{
	PictureLoader loader = arg;
	PictureLoaderTask task;
	LinkedListNode *node;

	LCUIMutex_Lock(&loader->mutex);
	while (loader->active) {
		LCUICond_TimedWait(&loader->cond, &loader->mutex, 1000);
		if (loader->loading) {
			continue;
		}
		task = NULL;
		for (LinkedList_Each(node, &loader->pictures)) {
			task = node->data;
			if (task->state == PICTURE_LOADER_TASK_STATE_PENDING) {
				break;
			}
			task = NULL;
		}
		if (!task || task->state != PICTURE_LOADER_TASK_STATE_PENDING) {
			continue;
		}
		task->active = TRUE;
		task->state = PICTURE_LOADER_TASK_STATE_LOADING;
		Logger_Debug("[picture-loader] [%ls] loading\n",
			     task->filepath);
		FileStorage_GetImage(loader->storage, task->filepath,
				     PictureLoader_OnEnd,
				     PictureLoader_OnProgress, task);
		loader->loading = TRUE;
	}
	LCUIMutex_Unlock(&loader->mutex);
	LCUIThread_Exit(NULL);
}

PictureLoader PictureLoader_Create(
    int storage, void (*on_progress)(const wchar_t *, float),
    void (*on_success)(const wchar_t *, LCUI_Graph *),
    void (*on_error)(const wchar_t *), void (*on_free)(const wchar_t *))
{
	PictureLoader loader = NEW(PictureLoaderRec, 1);

	loader->active = TRUE;
	loader->storage = storage;
	loader->pictures_max_len = 6;
	loader->on_progress = on_progress;
	loader->on_success = on_success;
	loader->on_error = on_error;
	loader->on_free = on_free;
	LCUICond_Init(&loader->cond);
	LCUIMutex_Init(&loader->mutex);
	LCUIThread_Create(&loader->thread, PictureLoader_Thread, loader);
	return loader;
}

void PictureLoader_Destroy(PictureLoader loader)
{
	LCUIMutex_Lock(&loader->mutex);
	loader->active = FALSE;
	LCUICond_Signal(&loader->cond);
	LCUIMutex_Unlock(&loader->mutex);
	LCUIThread_Join(loader->thread, NULL);
	LinkedList_Clear(&loader->pictures, PictureLoaderTask_Destroy);
	LCUIMutex_Destroy(&loader->mutex);
	LCUICond_Destroy(&loader->cond);
	free(loader);
}

void PictureLoader_Load(PictureLoader loader, const wchar_t *filepath)
{
	LinkedListNode *node;
	PictureLoaderTask task = NULL;

	LCUIMutex_Lock(&loader->mutex);
	for (LinkedList_Each(node, &loader->pictures)) {
		task = node->data;
		if (wcscmp(task->filepath, filepath) != 0) {
			continue;
		}
		PictureLoaderTask_Dispatch(task);
		if (task->state == PICTURE_LOADER_TASK_STATE_PENDING) {
			LinkedList_Unlink(&loader->pictures, node);
			LinkedList_InsertNode(&loader->pictures, 0, node);
		}
		LCUIMutex_Unlock(&loader->mutex);
		return;
	}
	if (loader->pictures.length >= loader->pictures_max_len) {
		for (LinkedList_EachReverse(node, &loader->pictures)) {
			task = node->data;
			if (!task->active) {
				Logger_Debug("[picture-loader] [%ls] removed\n",
					     task->filepath);
				PictureLoaderTask_Destroy(task);
				LinkedList_DeleteNode(&loader->pictures, node);
				break;
			}
		}
	}
	task = NEW(PictureLoaderTaskRec, 1);
	task->active = FALSE;
	task->progress = 0;
	task->loader = loader;
	task->changed = FALSE;
	task->filepath = wcsdup2(filepath);
	task->state = PICTURE_LOADER_TASK_STATE_PENDING;
	Graph_Init(&task->data);
	LinkedList_Insert(&loader->pictures, 0, task);
	LCUICond_Signal(&loader->cond);
	LCUIMutex_Unlock(&loader->mutex);
	Logger_Debug("[picture-loader] [%ls] pending\n", task->filepath);
}
