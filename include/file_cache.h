/* ***************************************************************************
* file_cache.h -- file list cache, it used for file list changes detect.
*
* Copyright (C) 2015 by Liu Chao <lc-soft@live.cn>
*
* This file is part of the LC-Finder project, and may only be used, modified,
* and distributed under the terms of the GPLv2.
*
* (GPLv2 is abbreviation of GNU General Public License Version 2)
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
* file_cache.h -- 文件列表的缓存，方便检测文件列表的增删状态。
*
* 版权所有 (C) 2015 归属于 刘超 <lc-soft@live.cn>
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

#ifndef __FILE_CACHE_H__
#define __FILE_CACHE_H__

typedef enum {
	STATE_NONE,
	STATE_STARTED,
	STATE_FINISHED
} SyncTaskState;

/** 文件列表同步任务 */
typedef struct SyncTaskRec_ {
	char *dirpath;			/**< 目标文件夹位置 */
	SyncTaskState state;		/**< 任务状态 */
	unsigned long int count;	/**< 当前已经缓存的文件数量 */
} SyncTaskRec, *SyncTask;

typedef void(*FileHanlder)(void*, const char*);

/** 新建同步任务 */
SyncTask NewSyncTask( const char *dirpath );

/** 删除同步任务 */
void DeleteSyncTask( SyncTask *tptr );

/** 遍历每个新增的文件 */
int SyncTask_InAddFiles( SyncTask t, FileHanlder func, void *func_data );

/** 遍历每个已删除的文件 */
int SyncTask_InDeletedFiles( SyncTask t, FileHanlder func, void *func_data );

/** 开始同步文件列表 */
int LCFinder_StartSyncFiles( SyncTask t );

/** 终止同步文件列表 */
void LCFinder_StopSync( SyncTask t );

#endif
