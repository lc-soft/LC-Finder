/* ***************************************************************************
 * file_cache.c -- file list cache, it used for file list changes detection.
 *
 * Copyright (C) 2015-2018 by Liu Chao <lc-soft@live.cn>
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
 * file_cache.c -- 文件列表的缓存，方便检测文件列表的增删状态。
 *
 * 版权所有 (C) 2015-2018 归属于 刘超 <lc-soft@live.cn>
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

#define DEBUG
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/font/charset.h>

#include "kvdb.h"
#include "common.h"
#include "file_cache.h"

#define MAX_PATH_LEN	2048
#define WCSLEN(STR)	(sizeof( STR ) / sizeof( wchar_t ))
#define GetDirStats(T)	(DirStats)(((char*)(T)) + sizeof(SyncTaskRec))

typedef kvdb_t* FileCache;

typedef struct FileInfoHanlderPackRec_ {
	void *data;
	FileInfoHanlder handler;
} FileInfoHanlderPackRec, *FileInfoHanlderPack;

/** 文件夹内的文件变更状态统计 */
typedef struct DirStatsRec_ {
	FileCache db;
	Dict *files;		/**< 之前已缓存的文件列表 */
	Dict *added_files;	/**< 新增的文件 */
	Dict *changed_files;	/**< 已改变的文件 */
	Dict *deleted_files;	/**< 删除的文件 */
} DirStatsRec, *DirStats;

static unsigned int Dict_KeyHash(const void *key)
{
	unsigned int hash = 5381;
	const wchar_t *buf = key;

	while (*buf) {
		hash = ((hash << 5) + hash) + (*buf++);
	}
	return hash;
}

static int Dict_KeyCompare(void *privdata, const void *key1, const void *key2)
{
	if (wcscmp(key1, key2) == 0) {
		return 1;
	}
	return 0;
}

static void *Dict_KeyDup(void *privdata, const void *key)
{
	size_t len;
	wchar_t *newkey;
	len = wcslen(key) + 1;
	newkey = malloc(len * sizeof(wchar_t));
	if (!newkey) {
		return NULL;
	}
	wcsncpy(newkey, key, len);
	return newkey;
}

static void Dict_KeyDestructor(void *privdata, void *key)
{
	free(key);
}

static void FilesDict_ValDestructor(void *privdata, void *val)
{
	free(val);
}

static DictType FilePathsDict = {
	Dict_KeyHash,
	Dict_KeyDup,
	NULL,
	Dict_KeyCompare,
	Dict_KeyDestructor,
	NULL
};

static DictType FilesDict = {
	Dict_KeyHash,
	Dict_KeyDup,
	NULL,
	Dict_KeyCompare,
	Dict_KeyDestructor,
	FilesDict_ValDestructor
};

static void FileCache_OnFetch(const char *key, size_t keylen,
			      const void *val, size_t vallen, void *data)
{
	FileCacheInfo info;
	FileCacheTime value = (FileCacheTime)val;
	FileInfoHanlderPack pack = data;

	info = malloc(sizeof(FileCacheInfoRec));
	info->path = malloc(keylen + sizeof(wchar_t));
	info->mtime = value->mtime;
	info->ctime = value->ctime;

	keylen /= sizeof(wchar_t);
	wcsncpy(info->path, (const wchar_t*)key, keylen);
	info->path[keylen] = 0;

	pack->handler(pack->data, info);
}

size_t FileCache_ReadAll(FileCache cache, FileInfoHanlder handler, void *data)
{
	FileInfoHanlderPackRec pack = { data, handler };
	return kvdb_each(cache, FileCache_OnFetch, &pack);
}

int FileCache_Put(FileCache cache, const wchar_t *path, FileCacheTime time)
{
	return kvdb_put(cache, (const char*)path,
			wcslen(path) * sizeof(wchar_t), (char*)time,
			sizeof(FileCacheTimeRec));
}

