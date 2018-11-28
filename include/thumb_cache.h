/* ***************************************************************************
 * thumb_cache.h -- thumbnail data cache
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
 * thumb_cache.h -- 缩略图数据缓存
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

#ifndef LCFINDER_THUMB_CACHE_H
#define LCFINDER_THUMB_CACHE_H

#ifndef LCFINDER_THUMB_CACHE_C
typedef void* ThumbCache;
typedef void* ThumbLinker;
#endif

/** 新建一个缩略图缓存 */
ThumbCache ThumbCache_New(size_t max_size);

void ThumbCache_Destroy(ThumbCache cache);

/** 添加缩略图链接器 */
ThumbLinker ThumbCache_AddLinker(ThumbCache cache, void(*on_remove)(void*));

/** 删除缩略图链接器 */
void ThumbLinker_Destroy(ThumbLinker linker);

/** 直接从缩略图缓存中取缩略图 */
LCUI_Graph *ThumbCache_Get(ThumbCache cache, const char *path);

/** 从缩略图缓存中删除缩略图 */
int ThumbCache_Delete(ThumbCache cache, const char *path);

/** 将缩略图添加至缩略图缓存中 */
LCUI_Graph *ThumbCache_Add(ThumbCache cache, const char *path,
			   LCUI_Graph *thumb);

/** 链接到缩略图，并获取缩略图 */
LCUI_Graph *ThumbLinker_Link(ThumbLinker linker, const char *path,
			     void *privdata);


/** 解除缩略图链接 */
int ThumbLinker_Unlink(ThumbLinker linker, const char *path);

#endif
