/* ***************************************************************************
 * file_cache.h -- file list cache, it used for file list changes detection.
 *
 * Copyright (C) 2015-2018 by Liu Chao <lc-soft@live.cn>
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
 * file_cache.h -- 文件列表的缓存，方便检测文件列表的增删状态。
 *
 * 版权所有 (C) 2015-2018 归属于 刘超 <lc-soft@live.cn>
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

#ifndef LCFINDER_FILE_CACHE_H
#define LCFINDER_FILE_CACHE_H

#include <wchar.h>

typedef enum {
	STATE_NONE,
	STATE_STARTED,
	STATE_SAVING,
	STATE_FINISHED
} SyncTaskState;

typedef struct FileCacheInfoRec_ {
	wchar_t *path;		/**< 文件路径 */
	unsigned int ctime;	/**< 创建时间 */
	unsigned int mtime;	/**< 修改时间 */
} FileCacheInfoRec, *FileCacheInfo;

 /** 文件状态信息 */
typedef struct FileCacheTimeRec_ {
	unsigned int ctime;	/**< 创建时间 */
	unsigned int mtime;	/**< 修改时间 */
} FileCacheTimeRec, *FileCacheTime;

/** 文件列表同步任务 */
typedef struct SyncTaskRec_ {
	wchar_t *file;				/**< 数据文件 */
	wchar_t *tmpfile;			/**< 临时数据文件 */
	wchar_t *scan_dir;			/**< 需扫描的目录 */
	wchar_t *data_dir;			/**< 数据存放目录 */
	SyncTaskState state;			/**< 任务状态 */
	unsigned long int total_files;		/**< 当前缓存的总文件数量 */
	unsigned long int added_files;		/**< 当前缓存的新增的文件数量 */
	unsigned long int changed_files;	/**< 当前缓存的已修改的文件数量 */
	unsigned long int deleted_files;	/**< 当前缓存的删除的文件数量 */
} SyncTaskRec, *SyncTask;

typedef void(*FileInfoHanlder)(void*, const FileCacheInfo);

#ifdef HAVE_FILECACHE
typedef struct FileCacheRec_* FileCache;
#else
typedef void* FileCache;
#endif

FileCache FileCache_Open(const char *file);

void FileCache_Close(FileCache db);

size_t FileCache_ReadAll(FileCache db, FileInfoHanlder handler, void *data);

int FileCache_Put(FileCache db, const wchar_t *path, FileCacheTime time);

int FileCache_Delete(FileCache db, const wchar_t *path);

SyncTask SyncTask_New( const char *data_dir, const char *scan_dir );

/** 新建同步任务 */
SyncTask SyncTask_NewW( const wchar_t *data_dir, const wchar_t *scan_dir );

/** 添加文件至缓存 */
int SyncTask_AddFileW( SyncTask t, const wchar_t *path,
		       unsigned int ctime, unsigned int mtime );

/** 打开缓存 */
int SyncTask_OpenCacheW( SyncTask t, const wchar_t *path );

/** 从缓存中删除一个文件记录 */
int SyncTask_DeleteFileW( SyncTask t, const wchar_t *filepath );

/** 清除缓存 */
void SyncTask_ClearCache( SyncTask t );

/** 关闭缓存 */
void SyncTask_CloseCache( SyncTask t );

/** 删除同步任务 */
void SyncTask_Delete( SyncTask t );

/** 遍历每个新增的文件 */
int SyncTask_InAddedFiles( SyncTask t, FileInfoHanlder func, void *func_data );

/** 遍历每个已修改的文件 */
int SyncTask_InChangedFiles( SyncTask t, FileInfoHanlder func, void *func_data );

/** 遍历每个已删除的文件 */
int SyncTask_InDeletedFiles( SyncTask t, FileInfoHanlder func, void *func_data );

/** 开始同步文件列表 */
int SyncTask_Start( SyncTask t );

/** 结束同步文件列表 */
void SyncTask_Finish( SyncTask t );

/** 提交变更后文件列表至缓存数据库中 */
void SyncTask_Commit( SyncTask t );

#endif
