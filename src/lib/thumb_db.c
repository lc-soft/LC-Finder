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
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/graph.h>
#include "thumb_db.h"

#define THUMB_MAX_SIZE 8553600

typedef struct ThumbDataBlockRec_ {
	size_t width;
	size_t height;
	size_t mem_size;
	int color_type;
	int modify_time;
} ThumbDataBlockRec, *ThumbDataBlock;

ThumbDB ThumbDB_Open( const char *filepath )
{
	int rc;
	ThumbDB db;
	rc = unqlite_open( &db, filepath, UNQLITE_OPEN_CREATE );
	if( rc != UNQLITE_OK ) {
		return NULL;
	}
	rc = unqlite_kv_store( db, "__program__", -1, "LCFinder", 9 );
	if( rc != UNQLITE_OK ) {
		printf( "[thumbdb] cannot open db: %s\n", filepath );
	}
	return db;
}

void ThumbDB_Close( ThumbDB db )
{
	unqlite_close( db );
}

int ThumbDB_Load( ThumbDB db, const char *filepath, ThumbData data )
{
	int rc;
	uchar_t *bytes;
	unqlite_int64 size;
	ThumbDataBlock block;
	rc = unqlite_kv_fetch( db, filepath, -1, NULL, &size );
	if( rc != UNQLITE_OK ) {
		return -1;
	}
	block = malloc( (size_t)size );
	bytes = (uchar_t*)block + sizeof( ThumbDataBlockRec );
	rc = unqlite_kv_fetch( db, filepath, -1, block, &size );
	if( rc != UNQLITE_OK ) {
		return -1;
	}
	Graph_Init( &data->graph );
	data->graph.color_type = block->color_type;
	Graph_Create( &data->graph, block->width, block->height );
	memcpy( data->graph.bytes, bytes, block->mem_size );
	data->modify_time = block->modify_time;
	free( block );
	return 0;
}

int ThumbDB_Save( ThumbDB db, const char *filepath, ThumbData data )
{
	int rc;
	uchar_t *buff;
	ThumbDataBlock block;
	size_t head_size = sizeof( ThumbDataBlockRec );
	size_t size = head_size + data->graph.mem_size;
	if( size > THUMB_MAX_SIZE ) {
		return -1;
	}
	block = malloc( size );
	buff = (uchar_t*)block + head_size;
	block->width = data->graph.width;
	block->height = data->graph.height;
	block->mem_size = data->graph.mem_size;
	block->modify_time = data->modify_time;
	block->color_type = data->graph.color_type;
	memcpy( buff, data->graph.bytes, data->graph.mem_size );
	rc = unqlite_kv_store( db, filepath, -1, block, size );
	free( block );
	return rc == UNQLITE_OK ? 0 : -2;
}
