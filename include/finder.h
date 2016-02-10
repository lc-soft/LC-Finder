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

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

extern Finder finder;

void LCFinder_Init( void );

DB_Dir LCFinder_GetDir( const char *dirpath );

DB_Dir LCFinder_AddDir( const char *dirpath );

#endif
