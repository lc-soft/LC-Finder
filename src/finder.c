/* ***************************************************************************
 * finder.c -- main code of LC-Finder, responsible for the initialization of
 * the LC-Finder and the scheduling of other functions.
 *
 * Copyright (C) 2016-2017 by Liu Chao <lc-soft@live.cn>
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
 * finder.c -- LC-Finder 主程序代码，负责整个程序的初始化和其它功能的调度。
 *
 * 版权所有 (C) 2016-2017 归属于 刘超 <lc-soft@live.cn>
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include "finder.h"
#include "i18n.h"
#include "ui.h"
#include "file_storage.h"
#include <LCUI/font/charset.h>

#define DEBUG
#define LANG_DIR L"locales"
#define LANG_FILE_EXT L".yaml"
#define CONFIG_FILE L"config.bin"
#define STORAGE_FILE L"storage.db"

#define THUMB_CACHE_SIZE (64 * 1024 * 1024)

#ifdef ASSERT
#undef ASSERT
#endif

#define ASSERT(X)                      \
	if (!(X)) {                    \
		_DEBUG_MSG("error\n"); \
		goto error;            \
	}

Finder finder;

typedef struct DirStatusDataPackRec_ {
	FileSyncStatus status;
	DB_Dir dir;
} DirStatusDataPackRec, *DirStatusDataPack;

typedef struct EventPackRec_ {
	LCFinder_EventHandler handler;
	void *data;
} EventPackRec, *EventPack;

typedef struct FileSyncDataPackRec_ {
	FileSyncStatus status;
	wchar_t *path;
	size_t path_len;
} FileSyncDataPackRec, *FileSyncDataPack;

static void OnEvent(LCUI_Event e, void *arg)
{
	EventPack pack = e->data;
	pack->handler(pack->data, arg);
}

int LCFinder_BindEvent(int event_id, LCFinder_EventHandler handler, void *data)
{
	EventPack pack = NEW(EventPackRec, 1);
	pack->handler = handler;
	pack->data = data;
	return EventTrigger_Bind(finder.trigger, event_id, OnEvent, pack, free);
}

int LCFinder_TriggerEvent(int event_id, void *data)
{
	return EventTrigger_Trigger(finder.trigger, event_id, data);
}

DB_Dir LCFinder_GetDir(const char *dirpath)
{
	size_t i;
	DB_Dir dir;
	for (i = 0; i < finder.n_dirs; ++i) {
		dir = finder.dirs[i];
		if (dir && strcmp(dir->path, dirpath) == 0) {
			return dir;
		}
	}
	return NULL;
}

static wchar_t *LCFinder_CreateThumbDB(const char *dirpath)
{
	ThumbDB db;
	char dbpath[PATH_LEN], path[PATH_LEN], name[44];

	strcpy(path, dirpath);
	EncodeSHA1(name, path, strlen(path));
	LCUI_EncodeString(dbpath, finder.thumbs_dir, PATH_LEN - 1,
			  ENCODING_UTF8);
	pathjoin(dbpath, dbpath, name);
	db = ThumbDB_Open(dbpath);
	if (!db) {
		return NULL;
	}
	Dict_Add(finder.thumb_dbs, path, db);
	return DecodeUTF8(dbpath);
}

DB_Dir LCFinder_AddDir(const char *dirpath, const char *token, int visible)
{
	char *path;
	size_t i, len;
	wchar_t **paths;
	DB_Dir dir, *dirs;
	len = strlen(dirpath);
	path = malloc((len + 2) * sizeof(char));
	strcpy(path, dirpath);
	if (path[len - 1] != PATH_SEP) {
		path[len] = PATH_SEP;
		path[len + 1] = 0;
	}
	for (i = 0; i < finder.n_dirs; ++i) {
		dir = finder.dirs[i];
		if (dir && strstr(dir->path, path)) {
			free(path);
			return NULL;
		}
	}
	dir = DB_AddDir(dirpath, token, visible);
	if (!dir) {
		free(path);
		return NULL;
	}
	i = finder.n_dirs;
	finder.n_dirs += 1;
	dirs = realloc(finder.dirs, sizeof(DB_Dir) * finder.n_dirs);
	if (!dirs) {
		finder.n_dirs -= 1;
		return NULL;
	}
	paths = realloc(finder.thumb_paths, sizeof(wchar_t *) * finder.n_dirs);
	if (!path) {
		finder.n_dirs -= 1;
		return NULL;
	}
	dirs[i] = dir;
	paths[i] = LCFinder_CreateThumbDB(dir->path);
	finder.dirs = dirs;
	finder.thumb_paths = paths;
	return dir;
}

void LCFinder_DeleteDir(DB_Dir dir)
{
	size_t i;
	SyncTask t;
	wchar_t *wpath, *wtoken;
	for (i = 0; i < finder.n_dirs; ++i) {
		if (dir == finder.dirs[i]) {
			break;
		}
	}
	if (i >= finder.n_dirs) {
		return;
	}
	finder.dirs[i] = NULL;
	wpath = DecodeUTF8(dir->path);
	/* 准备清除文件列表缓存 */
	t = SyncTask_NewW(finder.fileset_dir, wpath);
	LCFinder_TriggerEvent(EVENT_DIR_DEL, dir);
	SyncTask_ClearCache(t);
	if (dir->token) {
		wtoken = DecodeUTF8(dir->token);
		RemoveFolderAccessW(wtoken);
		free(wtoken);
	}
	free(wpath);
	/* 准备清除缩略图数据库 */
	wpath = finder.thumb_paths[i];
	finder.thumb_paths[i] = NULL;
	Dict_Delete(finder.thumb_dbs, dir->path);
