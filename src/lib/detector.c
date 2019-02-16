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
#include <LCUI/util/charset.h>
#include <LCUI/util.h>
#include <LCUI/thread.h>
#include "finder.h"
#include "detector.h"
#include "common.h"
#include "darknet.h"

// clang-format off

#define ASSERT(X)           \
	if (!(X)) {         \
		ret = -1;   \
		goto faild; \
	}

#define MODELS_DIR		L"models"
#define CFG_FILE_EXT		L".cfg"
#define DATA_FILE_EXT		L".data"
#define WEIGHTS_FILE_EXT	L".weights"
#define NAMES_FILE_EXT		L".names"
#define TEMPLATE_DATA_FILE	"detector/template/coco.data"
#define TEMPLATE_MODEL_FILE	"detector/template/darknet53.conv.74"
#define TEMPLATE_CFG_FILE	"detector/template/yolov3.cfg"
#define TASKS_DIR		"detector/tasks"

typedef enum DetectorState {
	DETECTOR_STATE_READY,
	DETECTOR_STATE_LOADING,
	DETECTOR_STATE_STOPPED
} DetectorState;

typedef struct ConfigReplaceItemRec_ {
	LCUI_BOOL replaced;
	const char *name;
	char value[64];
	int count;
} ConfigReplaceItemRec, *ConfigReplaceItem;

static struct DetetctorModule {
	darknet_network_t *net;
	darknet_detector_t *detector;

	LinkedList models;
	DetectorModel model;
	DetectorState state;
	LCUI_Thread threads[DETECTOR_TASK_TOTAL_NUM];

	LCUI_Mutex mutex;
	LCUI_Cond cond;

	int error;
	char error_msg[1024];
	wchar_t workdir[1024];
} dmod;

// clang-format on

static LCUI_BOOL TaskActive(DetectorTask task)
{
	return dmod.state == DETECTOR_STATE_READY &&
	       task->state == DETECTOR_TASK_STATE_RUNNING;
}

static void FreeIfValid(void *ptr)
{
	if (ptr) {
		free(ptr);
	}
}

