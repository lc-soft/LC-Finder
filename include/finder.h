#ifndef __LC_FINDER_H__
#define __LC_FINDER_H__

#include "file_cache.h"
#include "file_search.h"

typedef struct Finder_ {
	DB_Dir *dirs;
	DB_Tag *tags;
	int n_dirs;
	int n_tags;
} Finder;

typedef void( *EventHandler )(void*, void*);

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

extern Finder finder;

void LCFinder_Init( void );

/** 绑定事件 */
int LCFinder_BindEvent( const char *name, EventHandler handler, void *data );

/** 触发事件 */
int LCFinder_SendEvent( const char *name, void *data );

DB_Dir LCFinder_GetDir( const char *dirpath );

DB_Dir LCFinder_AddDir( const char *dirpath );

#endif
