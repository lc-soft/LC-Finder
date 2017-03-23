/* ***************************************************************************
 * thumb_db.c -- thumbnail database
 *
 * Copyright (C) 2016 by Liu Chao <lc-soft@live.cn>
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
 * 版权所有 (C) 2016 归属于 刘超 <lc-soft@live.cn>
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

#define LCFINDER_THUMB_DB_C
#include <stdio.h>
#include <stdint.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/graph.h>
#include <LCUI/thread.h>
#include "unqlite.h"
#include "thumb_db.h"

typedef struct ThumbDBRec_ {
	unqlite *db;
	LCUI_BOOL closed;
	LCUI_Mutex mutex;
} ThumbDBRec;

#define THUMB_MAX_SIZE 8553600

typedef struct ThumbDataBlockRec_ {
	size_t width;
	size_t height;
	size_t origin_width;
	size_t origin_height;
	size_t mem_size;
	int color_type;
	unsigned int modify_time;
} ThumbDataBlockRec, *ThumbDataBlock;

ThumbDB ThumbDB_Open( const char *filepath )
{
	int rc;
	ThumbDB tdb = malloc( sizeof(ThumbDBRec) );
	rc = unqlite_open( &tdb->db, filepath, UNQLITE_OPEN_CREATE );
	if( rc != UNQLITE_OK ) {
		free( tdb );
		return NULL;
	}
	rc = unqlite_kv_store( tdb->db, "__program__", -1, "LCFinder", 9 );
	if( rc != UNQLITE_OK ) {
		printf( "[thumbdb] cannot open db: %s\n", filepath );
	}
	tdb->closed = FALSE;
	LCUIMutex_Init( &tdb->mutex );
	return tdb;
}

void ThumbDB_Close( ThumbDB tdb )
{
	tdb->closed = TRUE;
	LCUIMutex_Lock( &tdb->mutex );
	unqlite_close( tdb->db );
	LCUIMutex_Unlock( &tdb->mutex );
	LCUIMutex_Destroy( &tdb->mutex );
	free( tdb );
}

static int ThumbDB_Lock( ThumbDB tdb )
{
	if( tdb->closed ) {
		return -1;
	}
	LCUIMutex_Lock( &tdb->mutex );
	if( tdb->closed ) {
		return -1;
	}
	return 0;
}

#define ThumbDB_Unlock(TDB) LCUIMutex_Unlock( &(TDB)->mutex )
#define ASSERT(X) if(!(X)) { return -1; }

int ThumbDB_Load( ThumbDB tdb, const char *filepath, ThumbData data )
{
	int rc;
	uchar_t *bytes;
	unqlite_int64 size;
	ThumbDataBlock block;
	ASSERT( ThumbDB_Lock( tdb ) == 0 );
	rc = unqlite_kv_fetch( tdb->db, filepath, -1, NULL, &size );
	if( rc != UNQLITE_OK ) {
		ThumbDB_Unlock( tdb );
		return -1;
	}
	block = malloc( (size_t)size );
	bytes = (uchar_t*)block + sizeof( ThumbDataBlockRec );
	rc = unqlite_kv_fetch( tdb->db, filepath, -1, block, &size );
	ThumbDB_Unlock( tdb );
	if( rc != UNQLITE_OK ) {
		return -1;
	}
	Graph_Init( &data->graph );
	data->graph.color_type = block->color_type;
	Graph_Create( &data->graph, block->width, block->height );
	memcpy( data->graph.bytes, bytes, block->mem_size );
	data->modify_time = block->modify_time;
	data->origin_width = block->origin_width;
	data->origin_height = block->origin_height;
	free( block );
	return 0;
}

int ThumbDB_Save( ThumbDB tdb, const char *filepath, ThumbData data )
{
	int rc;
	uchar_t *buff;
	ThumbDataBlock block;
	size_t head_size = sizeof( ThumbDataBlockRec );
	size_t size = head_size + data->graph.mem_size;
	if( size > THUMB_MAX_SIZE ) {
		return -1;
	}
	ASSERT( ThumbDB_Lock( tdb ) == 0 );
	block = malloc( size );
	buff = (uchar_t*)block + head_size;
	block->width = data->graph.width;
	block->height = data->graph.height;
	block->mem_size = data->graph.mem_size;
	block->modify_time = data->modify_time;
	block->origin_width = data->origin_width;
	block->origin_height = data->origin_height;
	block->color_type = data->graph.color_type;
	memcpy( buff, data->graph.bytes, data->graph.mem_size );
	rc = unqlite_kv_store( tdb->db, filepath, -1, block, size );
	if( rc == UNQLITE_OK ) {
		unqlite_commit( tdb->db );
	}
	ThumbDB_Unlock( tdb );
	free( block );
	return rc == UNQLITE_OK ? 0 : -2;
}