#ifdef _WIN32
	_wremove(wpath);
#endif
	/* 删除数据库中的源文件夹记录 */
	DB_DeleteDir(dir);
	free(dir->path);
	free(dir);
}

static void OnCloseFileCache(void *privdata, void *data)
{
	SyncTask_CloseCache(data);
	SyncTask_Delete(data);
}

size_t LCFinder_DeleteFiles(char *const *files, size_t nfiles,
			    int (*onstep)(void *, size_t, size_t),
			    void *privdata)
{
	size_t i, len;
	wchar_t *path;
	SyncTask task;
	Dict *tasks = StrDict_Create(NULL, OnCloseFileCache);
	for (i = 0; i < nfiles; ++i) {
		DB_Dir dir = LCFinder_GetSourceDir(files[i]);
		if (!dir) {
			continue;
		}
		task = Dict_FetchValue(tasks, dir->path);
		if (!task) {
			len = strlen(dir->path) + 1;
			path = malloc(sizeof(wchar_t) * len);
			LCUI_DecodeString(path, dir->path, len, ENCODING_UTF8);
			task = SyncTask_NewW(finder.fileset_dir, path);
			SyncTask_OpenCacheW(task, NULL);
			Dict_Add(tasks, dir->path, task);
			free(path);
			path = NULL;
		}
		len = strlen(files[i]) + 1;
		path = malloc(sizeof(wchar_t) * len);
		LCUI_DecodeString(path, files[i], len, ENCODING_UTF8);
		if (0 == SyncTask_DeleteFileW(task, path)) {
			DB_DeleteFile(files[i]);
			MoveFileToTrash(files[i]);
		}
		if (onstep) {
			if (0 != onstep(privdata, i, nfiles)) {
				break;
			}
		}
	}
	StrDict_Release(tasks);
	return i;
}

static void SyncAddedFile(void *data, const FileInfo info)
{
	char path[PATH_LEN];
	DirStatusDataPack pack = data;
	int ctime = (int)info->ctime;
	int mtime = (int)info->mtime;
	pack->status->synced_files += 1;
	LCUI_EncodeString(path, info->path, PATH_LEN, ENCODING_UTF8);
	DB_AddFile(pack->dir, path, ctime, mtime);
	// wprintf(L"sync: add file: %s, ctime: %d\n", wpath, ctime);
}

