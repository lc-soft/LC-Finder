/* ***************************************************************************
* thumb_db.h -- thumbnail cache database.
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
* thumb_db.h -- 缩略图缓存数据库
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

#ifndef LCFINDER_THUMB_DB_H
#define LCFINDER_THUMB_DB_H

#ifndef LCFINDER_THUMB_DB_C
typedef void* ThumbDB;
#else:
typedef struct ThumbDBRec_ *ThumbDB;
#endif

typedef struct ThumbDatakRec_ {
	uint_t modify_time;		/**< 修改时间 */
	int origin_width;		/**< 原始宽度 */
	int origin_height;		/**< 原始高度 */
	LCUI_Graph graph;		/**< 缩略图数据 */
} ThumbDataRec, *ThumbData;

/** 新建一个缩略图数据库实例 */
ThumbDB ThumbDB_Open( const char *filepath );

/** 销毁缩略图数据库实例 */
void ThumbDB_Close( ThumbDB tdb );

/** 从数据库中载入指定文件路径的缩略图数据 */
int ThumbDB_Load( ThumbDB tdb, const char *filepath, ThumbData data );

/** 将缩略图数据保存至缓存中 */
int ThumbDB_Save( ThumbDB tdb, const char *filepath, ThumbData data );

#endif
