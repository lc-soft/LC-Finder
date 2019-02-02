/* ***************************************************************************
 * finder.h -- main code of LC-Finder, responsible for the initialization of
 * the LC-Finder and the scheduling of other functions.
 *
 * Copyright (C) 2016-2019 by Liu Chao <lc-soft@live.cn>
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

#ifndef LCFINDER_H
#define LCFINDER_H

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include "build.h"
#include "types.h"
#include "bridge.h"
#include "common.h"
#include "file_cache.h"
#include "file_search.h"
#include "thumb_db.h" 
#include "thumb_cache.h" 

LCFINDER_BEGIN_HEADER

/** 事件类型 */
enum LCFinderEventType {
	EVENT_DIR_ADD,
	EVENT_DIR_DEL,
	EVENT_SYNC,
	EVENT_SYNC_DONE,
	EVENT_TAG_ADD,
	EVENT_TAG_UPDATE,
	EVENT_DIRS_CHG,
	EVENT_FILE_DEL,
	EVENT_THUMBDB_DEL_DONE,
	EVENT_LANG_CHG,
	EVENT_PRIVATE_SPACE_CHG,
	EVENT_LICENSE_CHG
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
	char encrypted_password[48];	/**< 加密后的密码 */
	int scaling;			/**< 界面的缩放比例，100 ~ 200 */

	wchar_t detector_model_name[64];
} FinderConfigRec, *FinderConfig;

typedef struct FinderLicenseRec_ {
	LCUI_BOOL is_active;
	LCUI_BOOL is_trial;
} FinderLicenseRec, *FinderLicense;

enum FinderState {
	FINDER_STATE_NONE,
	FINDER_STATE_ACTIVATED,
	FINDER_STATE_BLOCKED
};

/** LCFinder 的主要数据记录 */
typedef struct Finder_ {
	int state;			/**< 当前状态 */
	DB_Dir *dirs;			/**< 源文件夹列表 */
	DB_Tag *tags;			/**< 标签列表 */
	size_t n_dirs;			/**< 多少个源文件夹 */
	size_t n_tags;			/**< 多少个标签 */
	wchar_t *work_dir;		/**< 工作目录 */
	wchar_t *data_dir;		/**< 数据文件夹 */
	wchar_t *fileset_dir;		/**< 文件列表缓存所在文件夹 */
	wchar_t *thumbs_dir;		/**< 缩略图数据库所在文件夹 */
	wchar_t **thumb_paths;		/**< 缩略图数据库路径列表 */
	ThumbCache thumb_cache;		/**< 缩略图数据缓存 */
	Dict *thumb_dbs;		/**< 缩略图数据库记录，以源文件夹路径作为索引 */
	LCUI_EventTrigger trigger;	/**< 事件触发器 */
	FinderConfigRec config;		/**< 当前配置 */
	FinderLicenseRec license;	/**< 当前许可证状态信息 */
	int open_private_space;		/**< 是否打开了私人空间 */
	int storage;			/**< 文件服务连接标识符，主要用于获取文件基本信息 */
	int storage_for_image;		/**< 文件服务连接标识符，主要用于读取图片内容 */
	int storage_for_thumb;		/**< 文件服务连接标识符，主要用于获取图片缩略图 */
	int storage_for_scan;		/**< 文件服务连接标识符，主要用于扫描文件列表 */
} Finder;

typedef void(*LCFinder_EventHandler)(void*, void*);

/** 文件同步状态记录 */
typedef struct FileSyncStatusRec_ {
	size_t task_i;
	int state;		/**< 当前状态 */
	size_t files;		/**< 文件总数 */
	size_t dirs;		/**< 目录总数 */
	size_t added_files;	/**< 增加的文件数量 */
	size_t changed_files;	/**< 改变的文件数量 */
	size_t deleted_files;	/**< 删除的文件数量 */
	size_t scaned_files;	/**< 已扫描的文件数量 */
	size_t synced_files;	/**< 已同步的文件数量 */
	size_t scaned_dirs;	/**< 已扫描的目录数量 */
	SyncTask task;		/**< 当前正执行的任务 */
	SyncTask *tasks;	/**< 所有任务 */
	void *data;
	void(*callback)(void*);
} FileSyncStatusRec, *FileSyncStatus;

extern Finder finder;

/** 绑定事件 */
int LCFinder_BindEvent(int event_id, LCFinder_EventHandler handler, void *data);

/** 触发事件 */
int LCFinder_TriggerEvent(int event_id, void *data);

/** 获取指定文件路径所处的源文件夹 */
DB_Dir LCFinder_GetSourceDir(const char *filepath);

size_t LCFinder_GetSourceDirList(DB_Dir **outdirs);

/** 获取缩略图数据库总大小 */
int64_t LCFinder_GetThumbDBTotalSize(void);

/** 清除缩略图数据库 */
void LCFinder_ClearThumbDB(void);

void LCFinder_SyncFilesAsync(FileSyncStatus s);

DB_Dir LCFinder_GetDir(const char *dirpath);

DB_Dir LCFinder_AddDir(const char *dirpath, const char *token, int visible);

DB_Tag LCFinder_GetTagById(int id);

DB_Tag LCFinder_GetTag(const char *tagname);

DB_Tag LCFinder_AddTag(const char *tagname);

DB_Tag LCFinder_AddTagForFile(DB_File file, const char *tagname);

/** 获取文件的标签列表 */
size_t LCFinder_GetFileTags(DB_File file, DB_Tag **outtags);

/** 重新载入标签列表 */
void LCFinder_ReloadTags(void);

void LCFinder_DeleteDir(DB_Dir dir);

size_t LCFinder_DeleteFiles(char * const *files, size_t nfiles,
			    int(*onstep)(void*, size_t, size_t),
			    void *privdata);

/** 保存配置 */
int LCFinder_SaveConfig(void);

/** 载入配置 */
int LCFinder_LoadConfig(void);

/** 验证密码 */
LCUI_BOOL LCFinder_AuthPassword(const char *password);

/** 设置密码 */
void LCFinder_SetPassword(const char *password);

/** 开启私人空间 */
void LCFinder_OpenPrivateSpace(void);

/** 关闭私人空间 */
void LCFinder_ClosePrivateSpace(void);

int LCFinder_Init(int argc, char **argv);

int LCFinder_Run(void);

void LCFinder_Exit(void);

LCFINDER_END_HEADER

#endif