static void SyncChangedFile(void *data, const FileInfo info)
{
	char path[PATH_LEN];
	int ctime = (int)info->ctime;
	int mtime = (int)info->mtime;
	DirStatusDataPack pack = data;
	pack->status->synced_files += 1;
	LCUI_EncodeString(path, info->path, PATH_LEN, ENCODING_UTF8);
	DB_UpdateFileTime(pack->dir, path, ctime, mtime);
}

static void SyncDeletedFile(void *data, const FileInfo info)
{
	char path[PATH_LEN];
	DirStatusDataPack pack = data;
	pack->status->synced_files += 1;
	LCUI_EncodeString(path, info->path, PATH_LEN, ENCODING_UTF8);
	DB_DeleteFile(path);
	// wprintf(L"sync: delete file: %s\n", wpath);
}

DB_Dir LCFinder_GetSourceDir(const char *filepath)
{
	size_t i;
	DB_Dir dir;
	for (i = 0; i < finder.n_dirs; ++i) {
		dir = finder.dirs[i];
		if (dir && strstr(filepath, dir->path) == filepath) {
			return dir;
		}
	}
	return NULL;
}

size_t LCFinder_GetSourceDirList(DB_Dir **outdirs)
{
	size_t i, count;
	DB_Dir *dirs;
	dirs = malloc((finder.n_dirs + 1) * sizeof(DB_Dir));
	if (!dirs) {
		return -ENOMEM;
	}
	for (count = 0, i = 0; i < finder.n_dirs; ++i) {
		if (!finder.dirs[i]) {
			continue;
		}
		if (!finder.open_private_space && !finder.dirs[i]->visible) {
			continue;
		}
		dirs[count++] = finder.dirs[i];
	}
	dirs[count] = NULL;
	*outdirs = dirs;
	return count;
}

int64_t LCFinder_GetThumbDBTotalSize(void)
{
	size_t i;
	struct stat buf;
	int64_t sum_size;

	for (i = 0, sum_size = 0; i < finder.n_dirs; ++i) {
		if (!finder.thumb_paths[i]) {
			continue;
		}
		if (wgetfilestat(finder.thumb_paths[i], &buf) == 0) {
			sum_size += buf.st_size;
		}
	}
	return sum_size;
}

static void LCFinder_SwitchTask(FileSyncStatus s);
static void LCFinder_ScanDir(FileSyncStatus s, const wchar_t *path);

static void LCFinder_OnScanFinished(FileSyncStatus s)
{
	size_t i;
	if (s->task_i < finder.n_dirs - 1) {
		s->task_i += 1;
		if (s->task) {
			s->added_files += s->task->added_files;
			s->deleted_files += s->task->deleted_files;
			s->changed_files += s->task->changed_files;
			SyncTask_Finish(s->task);
		}
		LCFinder_SwitchTask(s);
		return;
	}
	DB_Begin();
	s->state = STATE_SAVING;
	LOG("\n\nstart sync\n");
	for (i = 0; i < finder.n_dirs; ++i) {
		DirStatusDataPackRec pack;
		pack.dir = finder.dirs[i];
		if (!pack.dir) {
			continue;
		}
		pack.status = s;
		s->task = s->tasks[i];
		SyncTask_InAddedFiles(s->task, SyncAddedFile, &pack);
		SyncTask_InDeletedFiles(s->task, SyncDeletedFile, &pack);
		SyncTask_InChangedFiles(s->task, SyncChangedFile, &pack);
		SyncTask_Commit(s->task);
		SyncTask_Delete(s->task);
		s->task = NULL;
	}
	DB_Commit();
	LOG("\n\nend sync\n");
	s->state = STATE_FINISHED;
	s->task = NULL;
	s->task_i = 0;
	free(s->tasks);
	s->tasks = NULL;
	if (s->callback) {
		s->callback(s->data);
	}
}

