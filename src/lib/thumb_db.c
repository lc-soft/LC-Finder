/* ***************************************************************************
 * thumb_db.c -- thumbnail database
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
 * thumb_db.c -- 缩略图缓存数据库
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

#define HAVE_THUMB_DB
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/graph.h>
#include <LCUI/thread.h>
#include "kvdb.h"
#include "thumb_db.h"

#define THUMB_MAX_SIZE 8553600
#define ThumbDB_Unlock(TDB) LCUIMutex_Unlock( &(TDB)->mutex )
#define ASSERT(X) if(!(X)) { return -1; }

typedef struct ThumbDBRec_ {
	kvdb_t *db;
	LCUI_BOOL closed;
	LCUI_Mutex mutex;
} ThumbDBRec;

typedef struct ThumbDataBlockRec_ {
	uint32_t width;
	uint32_t height;
	uint32_t origin_width;
	uint32_t origin_height;
	uint32_t mem_size;
	int color_type;
	uint32_t modify_time;
} ThumbDataBlockRec, *ThumbDataBlock;

ThumbDB ThumbDB_Open(const char *filepath)
{
	ThumbDB tdb;
	
	tdb = malloc(sizeof(ThumbDBRec));
	tdb->db = kvdb_open(filepath);
	if (!tdb->db) {
		printf("[thumbdb] cannot open db: %s\n", filepath);
		free(tdb);
		return NULL;
	}
	tdb->closed = FALSE;
	LCUIMutex_Init(&tdb->mutex);
	return tdb;
}

void ThumbDB_Close(ThumbDB tdb)
{
	tdb->closed = TRUE;
	LCUIMutex_Lock(&tdb->mutex);
	kvdb_close(tdb->db);
	LCUIMutex_Unlock(&tdb->mutex);
	LCUIMutex_Destroy(&tdb->mutex);
	free(tdb);
}

int ThumbDB_GetSize(const char *filepath, int64_t *size)
{
	return kvdb_get_db_size(filepath, size);
}

int ThumbDB_DestroyDB(const char *filepath)
{
	return kvdb_destroy_db(filepath);
}

static int ThumbDB_Lock(ThumbDB tdb)
{
	if (tdb->closed) {
		return -1;
	}
	LCUIMutex_Lock(&tdb->mutex);
	if (tdb->closed) {
		return -1;
	}
	return 0;
}

int ThumbDB_Load(ThumbDB tdb, const char *filepath, ThumbData data)
{
	size_t size;
	size_t keylen;
	uchar_t *bytes;
	ThumbDataBlock block;

	ASSERT(ThumbDB_Lock(tdb) == 0);
	keylen = strlen(filepath);
	block = kvdb_get(tdb->db, filepath, keylen, &size);
	if (!block) {
		ThumbDB_Unlock(tdb);
		return -1;
	}
	bytes = (uchar_t*)block + sizeof(ThumbDataBlockRec);
	ThumbDB_Unlock(tdb);
	Graph_Init(&data->graph);
	data->graph.color_type = block->color_type;
	Graph_Create(&data->graph, block->width, block->height);
	memcpy(data->graph.bytes, bytes, block->mem_size);
	data->modify_time = block->modify_time;
	data->origin_width = block->origin_width;
	data->origin_height = block->origin_height;
	free(block);
	return 0;
}

int ThumbDB_Save(ThumbDB tdb, const char *filepath, ThumbData data)
{
	int rc;
	uchar_t *buff;
	ThumbDataBlock block;
	size_t head_size = sizeof(ThumbDataBlockRec);
	size_t size = head_size + data->graph.mem_size;
	if (size > THUMB_MAX_SIZE) {
		return -1;
	}
	ASSERT(ThumbDB_Lock(tdb) == 0);
	block = malloc(size);
	buff = (uchar_t*)block + head_size;
	block->width = data->graph.width;
	block->height = data->graph.height;
	block->mem_size = data->graph.mem_size;
	block->modify_time = data->modify_time;
	block->origin_width = data->origin_width;
	block->origin_height = data->origin_height;
	block->color_type = data->graph.color_type;
	memcpy(buff, data->graph.bytes, data->graph.mem_size);
	rc = kvdb_put(tdb->db, filepath, strlen(filepath), (char*)block, size);
	ThumbDB_Unlock(tdb);
	free(block);
	return rc == 0 ? 0 : -2;
}
