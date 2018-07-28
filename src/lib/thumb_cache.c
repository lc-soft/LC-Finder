/* ***************************************************************************
 * thumb_cache.c -- thumbnail data cache
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
 * thumb_cache.c -- 缩略图数据缓存
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

#define LCFINDER_THUMB_CACHE_C
#include <stdlib.h>
#include <string.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/graph.h>
#include <LCUI/thread.h>
#include "common.h"

 /** 缩略图连接器记录 */
typedef struct ThumbLinkerRec_ {
	LinkedList links;		/**< 缩略图链接记录 */
	void (*on_remove)(void*);	/**< 回调函数，当缩略图被移出缓存池时被调用 */
	LinkedListNode node;		/**< 所在链表的结点 */
} ThumbLinkerRec, *ThumbLinker;

/** 缓存区的数据结构 */
typedef struct ThumbCacheRec_ {
	size_t size;			/**< 当前大小 */
	size_t max_size;		/**< 最大大小 */
	Dict *paths;			/**< 缩略图路径映射表 */
	LinkedList thumbs;		/**< 缩略图缓存列表 */
	LinkedList linkers;		/**< 缩略图链接器列表 */
	LCUI_Mutex mutex;		/**< 互斥锁 */
} ThumbCacheRec, *ThumbCache;

#include "thumb_cache.h"

/** 缩略图数据节点 */
typedef struct ThumbDataNodeRec_ {
	LCUI_Graph graph;		/**< 缩略图 */
	char *path;			/**< 路径 */
	LinkedList links;		/**< 链接列表 */
	LinkedListNode node;		/**< 在列表中的节点 */
	ThumbCache cache;		/**< 所属的缓存 */
} ThumbDataNodeRec, *ThumbDataNode;

/** 缩略图链接记录 */
typedef struct ThumbLinkRec_ {
	ThumbLinker linker;		/**< 所属链接器 */
	void *privdata;			/**< 私有数据 */
	LinkedListNode node;		/**< 所在链表的结点 */
	ThumbDataNode tnode;		/**< 所在缩略图数据结点 */
} ThumbLinkRec, *ThumbLink;

static void OnDirectDeleteThumbLink( void *data )
{
	ThumbLink lnk = data;
	LinkedList_Unlink( &lnk->linker->links, &lnk->node );
	lnk->linker->on_remove( lnk->privdata );
	lnk->privdata = NULL;
	free( lnk );
}

static void OnDeleteThumbLink( void *data )
{
	ThumbLink lnk = data;
	LinkedList_Unlink( &lnk->tnode->links, &lnk->node );
	lnk->linker->on_remove( lnk->privdata );
	if( lnk->tnode->links.length < 1 ) {
		ThumbCache_Delete( lnk->tnode->cache, lnk->tnode->path );
	}
	lnk->privdata = NULL;
	free( lnk );
}

ThumbCache ThumbCache_New( size_t max_size )
{
	ThumbCache cache = NEW( ThumbCacheRec, 1 );
	LinkedList_Init( &cache->linkers );
	LinkedList_Init( &cache->thumbs );
	cache->max_size = max_size;
	cache->paths = StrDict_Create( NULL, NULL );
	LCUIMutex_Init( &cache->mutex );
	return cache;
}

ThumbLinker ThumbCache_AddLinker( ThumbCache cache, void( *on_remove )(void*) )
{
	ThumbLinker tlnk = NEW( ThumbLinkerRec, 1 );
	LinkedList_Init( &tlnk->links );
	tlnk->on_remove = on_remove;
	tlnk->node.data = tlnk;
	LCUIMutex_Lock( &cache->mutex );
	LinkedList_AppendNode( &cache->linkers, &tlnk->node );
	LCUIMutex_Unlock( &cache->mutex );
	return tlnk;
}

void ThumbCache_DeleteLinker( ThumbCache cache, ThumbLinker linker )
{
	LinkedList_Unlink( &cache->linkers, &linker->node );
	LinkedList_ClearData( &linker->links, OnDeleteThumbLink );
	free( linker );
}