static void LCFinder_OnScanFile(FileStatus *status, void *data)
{
	unsigned int ctime, mtime;
	FileSyncDataPack pack = data;

	if (!status) {
		goto finish;
	}
	ctime = (unsigned int)status->ctime;
	mtime = (unsigned int)status->mtime;
	SyncTask_AddFileW(pack->status->task, pack->path, ctime, mtime);
finish:
	pack->status->scaned_files += 1;
	if (pack->status->scaned_dirs == pack->status->dirs &&
	    pack->status->scaned_files == pack->status->files) {
		LCFinder_OnScanFinished(pack->status);
	}
	free(pack->path);
	free(pack);
}

static void LCFinder_ScanFile(FileSyncStatus s, const wchar_t *path)
{
	FileSyncDataPack pack;
	size_t len = wcslen(path);
	pack = NEW(FileSyncDataPackRec, 1);
	pack->path = malloc(sizeof(wchar_t) * (len + 1));
	wcsncpy(pack->path, path, len);
	if (path[len - 1] == PATH_SEP) {
		len -= 1;
	}
	pack->status = s;
	pack->path[len] = 0;
	pack->path_len = len;
	pack->status->files += 1;
	FileStorage_GetStatus(finder.storage, path, FALSE, LCFinder_OnScanFile,
			      pack);
}

static void LCFinder_OnScanDir(FileStatus *status, FileStream stream,
			       void *data)
{
	char *p;
	char buf[PATH_LEN];
	size_t max_len;
	wchar_t *name;
	wchar_t path[PATH_LEN];
	FileSyncDataPack pack = data;

	if (!status || !stream) {
		goto finish;
	}
	path[PATH_LEN - 1] = 0;
	name = path + pack->path_len;
	max_len = PATH_LEN - pack->path_len - 1;
	wcsncpy(path, pack->path, PATH_LEN - 1);
	if (path[pack->path_len - 1] != PATH_SEP) {
		path[pack->path_len] = PATH_SEP;
		path[pack->path_len + 1] = 0;
		name += 1;
	}
	while (1) {
		p = FileStream_ReadLine(stream, buf, PATH_LEN - 1);
		if (!p) {
			break;
		}
		buf[PATH_LEN - 1] = 0;
		buf[strlen(buf) - 1] = 0;
		LCUI_DecodeString(name, buf + 1, max_len, ENCODING_UTF8);
		if (buf[0] == 'd') {
			LCFinder_ScanDir(pack->status, path);
			continue;
		}
		LCFinder_ScanFile(pack->status, path);
	}

finish:
	pack->status->scaned_dirs += 1;
	if (pack->status->scaned_dirs == pack->status->dirs &&
	    pack->status->scaned_files == pack->status->files) {
		LCFinder_OnScanFinished(pack->status);
	}
	free(pack->path);
	free(pack);
}

static void LCFinder_ScanDir(FileSyncStatus s, const wchar_t *path)
{
	FileSyncDataPack pack;
	size_t len = wcslen(path);
	pack = NEW(FileSyncDataPackRec, 1);
	pack->path = malloc(sizeof(wchar_t) * (len + 1));
	wcsncpy(pack->path, path, len);
	if (path[len - 1] == PATH_SEP) {
		len -= 1;
	}
	pack->status = s;
	pack->path[len] = 0;
	pack->path_len = len;
	pack->status->dirs += 1;
	FileStorage_GetFile(finder.storage_for_scan, path, LCFinder_OnScanDir,
			    pack);
}

static void LCFinder_SwitchTask(FileSyncStatus s)
{
	DB_Dir dir;
	wchar_t *path;
	s->task = s->tasks[s->task_i];
	if (s->task) {
		SyncTask_Start(s->task);
		dir = finder.dirs[s->task_i];
		path = DecodeUTF8(dir->path);
		LCFinder_ScanDir(s, path);
		free(path);
	} else {
		LCFinder_OnScanFinished(s);
	}
}

