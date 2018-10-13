/* ***************************************************************************
 * file_cache_unqlite.c -- file list cache, Based on UnQLite
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
 * file_cache_unqlite.c -- 文件列表缓存，基于 UnQLite 实现
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

#ifdef LCFINDER_USE_UNQLITE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "file_cache.h"
#include "unqlite.h"

#define MAX_PATH_LEN 2048

FileCache FileCache_Open(const char *file)
{
	int rc;
	unqlite *db;

	rc = unqlite_open(&db, file, UNQLITE_OPEN_CREATE);
	if (rc != UNQLITE_OK) {
		return NULL;
	}

	return db;
}

void FileCache_Close(FileCache db)
{
	unqlite_close(db);
}

size_t FileCache_ReadAll(FileCache db, FileInfoHanlder handler, void *data)
{
	int rc;
	int key_size;
	size_t count;
	wchar_t buf[MAX_PATH_LEN];

	unqlite_kv_cursor *cur;
	unqlite_int64 data_size = sizeof(FileCacheTimeRec);

	FileCacheInfo info;
	FileCacheTimeRec ft;

	rc = unqlite_kv_cursor_init(db, &cur);
	if (rc != UNQLITE_OK) {
		unqlite_close(db);
		return 0;
	}
	rc = unqlite_kv_cursor_first_entry(cur);
	if (rc != UNQLITE_OK) {
		unqlite_kv_cursor_release(db, cur);
		unqlite_close(db);
		return 0;
	}
	count = 0;
	while (unqlite_kv_cursor_valid_entry(cur)) {
		key_size = MAX_PATH_LEN;

		unqlite_kv_cursor_key(cur, buf, &key_size);
		unqlite_kv_cursor_data(cur, &ft, &data_size);
		unqlite_kv_cursor_next_entry(cur);

		info = malloc(sizeof(FileCacheInfoRec));
		info->path = malloc(key_size + sizeof(wchar_t));
		info->mtime = ft.mtime;
		info->ctime = ft.ctime;

		key_size /= sizeof(wchar_t);
		buf[(int)key_size] = 0;
		wcsncpy(info->path, buf, key_size + 1);

		handler(data, info);

		++count;
	}
	unqlite_kv_cursor_release(db, cur);
	return count;
}

int FileCache_Put(FileCache db, const wchar_t *path, FileCacheTime time)
{
	return unqlite_kv_store(db, path, wcslen(path) * sizeof(wchar_t),
				time, sizeof(FileCacheTimeRec));
}

int FileCache_Delete(FileCache db, const wchar_t *path)
{
	size_t size = sizeof(wchar_t) * wcslen(path);
	if (unqlite_kv_delete(db, path, size) == UNQLITE_OK) {
		return 0;
	}
	return -1;
}

#endif
