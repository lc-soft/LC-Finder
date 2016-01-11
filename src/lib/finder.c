#include <string.h>
#include "file_search.h"
#include "file_cache.h"

static struct FinderRec_ {
	DB_Dir *dirs;
	DB_Tag *tags;
	int n_dirs;
	int n_tags;
} finder;

DB_Dir LCFinder_GetDir( const char *dirpath )
{
	int i;
	DB_Dir dir;
	for( i = 0; i < finder.n_dirs; ++i ) {
		dir = finder.dirs[i];
		if( strcmp(dir->path, dirpath) == 0 ) {
			return dir;
		}
	}
	return NULL;
}

DB_Dir LCFinder_AddDir( const char *dirpath )
{
	int i;
	DB_Dir dir;
	for( i = 0; i < finder.n_dirs; ++i ) {
		dir = finder.dirs[i];
		if( strstr( dir->path, dirpath ) == 0 ) {
			return NULL;
		}
	}
	return DB_AddDir( dirpath );
}

void LCFinder_Init(void)
{
	finder.n_dirs = DB_GetDirs( &finder.dirs );
	finder.n_tags = DB_GetTags( &finder.tags );
}
 