void LCFinder_SyncFilesAsync(FileSyncStatus s)
{
	size_t i;
	wchar_t path[PATH_LEN];
	path[PATH_LEN - 1] = 0;
	s->task_i = 0;
	s->task = NULL;
	s->tasks = NULL;
	s->files = 0;
	s->dirs = 0;
	s->added_files = 0;
	s->synced_files = 0;
	s->scaned_files = 0;
	s->scaned_dirs = 0;
	s->deleted_files = 0;
	s->state = STATE_STARTED;
	if (finder.n_dirs < 1) {
		s->tasks = NULL;
		LCFinder_OnScanFinished(s);
		return;
	}
	s->tasks = NEW(SyncTask, finder.n_dirs);
	for (i = 0; i < finder.n_dirs; ++i) {
		DB_Dir dir = finder.dirs[i];
		if (!dir) {
			continue;
		}
		LCUI_DecodeString(path, dir->path, PATH_LEN - 1, ENCODING_UTF8);
		s->tasks[i] = SyncTask_NewW(finder.fileset_dir, path);
	}
	LCFinder_SwitchTask(s);
}

/** 初始化工作目录 */
static int LCFinder_InitWorkDir(void)
{
	size_t len, tdir_len, fdir_len;
	wchar_t data_dir[PATH_LEN];
	wchar_t *dirs[2] = { L"fileset", L"thumbs" };
	if (GetAppInstalledLocationW(data_dir, PATH_LEN) != 0) {
		LOG("[workdir] cannot get app installed location\n");
		return -1;
	}
	len = wcslen(data_dir) + 1;
	finder.work_dir = NEW(wchar_t, len);
	wcsncpy(finder.work_dir, data_dir, len);
	wchdir(finder.work_dir);
	if (GetAppDataFolderW(data_dir, PATH_LEN) != 0) {
		LOG("[workdir] cannot get app data folder path\n");
		return -1;
	}
	len = wcslen(data_dir) + 2;
	tdir_len = len + wcslen(dirs[0]);
	fdir_len = len + wcslen(dirs[1]);
	finder.data_dir = NEW(wchar_t, len);
	finder.fileset_dir = NEW(wchar_t, tdir_len);
	finder.thumbs_dir = NEW(wchar_t, fdir_len);
	wcsncpy(finder.data_dir, data_dir, len);
	wpathjoin(finder.fileset_dir, data_dir, dirs[0]);
	wpathjoin(finder.thumbs_dir, data_dir, dirs[1]);
	wmkdir(finder.fileset_dir);
	wmkdir(finder.thumbs_dir);
	LOG("[workdir] work directory: %S\n", finder.work_dir);
	LOG("[workdir] data directory: %S\n", finder.data_dir);
	return 0;
}

DB_Tag LCFinder_GetTag(const char *tagname)
{
	size_t i;
	for (i = 0; i < finder.n_tags; ++i) {
		if (strcmp(finder.tags[i]->name, tagname) == 0) {
			return finder.tags[i];
		}
	}
	return NULL;
}

DB_Tag LCFinder_AddTag(const char *tagname)
{
	DB_Tag *tags;
	DB_Tag tag = DB_AddTag(tagname);
	if (!tag) {
		return NULL;
	}
	finder.n_tags += 1;
	tags = realloc(finder.tags, sizeof(DB_Tag) * (finder.n_tags + 1));
	if (!tags) {
		finder.n_tags -= 1;
		return NULL;
	}
	tags[finder.n_tags - 1] = tag;
	tags[finder.n_tags] = NULL;
	finder.tags = tags;
	LCFinder_TriggerEvent(EVENT_TAG_ADD, tag);
	return tag;
}

DB_Tag LCFinder_AddTagForFile(DB_File file, const char *tagname)
{
	DB_Tag tag = LCFinder_GetTag(tagname);
	if (!tag) {
		tag = LCFinder_AddTag(tagname);
	}
	if (tag) {
		tag->count += 1;
		DBFile_AddTag(file, tag);
		LCFinder_TriggerEvent(EVENT_TAG_UPDATE, tag);
	}
	return tag;
}

