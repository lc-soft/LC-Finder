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
* finder.h -- LCUI-Finder 主程序代码，负责整个程序的初始化和其它功能的调度。
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

#ifndef __LC_FINDER_H__
#define __LC_FINDER_H__

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include "common.h"
#include "file_cache.h"
#include "file_search.h"
#include "thumb_db.h" 
#include "thumb_cache.h" 

enum LCFinderEventType {
	EVENT_DIR_ADD,
	EVENT_DIR_DEL,
	EVENT_SYNC,
	EVENT_SYNC_DONE
};

typedef struct Finder_ {
	DB_Dir *dirs;
	DB_Tag *tags;
	int n_dirs;
	int n_tags;
	wchar_t *data_dir;
	wchar_t *fileset_dir;
	wchar_t *thumbs_dir;
	wchar_t **thumb_paths;
	Dict *thumb_dbs;
	LCUI_EventTrigger trigger;
} Finder;

typedef void( *EventHandler )(void*, void*);

typedef struct FileSyncStatusRec_ {
	int state;
	int added_files;
	int deleted_files;
	int scaned_files;
	int synced_files;
	SyncTask task;		/**< 当前正执行的任务 */
	SyncTask *tasks;	/**< 所有任务 */
} FileSyncStatusRec, *FileSyncStatus;

extern Finder finder;

void LCFinder_Init( void );

/** 绑定事件 */
int LCFinder_BindEvent( int event_id, EventHandler handler, void *data );

/** 触发事件 */
int LCFinder_TriggerEvent( int event_id, void *data );

DB_Dir LCFinder_GetSourceDir( const char *filepath );

int LCFinder_SyncFiles( FileSyncStatus s );

DB_Dir LCFinder_GetDir( const char *dirpath );

DB_Dir LCFinder_AddDir( const char *dirpath );

void LCFinder_DeleteDir( DB_Dir dir );

#endif
