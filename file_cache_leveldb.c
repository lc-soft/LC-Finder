/* ***************************************************************************
 * file_cache_leveldb.c -- file list cache, Based on LevelDB
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
 * file_cache_leveldb.c -- 文件列表缓存，基于 LevelDB 实现
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

#include "build.h"

#ifdef LCFINDER_USE_LEVELDB
#define HAVE_FILECACHE
#include "file_cache.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <leveldb/c.h>

typedef struct FileCacheRec_ {
	leveldb_t *db;
	leveldb_options_t *options;
	leveldb_readoptions_t *roptions;
	leveldb_writeoptions_t *woptions;
} FileCacheRec;

FileCache FileCache_Open(const char *file)
{
	char *err = NULL;
	FileCache cache = malloc(sizeof(FileCacheRec));

	cache->options = leveldb_options_create();
	cache->woptions = leveldb_writeoptions_create();
	leveldb_options_set_create_if_missing(cache->options, 1);
	leveldb_writeoptions_set_sync(cache->woptions, 1);
	cache->db = leveldb_open(cache->options, file, &err);
	if (err) {
		printf("[filecache] error: %s\n", err);
		return NULL;
	}
	return cache;
}

void FileCache_Close(FileCache cache)
{
	leveldb_readoptions_destroy(cache->roptions);
	leveldb_writeoptions_destroy(cache->woptions);
	leveldb_options_destroy(cache->options);
	leveldb_close(cache->db);
	free(cache);
}

size_t FileCache_ReadAll(FileCache cache, FileInfoHanlder handler, void *data)
{
	size_t count = 0;
	size_t key_size, data_size;
	const wchar_t *key;

	leveldb_iterator_t *iter;

	FileCacheInfo info;
	FileCacheTime value;

	iter = leveldb_create_iterator(cache->db, cache->roptions);
	leveldb_iter_seek_to_first(iter);

	while (leveldb_iter_valid(iter)) {
		key = (wchar_t*)leveldb_iter_key(iter, &key_size);
		value = (FileCacheTime)leveldb_iter_value(iter, &data_size);

		info = malloc(sizeof(FileCacheInfoRec));
		info->path = malloc(key_size);
		info->mtime = value->mtime;
		info->ctime = value->ctime;

		key_size /= sizeof(wchar_t);
		wcsncpy(info->path, key, key_size);
		info->path[key_size] = 0;

		handler(data, info);

		leveldb_iter_next(iter);
		++count;
	}
	leveldb_iter_destroy(iter);
	return count;
}

int FileCache_Put(FileCache cache, const wchar_t *path, FileCacheTime time)
{
	char *err = NULL;
	leveldb_put(cache->db, cache->woptions, (const char*)path,
		    wcslen(path) * sizeof(wchar_t), (const char*)time,
		    sizeof(FileCacheTimeRec), &err);
	if (err) {
		printf("[filecache] error: %s\n", err);
		return -1;
	}
	return 0;
}

int FileCache_Delete(FileCache cache, const wchar_t *path)
{
	char *err = NULL;
	leveldb_delete(cache->db, cache->woptions, (const char*)path,
		       wcslen(path) * sizeof(wchar_t), &err);
	if (err) {
		printf("[filecache] error: %s\n", err);
		return -1;
	}
	return 0;
}

#endif