size_t LCFinder_GetFileTags(DB_File file, DB_Tag **outtags)
{
	size_t i, j, count, n;
	DB_Tag *tags, *newtags;
	n = DBFile_GetTags(file, &tags);
	newtags = malloc(sizeof(DB_Tag) * (n + 1));
	for (count = 0, i = 0; i < n; ++i) {
		for (j = 0; j < finder.n_tags; ++j) {
			if (tags[i]->id == finder.tags[j]->id) {
				newtags[count] = finder.tags[j];
				++count;
				break;
			}
		}
		free(tags[i]);
	}
	*outtags = newtags;
	free(tags);
	return count;
}

void LCFinder_ReloadTags(void)
{
	size_t i;
	for (i = 0; i < finder.n_tags; ++i) {
		free(finder.tags[i]->name);
		finder.tags[i]->name = NULL;
	}
	free(finder.tags);
	finder.n_tags = DB_GetTags(&finder.tags);
}

/** 初始化文件数据库 */
static int LCFinder_InitFileDB(void)
{
	char *path;
	wchar_t wpath[PATH_LEN];
	wpathjoin(wpath, finder.data_dir, STORAGE_FILE);
	LOGW(L"[filedb] path: %s\n", wpath);
	path = EncodeUTF8(wpath);
	ASSERT(DB_Init(path) == 0);
	finder.n_dirs = DB_GetDirs(&finder.dirs);
	finder.n_tags = DB_GetTags(&finder.tags);
	free(path);
	return 0;

error:
	free(path);
	return 1;
}

static void LCFinder_ExitFileDB(void)
{
	size_t i;
	for (i = 0; i < finder.n_dirs; ++i) {
		if (finder.dirs[i]) {
			DBDir_Release(finder.dirs[i]);
		}
		finder.dirs[i] = NULL;
	}
	for (i = 0; i < finder.n_tags; ++i) {
		DBTag_Release(finder.tags[i]);
		finder.tags[i] = NULL;
	}
	DB_Exit();
}

static int LCFinder_InitThumbCache(void)
{
	finder.thumb_cache = ThumbCache_New(THUMB_CACHE_SIZE);
	if (!finder.thumb_cache) {
		return -1;
	}
	return 0;
}

/** 初始化语言文件列表 */
static int LCFinder_InitLanguage(void)
{
	int dir_len;
	char file[PATH_LEN];
	wchar_t *wfile, lang_path[PATH_LEN];
	LCUI_DirEntry *entry;
	LCUI_Dir dir;

	dir_len = wpathjoin(lang_path, finder.work_dir, LANG_DIR);
	LOG("[i18n] open directory: %S\n", lang_path);
	if (LCUI_OpenDir(lang_path, &dir) != 0) {
		LOG("[i18n] open directory failed\n");
		return -1;
	}
	lang_path[dir_len++] = PATH_SEP;
	wfile = lang_path + dir_len;
	while ((entry = LCUI_ReadDir(&dir))) {
		int len;
		wchar_t *name, *p;
		name = LCUI_GetFileNameW(entry);
		/* 忽略 . 开头的文件，包括 . 和 .. 文件夹 */
		if (name[0] == '.') {
			continue;
		}
		if (!LCUI_FileIsRegular(entry)) {
			continue;
		}
		p = wcsstr(name, LANG_FILE_EXT);
		if (!p) {
			continue;
		}
		p += sizeof(LANG_FILE_EXT) / sizeof(wchar_t) - 1;
		if (*p) {
			continue;
		}
		wcscpy(wfile, name);
		len = LCUI_EncodeString(file, lang_path, PATH_LEN - 1,
					ENCODING_ANSI);
		file[len] = 0;
		LOG("[i18n] load file: %S\n", lang_path);
		I18n_LoadLanguage(file);
	}
	LOG("[i18n] set language: %s\n", finder.config.language);
	if (I18n_SetLanguage(finder.config.language)) {
		return 0;
	}
	I18n_GetDefaultLanguage(finder.config.language, 32);
	LCFinder_SaveConfig();
	LOG("[i18n] set language: %s\n", finder.config.language);
	if (I18n_SetLanguage(finder.config.language)) {
		return 0;
	}
	return -1;
}