int FileCache_Delete(FileCache cache, const wchar_t *path)
{
	return kvdb_delete(cache, (const char*)path,
			   wcslen(path) * sizeof(wchar_t));
}

SyncTask SyncTask_New(const char *data_dir, const char *scan_dir)
{
	SyncTask t;
	wchar_t *wcs_data_dir, *wcs_scan_dir;

	wcs_data_dir = DecodeUTF8(data_dir);
	wcs_scan_dir = DecodeUTF8(scan_dir);
	t = SyncTask_NewW(wcs_data_dir, wcs_scan_dir);
	free(wcs_data_dir);
	free(wcs_scan_dir);
	return t;
}

SyncTask SyncTask_NewW(const wchar_t *data_dir, const wchar_t *scan_dir)
{
	SyncTask t;
	DirStats ds;
	wchar_t name[44];
	size_t max_len, len1, len2;
	const wchar_t suffix[] = L".tmp";

	t = malloc(sizeof(SyncTaskRec) + sizeof(DirStatsRec));
	ds = GetDirStats(t);
	len1 = wcslen(data_dir) + 1;
	len2 = wcslen(scan_dir) + 1;
	ds->files = Dict_Create(&FilesDict, NULL);
	ds->added_files = Dict_Create(&FilesDict, NULL);
	ds->changed_files = Dict_Create(&FilePathsDict, NULL);
	ds->deleted_files = Dict_Create(&FilePathsDict, NULL);
	t->data_dir = malloc(sizeof(wchar_t) * len1);
	t->scan_dir = malloc(sizeof(wchar_t) * len2);
	wcsncpy(t->data_dir, data_dir, len1);
	wcsncpy(t->scan_dir, scan_dir, len2);
	WEncodeSHA1(name, t->scan_dir, len2);
	max_len = len1 + WCSLEN(name) + WCSLEN(suffix) + 1;
	t->tmpfile = malloc(max_len * sizeof(wchar_t));
	t->file = malloc(max_len * sizeof(wchar_t));
	wcsncpy(t->tmpfile, t->data_dir, len1);
	wpathjoin(t->file, data_dir, name);
	swprintf(t->tmpfile, max_len, L"%ls%ls", t->file, suffix);
	t->state = STATE_NONE;
	t->changed_files = 0;
	t->deleted_files = 0;
	t->total_files = 0;
	t->added_files = 0;
	return t;
}

void SyncTask_ClearCache(SyncTask t)
{
	char *file = EncodeANSI(t->file);
	char *tmpfile = EncodeANSI(t->tmpfile);
	kvdb_destroy_db(file);
	kvdb_destroy_db(tmpfile);
	free(file);
	free(tmpfile);
}

void SyncTask_Delete(SyncTask t)
{
	DirStats ds = GetDirStats(t);
	free(t->scan_dir);
	free(t->data_dir);
	free(t->file);
	free(t->tmpfile);
	t->file = NULL;
	t->tmpfile = NULL;
	t->scan_dir = NULL;
	t->data_dir = NULL;
	Dict_Release(ds->files);
	Dict_Release(ds->added_files);
	Dict_Release(ds->changed_files);
	Dict_Release(ds->deleted_files);
	free(t);
}

static int FileDict_ForEach(Dict *d, FileInfoHanlder func, void *data)
{
	int count = 0;
	DictEntry *entry;
	DictIterator *iter = Dict_GetIterator(d);
	while ((entry = Dict_Next(iter))) {
		func(data, DictEntry_GetVal(entry));
		++count;
	}
	Dict_ReleaseIterator(iter);
	return count;
}

int SyncTask_InAddedFiles(SyncTask t, FileInfoHanlder func, void *func_data)
{
	DirStats ds = GetDirStats(t);
	return FileDict_ForEach(ds->added_files, func, func_data);
}

int SyncTask_InChangedFiles(SyncTask t, FileInfoHanlder func, void *func_data)
{
	DirStats ds = GetDirStats(t);
	return FileDict_ForEach(ds->changed_files, func, func_data);
}