static int Detector_SetError(int err, const char *msg)
{
	dmod.error = err;
	strncpy(dmod.error_msg, msg, 1024);
	dmod.error_msg[1023] = 0;
	return err;
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

static int DetectorModel_LoadClasses(DetectorModel model)
{
	char *path;
	darknet_dataconfig_t *cfg;

	if (!model->datacfg) {
		return -1;
	}
	path = EncodeANSI(model->datacfg);
	cfg = darknet_dataconfig_load(path);
	model->classes = darknet_dataconfig_get_classes(cfg);
	darknet_dataconfig_destroy(cfg);
	free(path);
	return 0;
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
	model->classes = 0;
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
	DetectorModel_LoadClasses(model);
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
	wcscpy(dmod.workdir, path);
	dir_len = wpathjoin(path, path, MODELS_DIR);
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
	darknet_detector_destroy(dmod.detector);
	darknet_network_destroy(dmod.net);
	dmod.detector = NULL;
}

static int Detector_Reload(void)
{
	int ret = 0;
	char *workdir;
	char *cfgfile;
	char *datacfgfile;
	char *weightsfile;
	darknet_config_t *cfg = NULL;
	darknet_dataconfig_t *datacfg = NULL;

	LCUIMutex_Lock(&dmod.mutex);
	dmod.state = DETECTOR_STATE_LOADING;
	Detector_Unload();

	workdir = EncodeANSI(dmod.model->path);
	cfgfile = EncodeANSI(dmod.model->cfg);
	datacfgfile = EncodeANSI(dmod.model->datacfg);
	weightsfile = EncodeANSI(dmod.model->weights);

	darknet_try
	{
		cfg = darknet_config_load(cfgfile);
		datacfg = darknet_dataconfig_load(datacfgfile);
		darknet_config_set_workdir(cfg, workdir);
		darknet_dataconfig_set_workdir(datacfg, workdir);

		dmod.net = darknet_network_create(cfg);
		darknet_network_load_weights(dmod.net, weightsfile);

		dmod.detector = darknet_detector_create(dmod.net, datacfg);
		dmod.state = DETECTOR_STATE_READY;
	}
	darknet_catch(err)
	{
		Detector_SetError(err, darknet_get_last_error_string());
		ret = -1;
	}
	darknet_etry;

	free(workdir);
	free(cfgfile);
	free(datacfgfile);
	free(weightsfile);

	darknet_config_destroy(cfg);
	darknet_dataconfig_destroy(datacfg);

	LCUICond_Signal(&dmod.cond);
	LCUIMutex_Unlock(&dmod.mutex);
	return ret;
}

const char *Detector_GetLastErrorString(void)
{
	return dmod.error_msg;
}

size_t Detector_GetModels(DetectorModel **out_models)
{
	size_t i = 0;
	DetectorModel *models;
	LinkedListNode *node;

	models = malloc(sizeof(DetectorModel *) * (dmod.models.length + 1));
	if (!models) {
		return 0;
	}
	for (LinkedList_Each(node, &dmod.models)) {
		models[i++] = node->data;
	}
	models[dmod.models.length] = NULL;
	*out_models = models;
	return dmod.models.length;
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
	dmod.error = -ENOENT;
	strcpy(dmod.error_msg, "model not found");
	return -ENOENT;
}

static int Detector_SaveFileAnnotations(const char *path, darknet_detections_t *dets)
{
	FILE *fp;
	DB_Tag tag;

	size_t i;
	wchar_t *wpath;
	wchar_t *datafile;
	darknet_detection_t *det;

	wpath = DecodeUTF8(path);
	datafile = GetAnnotationFileNameW(wpath);
	fp = wfopen(datafile, L"w+");
	if (!fp) {
		_DEBUG_MSG("cannot open file: %ls\n", datafile);
		free(wpath);
		return -1;
	}
	for (i = 0; i < dets->length; ++i) {
		det = &dets->list[i];
		tag = LCFinder_GetTag(det->best_name);
		if (tag) {
			fprintf(fp, "%d %f %f %f %f\n", tag->id, det->box.x,
				det->box.y, det->box.w, det->box.h);
		}
	}
	free(wpath);
	free(datafile);
	fclose(fp);
	return 0;
}

static int Detector_DetectFile(DB_File file)
{
	size_t i;
	DB_Tag tag;
	darknet_detections_t dets;

	darknet_detector_test(dmod.detector, file->path, &dets);
	for (i = 0; i < dets.length; ++i) {
		tag = LCFinder_GetTag(dets.list[i].best_name);
		if (!tag) {
			tag = LCFinder_AddTag(dets.list[i].best_name);
		}
		DBFile_AddTag(file, tag);
	}
	Detector_SaveFileAnnotations(file->path, &dets);
	darknet_detections_destroy(&dets);
	return 0;
}

static int DetectorModel_InitConfigFile(DetectorModel model)
{
	FILE *infile;
	FILE *outfile;
	size_t i;
	char buff[1024] = { 0 };
	ConfigReplaceItem item;
	ConfigReplaceItemRec items[4] = { { FALSE, "${batch}", "64", 1 },
					  { FALSE, "${subdivisions}", "8", 1 },
					  { FALSE, "${classes}", "0", 3 },
					  { FALSE, "${filters}", "0", 3 } };

	sprintf(items[2].value, "%zu", model->classes);
	sprintf(items[3].value, "%zu", (model->classes + 5) * 3);
	infile = fopen(TEMPLATE_CFG_FILE, "r");
	if (!infile) {
		return Detector_SetError(
		    -ENOENT, "cannot open file: " TEMPLATE_CFG_FILE);
	}
	outfile = wfopen(model->cfg, L"w+");
	if (!outfile) {
		fclose(infile);
		snprintf(buff, 1024, "cannot create file: %ls", model->cfg);
		return Detector_SetError(-EIO, buff);
	}
	while (fgets(buff, sizeof(buff), infile)) {
		for (i = 0; i < 4; ++i) {
			item = &items[i];
			if (item->replaced) {
				continue;
			}
			if (strreplace(buff, 1024, item->name, item->value)) {
				items[i].count -= 1;
				if (items[i].count < 1) {
					items[i].replaced = TRUE;
				}
				break;
			}
		}
		fputs(buff, outfile);
		fputchar('\n');
	}
	fputchar('\n');
	fclose(infile);
	fclose(outfile);
	return 0;
}

static int DetectorModel_InitDir(DetectorModel model)
{
	return wmkdir(model->path);
}

static int DetectorModel_InitDataFile(DetectorModel model)
{
	FILE *fp;
	char buff[1024];

	fp = wfopen(model->datacfg, L"w+");
	if (!fp) {
		snprintf(buff, 1024, "cannot create file: %ls", model->datacfg);
		return Detector_SetError(-EIO, buff);
	}
	fprintf(fp, "classes=%zu\n", model->classes);
	fprintf(fp, "names=%ls%ls\n", model->name, NAMES_FILE_EXT);
	fputs("backup=backup\n\n", fp);
	fclose(fp);
	return 0;
}

static int DetectorModel_InitWeightsFile(DetectorModel model)
{
	int ret;
	char *path;

	path = EncodeANSI(model->weights);
	ret = cp(TEMPLATE_MODEL_FILE, path);
	free(path);
	return ret;
}

wchar_t *DetectorModel_GetNamesFile(DetectorModel model)
{
	wchar_t *path;

	path = malloc(sizeof(wchar_t) * (PATH_LEN + 1));
	wpathjoin(path, model->path, model->name);
	wcscat(path, NAMES_FILE_EXT);
	return path;
}

static int DetectorModel_InitNamesFile(DetectorModel model, DB_Tag *tags)
{
	FILE *fp;
	size_t i;
	wchar_t *wpath;
	char *path;
	char buff[1024];

	wpath = DetectorModel_GetNamesFile(model);
	path = EncodeANSI(wpath);
	free(wpath);

	fp = fopen(path, "w+");
	if (!fp) {
		snprintf(buff, 1024, "cannot create file: %s", path);
		return Detector_SetError(-EIO, buff);
	}
	for (i = 0; i < model->classes; ++i) {
		fputs(tags[i]->name, fp);
		fputc('\n', fp);
	}
	fclose(fp);
	free(path);
	return 0;
}

DetectorModel Detector_CreateModel(const wchar_t *name)
{
	size_t i;
	wchar_t path[PATH_LEN + 1];
	DetectorModel model;
	DB_Tag *tags;

	model = malloc(sizeof(DetectorModelRec));
	if (!model) {
		return NULL;
	}
	wpathjoin(path, dmod.workdir, MODELS_DIR);
	wpathjoin(path, path, name);
	model->name = wcsdup2(name);
	model->path = wcsdup2(path);
	model->classes = DB_GetTagsOrderById(&tags);
	model->cfg = malloc(sizeof(char) * (PATH_LEN + 1));
	model->datacfg = malloc(sizeof(char) * (PATH_LEN + 1));
	model->weights = malloc(sizeof(char) * (PATH_LEN + 1));
	wpathjoin(model->cfg, path, name);
	wpathjoin(model->datacfg, path, name);
	wpathjoin(model->weights, path, name);
	wcscat(model->cfg, CFG_FILE_EXT);
	wcscat(model->datacfg, DATA_FILE_EXT);
	wcscat(model->weights, WEIGHTS_FILE_EXT);
	DetectorModel_InitDir(model);
	DetectorModel_InitDataFile(model);
	DetectorModel_InitConfigFile(model);
	DetectorModel_InitNamesFile(model, tags);
	DetectorModel_InitWeightsFile(model);
	for (i = 0; i < model->classes; ++i) {
		DBTag_Release(tags[i]);
	}
	free(tags);
	LinkedList_Append(&dmod.models, model);
	return model;
}

int Detector_DestroyModel(DetectorModel model)
{
	int ret = 0;
	LinkedListNode *node;

	if (dmod.model == model) {
		dmod.model = NULL;
	}
	ret |= MoveFileToTrashW(model->cfg);
	ret |= MoveFileToTrashW(model->datacfg);
	ret |= MoveFileToTrashW(model->weights);
	ret |= MoveFileToTrashW(model->path);
	for (LinkedList_Each(node, &dmod.models)) {
		if (model == node->data) {
			DetectorModel_Destroy(model);
			LinkedList_DeleteNode(&dmod.models, node);
			break;
		}
	}
	return ret;
}

DetectorTask Detector_CreateTask(DetectorTaskType type)
{
	DetectorTask task = malloc(sizeof(DetectorTaskRec));

	task->type = type;
	task->current = 0;
	task->total = 0;
	task->state = DETECTOR_STATE_READY;
	return task;
}

void Detector_CancelTask(DetectorTask task)
{
	if (dmod.threads[task->type]) {
		task->state = DETECTOR_TASK_STATE_CANCELED;
		LCUIThread_Join(dmod.threads[task->type], NULL);
		dmod.threads[task->type] = 0;
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

static int Detector_RunTrainingTask(DetectorTask task)
{
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
	task->state = DETECTOR_TASK_STATE_PREPARING;
	if (!dmod.detector) {
		while (dmod.state == DETECTOR_STATE_LOADING) {
			LCUICond_TimedWait(&dmod.cond, &dmod.mutex, 2000);
		}
		if (!dmod.detector) {
			Detector_Reload();
		}
	}
	task->state = DETECTOR_TASK_STATE_RUNNING;
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
		for (i = 0; TaskActive(task); ++count, ++i) {
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
	if (terms.dirs) {
		free(terms.dirs);
		terms.dirs = NULL;
	}
	LinkedList_ClearData(&files, OnFreeDBFile);
	if (task->state == DETECTOR_TASK_STATE_RUNNING) {
		task->state = DETECTOR_TASK_STATE_FINISHED;
	}
	return 0;
}

static void Detector_TaskThread(void *arg)
{
	Detector_RunTask(arg);
	LCUIThread_Exit(NULL);
}

int Detector_RunTaskAync(DetectorTask task)
{
	if (task->state != DETECTOR_TASK_STATE_READY) {
		return -1;
	}
	LCUIThread_Create(&dmod.threads[task->type], Detector_TaskThread, task);
	return 0;
}