static void ThumbDBDict_ValDel(void *privdata, void *val)
{
	ThumbDB_Close(val);
}

/** 初始化缩略图数据库 */
static int LCFinder_InitThumbDB(void)
{
	size_t i;
	wchar_t *path;
	LOG("[thumbdb] init ...\n");
	finder.thumb_dbs = StrDict_Create(NULL, ThumbDBDict_ValDel);
	finder.thumb_paths = malloc(sizeof(wchar_t *) * finder.n_dirs);
	if (!finder.thumb_dbs || !finder.thumb_paths) {
		return -ENOMEM;
	}
	for (i = 0; i < finder.n_dirs; ++i) {
		if (!finder.dirs[i]) {
			continue;
		}
		path = LCFinder_CreateThumbDB(finder.dirs[i]->path);
		finder.thumb_paths[i] = path;
		LOGW(L"[thumbdb] %d: %s\n", i, path);
	}
	LOG("[thumbdb] init done\n");
	return 0;
}

/** 退出缩略图数据库 */
static void LCFinder_ExitThumbDB(void)
{
	size_t i;
	if (!finder.thumb_dbs) {
		return;
	}
	LOG("[thumbdb] exit ..\n");
	Dict_Release(finder.thumb_dbs);
	for (i = 0; i < finder.n_dirs; ++i) {
		free(finder.thumb_paths[i]);
		finder.thumb_paths[i] = NULL;
	}
	free(finder.thumb_paths);
	finder.thumb_paths = NULL;
	finder.thumb_dbs = NULL;
	LOG("[thumbdb] exit done\n");
}

/** 清除缩略图数据库 */
void LCFinder_ClearThumbDB(void)
{
	size_t i;
	wchar_t *path;
	Dict_Release(finder.thumb_dbs);
	for (i = 0; i < finder.n_dirs; ++i) {
		path = finder.thumb_paths[i];
		finder.thumb_paths[i] = NULL;
#ifdef _WIN32
		_wremove(path);
#endif
		free(path);
	}
	free(finder.thumb_paths);
	finder.thumb_paths = NULL;
	finder.thumb_dbs = NULL;
	LCFinder_InitThumbDB();
	LCFinder_TriggerEvent(EVENT_THUMBDB_DEL_DONE, NULL);
}

static int LCFinder_InitFileStorage(void)
{
	FileStorage_Init();
	finder.storage = FileStorage_Connect();
	finder.storage_for_image = FileStorage_Connect();
	finder.storage_for_thumb = FileStorage_Connect();
	finder.storage_for_scan = FileStorage_Connect();
	ASSERT(finder.storage > 0);
	ASSERT(finder.storage_for_image > 0);
	ASSERT(finder.storage_for_thumb > 0);
	ASSERT(finder.storage_for_scan > 0);
	return 0;

error:
	return -1;
}

static void LCFinder_ExitFileStorage(void)
{
	FileStorage_Close(finder.storage);
	FileStorage_Close(finder.storage_for_image);
	FileStorage_Close(finder.storage_for_thumb);
	FileStorage_Close(finder.storage_for_scan);
	FileStorage_Exit();
}

int LCFinder_SaveConfig(void)
{
	FILE *file;
	char *path;
	wchar_t wpath[PATH_LEN];
	wpathjoin(wpath, finder.data_dir, CONFIG_FILE);
	path = EncodeANSI(wpath);
	file = fopen(path, "wb");
	if (!file) {
		LOG("[config] cannot open file: %s\n", path);
		return -1;
	}
	fwrite(&finder.config, sizeof(finder.config), 1, file);
	fclose(file);
	free(path);
	return 0;
}

LCUI_BOOL LCFinder_AuthPassword(const char *password)
{
	char pwd[48];
	EncodeSHA1(pwd, password, strlen(password));
	if (strcmp(pwd, finder.config.encrypted_password) == 0) {
		return TRUE;
	}
	return FALSE;
}

