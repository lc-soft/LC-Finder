#ifndef __LC_FINDER_H__
#define __LC_FINDER_H__

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include "common.h"
#include "file_cache.h"
#include "file_search.h"
#include "thumb_cache.h" 

enum LCFinderEventType {
	EVENT_DIR_ADD,
	EVENT_DIR_DEL,
	EVENT_SYNC
};

typedef struct Finder_ {
	DB_Dir *dirs;
	DB_Tag *tags;
	int n_dirs;
	int n_tags;
	wchar_t *data_dir;
	wchar_t *fileset_dir;
	wchar_t *thumbs_dir;
	ThumbCache *thumb_caches;
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

int LCFinder_SyncFiles( FileSyncStatus s );

DB_Dir LCFinder_GetDir( const char *dirpath );

DB_Dir LCFinder_AddDir( const char *dirpath );

#endif
