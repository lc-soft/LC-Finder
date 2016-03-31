#include <stdio.h>
#include "common.h"

const char *getdirname( const char *path )
{
	int i;
	const char *p = NULL;
	for( i = 0; path[i]; ++i ) {
		if( path[i] == PATH_SEP ) {
			p = path + i + 1;
		}
	}
	return p;
}