LCUI_Graph *ThumbCache_Get( ThumbCache cache, const char *path )
{
	ThumbDataNode data;
	LCUIMutex_Lock( &cache->mutex );
	data = Dict_FetchValue( cache->paths, path );
	LCUIMutex_Unlock( &cache->mutex );
	if( data ) {
		return &data->graph;
	}
	return NULL;
}

int ThumbCache_Delete( ThumbCache cache, const char *path )
{
	ThumbDataNode tdn;
	LCUIMutex_Lock( &cache->mutex );
	tdn = Dict_FetchValue( cache->paths, path );
	if( !tdn ) {
		LCUIMutex_Unlock( &cache->mutex );
		return -1;
	}
	LinkedList_Unlink( &cache->thumbs, &tdn->node );
	LinkedList_ClearData( &tdn->links, OnDirectDeleteThumbLink );
	cache->size -= tdn->graph.mem_size;
	Dict_Delete( cache->paths, path );
	LCUIMutex_Unlock( &cache->mutex );
	Graph_Free( &tdn->graph );
	free( tdn->path );
	free( tdn );
	return 0;
}

LCUI_Graph *ThumbCache_Add( ThumbCache cache, const char *path, 
			    LCUI_Graph *thumb )
{
	int len;
	size_t size;
	ThumbDataNode tdn;
	LinkedListNode *node, *prev_node;
	LCUIMutex_Lock( &cache->mutex );
	size = cache->size + thumb->mem_size;
	if( size > cache->max_size ) {
		for( LinkedList_Each( node, &cache->thumbs ) ) {
			tdn = node->data;
			size = cache->size + thumb->mem_size;
			if( size < cache->max_size ) {
				break;
			}
			/** 移除老的缩略图数据 */
			prev_node = node->prev;
			ThumbCache_Delete( cache, tdn->path );
			node = prev_node;
		}
		size = cache->size + thumb->mem_size;
		if( size > cache->max_size ) {
			LCUIMutex_Unlock( &cache->mutex );
			return NULL;
		}
	}
	cache->size = size;
	tdn = NEW( ThumbDataNodeRec, 1 );
	tdn->graph = *thumb;
	tdn->cache = cache;
	tdn->node.data = tdn;
	len = strlen( path ) + 1;
	tdn->path = NEW( char, len );
	strncpy( tdn->path, path, len );
	LinkedList_Init( &tdn->links );
	LinkedList_AppendNode( &cache->thumbs, &tdn->node );
	Dict_Add( cache->paths, tdn->path, tdn );
	LCUIMutex_Unlock( &cache->mutex );
	return &tdn->graph;
}

LCUI_Graph *ThumbCache_Link( ThumbCache cache, const char *path,
			     ThumbLinker linker, void *privdata )
{
	ThumbLink lnk;
	ThumbDataNode data;
	LinkedListNode *node;
	LCUIMutex_Lock( &cache->mutex );
	data = Dict_FetchValue( cache->paths, path );
	if( !data ) {
		LCUIMutex_Unlock( &cache->mutex );
		return NULL;
	}
	for( LinkedList_Each( node, &data->links ) ) {
		lnk = node->data;
		if( lnk->linker == linker ) {
			lnk->privdata = privdata;
			break;
		}
	}
	if( !node ) {
		lnk = NEW( ThumbLinkRec, 1 );
		lnk->privdata = privdata;
		lnk->node.data = lnk;
		lnk->linker = linker;
		lnk->tnode = data;
		LinkedList_AppendNode( &data->links, &lnk->node );
	}
	LCUIMutex_Unlock( &cache->mutex );
	return &data->graph;
}

int ThumbCache_Unlink( ThumbCache cache, ThumbLinker linker, const char *path )
{
	ThumbDataNode data;
	LinkedListNode *node;
	LCUIMutex_Lock( &cache->mutex );
	data = Dict_FetchValue( cache->paths, path );
	if( !data ) {
		LCUIMutex_Unlock( &cache->mutex );
		return -1;
	}
	for( LinkedList_Each( node, &data->links ) ) {
		ThumbLink lnk = node->data;
		if( lnk->linker == linker ) {
			OnDeleteThumbLink( node->data );
			break;
		}
	}
	LCUIMutex_Unlock( &cache->mutex );
	return 0;
}
