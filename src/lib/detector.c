/* ***************************************************************************
 * detector.c -- detector
 *
 * Copyright (C) 2019 by Liu Chao <lc-soft@live.cn>
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
 * detector.c -- 检测器，基于 darknet 实现
 *
 * 版权所有 (C) 2019 归属于 刘超 <lc-soft@live.cn>
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
#include <errno.h>
#include <LCUI_Build.h>
#include <LCUI/types.h>
#include <LCUI/font/charset.h>
#include <LCUI/util.h>
#include <LCUI/thread.h>
#include "finder.h"
#include "detector.h"
#include "common.h"
#include "darknet.h"

// clang-format off

#define CFG_FILE_EXT		L".cfg"
#define DATA_FILE_EXT		L".data"
#define WEIGHTS_FILE_EXT	L".weights"

#define TaskActive(T) (!T->cancel && dmod.state == DETECTOR_STATE_READY)

typedef enum DetectorState {
	DETECTOR_STATE_READY,
	DETECTOR_STATE_LOADING,
	DETECTOR_STATE_STOPPED
} DetectorState;

typedef struct DetectorModelRec_ {
	wchar_t *name;
	wchar_t *cfg;
	wchar_t *datacfg;
	wchar_t *weights;
	wchar_t *path;
} DetectorModelRec, *DetectorModel;

static struct DetetctorModule {
	darknet_network_t *net;
	darknet_detector_t *detector;

	LinkedList models;
	DetectorModel model;
	DetectorState state;
	LCUI_Thread threads[DETECTOR_TASK_TOTAL_NUM];

	LCUI_Mutex mutex;
	LCUI_Cond cond;
} dmod;

// clang-format on

static void FreeIfValid(void *ptr)
{
	if (ptr) {
		free(ptr);
	}
}

static void DetectorModel_Destroy(DetectorModel model)
{
	FreeIfValid(model->name);
	FreeIfValid(model->path);
	FreeIfValid(model->weights);
	FreeIfValid(model->datacfg);
	FreeIfValid(model->cfg);
	FreeIfValid(model);
}

static DetectorModel DetectorModel_Create(const wchar_t *modeldir)
{
	LCUI_Dir dir;
	LCUI_DirEntry *entry;
	DetectorModel model;

	size_t dir_len;
	wchar_t *name, *file, path[PATH_LEN + 1];

	model = malloc(sizeof(DetectorModelRec));
	if (!model) {
		return NULL;
	}
	wcscpy(path, modeldir);
	if (LCUI_OpenDirW(path, &dir) != 0) {
		LOG("[detector] open model directory failed: %ls\n", path);
		free(model);
		return NULL;
	}
	model->cfg = NULL;
	model->datacfg = NULL;
	model->weights = NULL;
	model->path = wcsdup2(modeldir);
	model->name = wcsdup2(wgetfilename(modeldir));

	dir_len = wcslen(modeldir);
	path[dir_len++] = PATH_SEP;
	file = path + dir_len;

	while ((entry = LCUI_ReadDirW(&dir))) {
		name = LCUI_GetFileNameW(entry);
		if (name[0] == '.') {
			continue;
		}
		if (!LCUI_FileIsRegular(entry)) {
			continue;
		}
		wcscpy(file, name);
		if (wcheckfileext(name, CFG_FILE_EXT) == 0) {
			model->cfg = wcsdup2(path);
		} else if (wcheckfileext(name, DATA_FILE_EXT) == 0) {
			model->datacfg = wcsdup2(path);
		} else if (wcheckfileext(name, WEIGHTS_FILE_EXT) == 0) {
			model->weights = wcsdup2(path);
		}
	}
	return model;
}

int Detector_Init(const wchar_t *workdir)
{
	LCUI_Dir dir;
	LCUI_DirEntry *entry;
	DetectorModel model;

	size_t dir_len;
	wchar_t *name, *file, path[PATH_LEN + 1];

	wpathjoin(path, workdir, L"detector");
	dir_len = wpathjoin(path, path, L"models");
	if (LCUI_OpenDirW(path, &dir) != 0) {
		LOG("[detector] open directory failed: %ls\n", path);
		return -ENOENT;
	}
	path[dir_len++] = PATH_SEP;
	file = path + dir_len;
	LOG("[detector] load models...\n");
	while ((entry = LCUI_ReadDirW(&dir))) {
		name = LCUI_GetFileNameW(entry);
		if (name[0] == '.') {
			continue;
		}
		if (!LCUI_FileIsDirectory(entry)) {
			continue;
		}
		wcscpy(file, name);
		model = DetectorModel_Create(path);
		if (model) {
			LOG("[detector] load model: %ls\n", model->name);
			LinkedList_Append(&dmod.models, model);
		}
	}
	return 0;
}

static void DestroyDetectorModel(void *arg)
{
	DetectorModel_Destroy(arg);
}

void Detector_Free(void)
{
	int i;

	dmod.state = DETECTOR_STATE_STOPPED;
	for (i = 0; i < DETECTOR_TASK_TOTAL_NUM; ++i) {
		LCUIThread_Join(dmod.threads[i], NULL);
	}
	dmod.model = NULL;
	LinkedList_ClearData(&dmod.models, DestroyDetectorModel);
}

static void Detector_Unload(void)
{
	if (!dmod.detector) {
		return;
	}
	darknet_network_destroy(dmod.net);
	darknet_detector_destroy(dmod.detector);
	dmod.detector = NULL;
}

#define ASSERT(X) if (!(X)) { ret = -1;goto faild;}

static int Detector_Reload(void)
{
	int ret = 0;
	char *workdir;
	char *cfgfile;
	char *datacfgfile;
	char *weightsfile;
	darknet_config_t *cfg;
	darknet_dataconfig_t *datacfg;

	LCUIMutex_Lock(&dmod.mutex);
	dmod.state = DETECTOR_STATE_LOADING;
	Detector_Unload();

	workdir = EncodeANSI(dmod.model->path);
	cfgfile = EncodeANSI(dmod.model->cfg);
	datacfgfile = EncodeANSI(dmod.model->datacfg);
	weightsfile = EncodeANSI(dmod.model->weights);

	ASSERT(cfg = darknet_config_create(cfgfile));
	ASSERT(datacfg = darknet_dataconfig_create(datacfgfile));
	darknet_config_set_workdir(cfg, workdir);
	darknet_dataconfig_set_workdir(datacfg, workdir);

	ASSERT(dmod.net = darknet_network_create(cfg));
	ASSERT(darknet_network_load_weights(dmod.net, weightsfile) == 0);
	darknet_config_destroy(cfg);

	ASSERT(dmod.detector = darnet_detector_create(dmod.net, datacfg));
	dmod.state = DETECTOR_STATE_READY;

faild:
	free(workdir);
	free(cfgfile);
	free(datacfgfile);
	free(weightsfile);

	LCUICond_Signal(&dmod.cond);
	LCUIMutex_Unlock(&dmod.mutex);
	return ret;
}

wchar_t **Detector_GetModels(void)
{
	size_t i = 0;
	wchar_t **models;
	DetectorModel model;
	LinkedListNode *node;

	models = malloc(sizeof(wchar_t *) * (dmod.models.length + 1));
	if (!models) {
		return NULL;
	}
	for (LinkedList_Each(node, &dmod.models)) {
		model = node->data;
		models[i++] = wcsdup2(model->name);
	}
	models[dmod.models.length] = NULL;
	return models;
}

int Detector_SetModel(const wchar_t *name)
{
	DetectorModel model;
	LinkedListNode *node;

	for (LinkedList_Each(node, &dmod.models)) {
		model = node->data;
		if (wcscmp(model->name, name) == 0) {
			dmod.model = model;
			return Detector_Reload();
		}
	}
	return -ENOENT;
}

static DB_Tag AllocTag(const char *name)
{
	size_t i;
	DB_Tag tag;
	DB_Tag *tags;

	for (i = 0; i < finder.n_tags; ++i) {
		tag = finder.tags[i];
		if (strcmp(tag->name, name) == 0) {
			return tag;
		}
	}
	tag = DB_AddTag(name);
	if (tag) {
		++finder.n_tags;
		tags = realloc(finder.tags, sizeof(DB_Tag) * finder.n_tags);
		if (!tags) {
			--finder.n_tags;
			return NULL;
		}
		finder.tags[finder.n_tags - 1] = tag;
	}
	return tag;
}

static int Detector_DetectFile(DB_File file)
{
	size_t i;
	DB_Tag tag;
	darknet_detections_t dets;

	darknet_detector_test(dmod.detector, file->path, &dets);
	for (i = 0; i < dets.length; ++i) {
		tag = AllocTag(dets.list[i].best_name);
		if (tag) {
			DBFile_AddTag(file, tag);
		}
	}
	darknet_detections_destroy(&dets);
	return 0;
}

DetectorTask Detector_CreateTask(DetectorTaskType type)
{
	DetectorTask task = malloc(sizeof(DetectorTaskRec));

	task->type = type;
	task->current = 0;
	task->total = 0;
	task->cancel = FALSE;
	task->state = DETECTOR_STATE_READY;
	return task;
}

void Detector_CancelTask(DetectorTask task)
{
	if (!task->cancel) {
		task->cancel = TRUE;
		LCUIThread_Join(dmod.threads[task->type], NULL);
	}
}

void Detector_FreeTask(DetectorTask task)
{
	Detector_CancelTask(task);
	free(task);
}

static void OnFreeDBFile(void *arg)
{
	DBFile_Release(arg);
}

int Detector_RunTask(DetectorTask task)
{
	size_t i;
	size_t count;
	DB_File file;
	DB_Query query;
	LinkedList files;
	LinkedListNode *node;
	DB_QueryTermsRec terms = { 0 };

	if (!dmod.model) {
		LOG("[detector] not model selected\n");
		return -ENOENT;
	}
	if (!dmod.detector) {
		while (dmod.state == DETECTOR_STATE_LOADING) {
			LCUICond_TimedWait(&dmod.cond, &dmod.mutex, 2000);
		}
		if (!dmod.detector) {
			Detector_Reload();
		}
	}

	terms.limit = 512;
	terms.modify_time = DESC;
	terms.n_dirs = LCFinder_GetSourceDirList(&terms.dirs);
	if (terms.n_dirs == finder.n_dirs) {
		free(terms.dirs);
		terms.dirs = NULL;
		terms.n_dirs = 0;
	}

	query = DB_NewQuery(&terms);
	task->total = DBQuery_GetTotalFiles(query);
	DB_DeleteQuery(query);

	LinkedList_Init(&files);
	for (count = 0; TaskActive(task) && count < task->total;) {
		query = DB_NewQuery(&terms);
		for (i = 0; !task->cancel; ++count, ++i) {
			file = DBQuery_FetchFile(query);
			if (!file) {
				break;
			}
			LinkedList_Append(&files, file);
		}
		terms.offset += terms.limit;
		DB_DeleteQuery(query);
		if (i < terms.limit) {
			break;
		}
	}
	for (LinkedList_Each(node, &files)) {
		if (!TaskActive(task)) {
			break;
		}
		Detector_DetectFile(node->data);
		++task->current;
	}
	LinkedList_ClearData(&files, OnFreeDBFile);
	return 0;
}

static void Detector_TaskThread(void *arg)
{
	Detector_RunTask(arg);
	Detector_FreeTask(arg);
	LCUIThread_Exit(NULL);
}

void Detector_RunTaskAync(DetectorTask task)
{
	LCUIThread_Create(&dmod.threads[task->type], Detector_TaskThread, task);
}
