/* ***************************************************************************
* thumb_pool.c -- thumbnail data pool
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
* thumb_pool.c -- 缩略图数据缓存池
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

#define __THUMBNAIL_POOL_C__
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/graph.h>
#include "thumb_pool.h"

typedef struct ThumbDataNodeRec_ {
	LCUI_Graph graph;		/** 缩略图 */
	void *privdata;			/** 私有数据 */
} ThumbDataNodeRec, ThumbDataNode;

/** 缓存池的数据结构 */
typedef struct ThumbPoolRec_ {
	size_t size;			/**< 当前大小 */
	size_t max_size;		/**< 最大大小 */
	LinkedList thumbs;		/**< 缩略图缓存列表 */
	void (*on_remove)(void*);	/** 回调函数，当缩略图被移出缓存池时被调用 */
} ThumbPoolRec, *ThumbPool;

ThumbPool ThumbPool_New( size_t max_size, void (*on_remove)(void*) )
{
	ThumbPool pool = NEW( ThumbPoolRec, 1 );
	LinkedList_Init( &pool->thumbs );
	pool->max_size = max_size;
	pool->on_remove = on_remove;
	return pool;
}

int ThumbPool_Add( ThumbPool pool, LCUI_Graph *thumb, void *privdata )
{
	size_t size;
	int count = 0;
	ThumbDataNode *tdn;
	LinkedListNode *node, *prev_node;
	size = pool->size + thumb->mem_size;
	if( size > pool->max_size ) {
		LinkedList_ForEach( node, &pool->thumbs ) {
			++count;
			tdn = node->data;
			size -= tdn->graph.mem_size;
			if( size < pool->max_size ) {
				break;
			}
		}
		if( size > pool->max_size ) {
			return -1;
		}
		/** 移除老的缩略图数据 */
		LinkedList_ForEach( node, &pool->thumbs ) {
			--count;
			if( count < 0 ) {
				break;
			}
			tdn = node->data;
			prev_node = node->prev;
			Graph_Free( &tdn->graph );
			if( pool->on_remove ) {
				pool->on_remove( tdn->privdata );
			}
			LinkedList_DeleteNode( &pool->thumbs, node );
			node = prev_node;
			free( tdn );
		}
	}
	tdn = NEW( ThumbDataNodeRec, 1 );
	tdn->graph = *thumb;
	tdn->privdata = privdata;
	LinkedList_Append( &pool->thumbs, tdn );
	pool->size = size;
	return 0;
}