int SyncTask_InDeletedFiles(SyncTask t, FileInfoHanlder func, void *func_data)
{
	DirStats ds = GetDirStats(t);
	return FileDict_ForEach(ds->deleted_files, func, func_data);
}

int SyncTask_OpenCacheW(SyncTask t, const wchar_t *path)
{
	DirStats ds;
	char *dbfile;

	ds = GetDirStats(t);
	path = path ? path : t->file;
	dbfile = EncodeANSI(path);
	ds->db = kvdb_open(dbfile);
	free(dbfile);
	if (!ds->db) {
		return -1;
	}
	return 0;
}

void SyncTask_CloseCache(SyncTask t)
{
	DirStats ds = GetDirStats(t);
	kvdb_close(ds->db);
}

static void SyncTask_OnLoadFile(void *data, const FileCacheInfo info)
{
	DirStats ds = data;
	Dict_Add(ds->files, info->path, info);
	Dict_Add(ds->deleted_files, info->path, info);
}

/** 从缓存记录中载入文件列表 */
static size_t SyncTask_LoadCache(SyncTask t)
{
	size_t count;
	DirStats ds;

	ds = GetDirStats(t);
	SyncTask_OpenCacheW(t, t->file);
	count = FileCache_ReadAll(ds->db, SyncTask_OnLoadFile, ds);
	t->deleted_files = count;
	SyncTask_CloseCache(t);
	return count;
}

int SyncTask_AddFileW(SyncTask t, const wchar_t *path,
		      unsigned int ctime, unsigned int mtime)
{
	size_t len;
	FileCacheInfo info;
	FileCacheTimeRec time;
	DirStats ds = GetDirStats(t);

	if (t->state != STATE_STARTED) {
		return -1;
	}
	len = wcslen(path);
	/* 若该文件路径存在于之前的缓存中，说明未被删除，否则将之
	 * 视为新增的文件。
	 */
	info = Dict_FetchValue(ds->files, path);
	if (info) {
		if (ctime != info->ctime || mtime != info->mtime) {
			info->ctime = ctime;
			info->mtime = mtime;
			Dict_Add(ds->changed_files, info->path, info);
			DEBUG_MSG("changed file: %ls\n", path);
			++t->changed_files;
		} else {
			DEBUG_MSG("unchanged file: %ls\n", path);
		}
		Dict_Delete(ds->deleted_files, path);
		--t->deleted_files;
	} else {
		info = NEW(FileCacheInfoRec, 1);
		info->path = NEW(wchar_t, len + 1);
		wcsncpy(info->path, path, len + 1);
		info->ctime = ctime;
		info->mtime = mtime;
		Dict_Add(ds->added_files, info->path, info);
		DEBUG_MSG("added file: %ls\n", path);
		++t->added_files;
	}
	time.ctime = info->ctime;
	time.mtime = info->mtime;
	FileCache_Put(ds->db, path, &time);
	++t->total_files;
	return 0;
}

int SyncTask_DeleteFileW(SyncTask t, const wchar_t *filepath)
{
	DirStats ds = GetDirStats(t);
	return FileCache_Delete(ds->db, filepath);
}

int SyncTask_Start(SyncTask t)
{
	SyncTask_LoadCache(t);
	if (0 != SyncTask_OpenCacheW(t, t->tmpfile)) {
		return -1;
	}
	t->state = STATE_STARTED;
	return 0;
}

void SyncTask_Finish(SyncTask t)
{
	t->state = STATE_FINISHED;
	SyncTask_CloseCache(t);
}

int SyncTask_Commit(SyncTask t)
{
	int ret;
	char *file = EncodeANSI(t->file);
	char *tmpfile = EncodeANSI(t->tmpfile);

	kvdb_destroy_db(file);
	ret = rename(tmpfile, file);
	if (ret != 0) {
		_DEBUG_MSG("%s\n", strerror(errno));
	}
	free(file);
	free(tmpfile);
	return ret;
}
