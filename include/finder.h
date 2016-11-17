/* ***************************************************************************
 * finder.h -- main code of LC-Finder, responsible for the initialization of 
 * the LC-Finder and the scheduling of other functions.
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
 * finder.h -- LC-Finder 主程序代码，负责整个程序的初始化和其它功能的调度。
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

#ifndef LCFINDER_H
#define LCFINDER_H

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include "common.h"
#include "file_cache.h"
#include "file_search.h"
#include "thumb_db.h" 
#include "thumb_cache.h" 

enum VersionType {
	VERSION_RELEASE,
	VERSION_RC,
	VERSION_BETA,
	VERSION_ALPHA
};

#define LCFINDER_NAME		L"LC's Finder"
#define LCFINDER_FOLDER_NAME	L"LCFinder"
#define LCFINDER_CONFIG_HEAD	"LCFinder Config Data"
#define LCFINDER_VER_MAJOR	0
#define LCFINDER_VER_MINOR	1
#define LCFINDER_VER_REVISION	0
#define LCFINDER_VER_TYPE	VERSION_BETA

/** 事件类型 */
enum LCFinderEventType {
	EVENT_DIR_ADD,
	EVENT_DIR_DEL,
	EVENT_SYNC,
	EVENT_SYNC_DONE,
	EVENT_TAG_ADD,
	EVENT_TAG_UPDATE,
	EVENT_FILE_DEL,
	EVENT_THUMBDB_DEL_DONE,
	EVENT_LANG_CHG
};

/** 配置数据结构 */
typedef struct FinderConfigRec_ {
	char head[32];			/**< 头部标记 */
	struct {
		int major;
		int minor;
		int revision;
		int type;
	} version;			/**< 版本号 */
	char language[32];		/**< 当前语言 */
	int files_sort;			/**< 文件的排序方式 */
} FinderConfigRec, *FinderConfig;

/** LCFinder 的主要数据记录 */
typedef struct Finder_ {
	DB_Dir *dirs;			/**< 源文件夹列表 */
	DB_Tag *tags;			/**< 标签列表 */
	int n_dirs;			/**< 多少个源文件夹 */
	int n_tags;			/**< 多少个标签 */
	wchar_t *work_dir;		/**< 工作目录 */
	wchar_t *data_dir;		/**< 数据文件夹 */
	wchar_t *fileset_dir;		/**< 文件列表缓存所在文件夹 */
	wchar_t *thumbs_dir;		/**< 缩略图数据库所在文件夹 */
	wchar_t **thumb_paths;		/**< 缩略图数据库路径列表 */
	ThumbCache thumb_cache;		/**< 缩略图数据缓存 */
	Dict *thumb_dbs;		/**< 缩略图数据库记录，以源文件夹路径作为索引 */
	LCUI_EventTrigger trigger;	/**< 事件触发器 */
	FinderConfigRec config;		/**< 当前配置 */
} Finder;

typedef void( *EventHandler )(void*, void*);

/** 文件同步状态记录 */
typedef struct FileSyncStatusRec_ {
	int state;		/**< 当前状态 */
	int added_files;	/**< 增加的文件数量 */
	int changed_files;	/**< 改变的文件数量 */
	int deleted_files;	/**< 删除的文件数量 */
	int scaned_files;	/**< 已扫描的文件数量 */
	int synced_files;	/**< 已同步的文件数量 */
	SyncTask task;		/**< 当前正执行的任务 */
	SyncTask *tasks;	/**< 所有任务 */
} FileSyncStatusRec, *FileSyncStatus;

extern Finder finder;

/** 绑定事件 */
int LCFinder_BindEvent( int event_id, EventHandler handler, void *data );

/** 触发事件 */
int LCFinder_TriggerEvent( int event_id, void *data );

/** 获取指定文件路径所处的源文件夹 */
DB_Dir LCFinder_GetSourceDir( const char *filepath );

/** 获取缩略图数据库总大小 */
int64_t LCFinder_GetThumbDBTotalSize( void );

/** 清除缩略图数据库 */
void LCFinder_ClearThumbDB( void );

/** 同步文件 */
int LCFinder_SyncFiles( FileSyncStatus s );

DB_Dir LCFinder_GetDir( const char *dirpath );

DB_Dir LCFinder_AddDir( const char *dirpath );

DB_Tag LCFinder_GetTag( const char *tagname );

DB_Tag LCFinder_AddTag( const char *tagname );

DB_Tag LCFinder_AddTagForFile( DB_File file, const char *tagname );

/** 获取文件的标签列表 */
int LCFinder_GetFileTags( DB_File file, DB_Tag **outtags );

/** 重新载入标签列表 */
void LCFinder_ReloadTags( void );

void LCFinder_DeleteDir( DB_Dir dir );

int LCFinder_DeleteFiles( const char **files, int nfiles,
			  int( *onstep )(void*, int, int), void *privdata );

/** 保存配置 */
int LCFinder_SaveConfig( void );

/** 载入配置 */
int LCFinder_LoadConfig( void );

#endif