void LCFinder_SetPassword(const char *password)
{
	EncodeSHA1(finder.config.encrypted_password, password,
		   strlen(password));
}

void LCFinder_OpenPrivateSpace(void)
{
	finder.open_private_space = TRUE;
	LCFinder_TriggerEvent(EVENT_DIRS_CHG, NULL);
}

void LCFinder_ClosePrivateSpace(void)
{
	finder.open_private_space = FALSE;
	LCFinder_TriggerEvent(EVENT_DIRS_CHG, NULL);
}

int LCFinder_LoadConfig(void)
{
	FILE *file;
	char *path;
	FinderConfigRec config;
	wchar_t wpath[PATH_LEN];
	LCUI_BOOL has_error = TRUE;
	I18n_GetDefaultLanguage(finder.config.language, 32);
	strcpy(finder.config.head, LCFINDER_CONFIG_HEAD);
	finder.config.encrypted_password[0] = 0;
	finder.config.version.type = LCFINDER_VER_TYPE;
	finder.config.version.major = LCFINDER_VER_MAJOR;
	finder.config.version.minor = LCFINDER_VER_MINOR;
	finder.config.version.revision = LCFINDER_VER_REVISION;
	wpathjoin(wpath, finder.data_dir, CONFIG_FILE);
	path = EncodeANSI(wpath);
	file = fopen(path, "rb");
	if (file) {
		if (fread(&config, sizeof(config), 1, file) == 1 &&
		    strcmp(finder.config.head, config.head) == 0) {
			has_error = FALSE;
			finder.config = config;
		}
		fclose(file);
	}
	if (has_error) {
		LCFinder_SaveConfig();
	}
	finder.open_private_space = FALSE;
	free(path);
	return 0;
}

static void LCFinder_InitEvent(void)
{
	finder.trigger = EventTrigger();
}

static void LCFinder_OnExit(LCUI_SysEvent e, void *arg)
{
	LCFinder_Exit();
}

#ifdef PLATFORM_WIN32
static void LoggerHandler(const char *str)
{
	OutputDebugStringA(str);
}

static void LoggerHandlerW(const wchar_t *str)
{
	OutputDebugStringW(str);
}
#endif

int LCFinder_Init(int argc, char **argv)
{
#ifdef PLATFORM_WIN32
	_wsetlocale(LC_ALL, L"chs");
	Logger_SetHandler(LoggerHandler);
	Logger_SetHandlerW(LoggerHandlerW);
#endif
	LCFinder_InitEvent();
	LCFinder_InitLicense();
	ASSERT(LCFinder_InitWorkDir() == 0);
	ASSERT(LCFinder_LoadConfig() == 0);
	ASSERT(LCFinder_InitLanguage() == 0);
	ASSERT(LCFinder_InitFileDB() == 0);
	ASSERT(LCFinder_InitThumbDB() == 0);
	ASSERT(LCFinder_InitThumbCache() == 0);
	ASSERT(LCFinder_InitFileStorage() == 0);
	ASSERT(UI_Init(argc, argv) == 0);
	LCUI_BindEvent(LCUI_QUIT, LCFinder_OnExit, NULL, NULL);
	finder.state = FINDER_STATE_ACTIVATED;
	return 0;

error:
	finder.state = FINDER_STATE_BLOCKED;
	return -1;
}

void LCFinder_Exit(void)
{
	UI_Exit();
	LCFinder_ExitThumbDB();
	LCFinder_ExitFileStorage();
	LCFinder_ExitFileDB();
}

int LCFinder_Run(void)
{
	if (finder.state != FINDER_STATE_ACTIVATED) {
		LCFinder_Exit();
		return -1;
	}
	return UI_Run();
}

#ifndef PLATFORM_WIN32_PC_APP
int main(int argc, char **argv)
{
	LCFinder_Init(argc, argv);
	return LCFinder_Run();
}
#endif
