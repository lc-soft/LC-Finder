#include <stdio.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include "common.h"

static unsigned int Dict_KeyHash( const void *key )
{
	const char *buf = key;
	unsigned int hash = 5381;
	while( *buf ) {
		hash = ((hash << 5) + hash) + (*buf++);
	}
	return hash;
}

static int Dict_KeyCompare( void *privdata, const void *key1, const void *key2 )
{
	if( strcmp( key1, key2 ) == 0 ) {
		return 1;
	}
	return 0;
}

static void *Dict_KeyDup( void *privdata, const void *key )
{
	char *newkey = malloc( (strlen( key ) + 1)*sizeof( char ) );
	strcpy( newkey, key );
	return newkey;
}

static void *Dict_ValueDup( void *privdata, const void *val )
{
	int *newval = malloc( sizeof( int ) );
	*newval = *((int*)val);
	return newval;
}

static void Dict_KeyDestructor( void *privdata, void *key )
{
	free( key );
}

static void Dict_ValueDestructor( void *privdata, void *val )
{
	free( val );
}

Dict *StrDict_Create( void *(*val_dup)(void*, const void*), 
		      void(*val_del)(void*, void*) )
{
	DictType *dtype = NEW( DictType, 1 );
	dtype->hashFunction = Dict_KeyHash;
	dtype->keyCompare = Dict_KeyCompare;
	dtype->keyDestructor = Dict_KeyDestructor;
	dtype->keyDup = Dict_KeyDup;
	dtype->valDup = val_dup;
	dtype->valDestructor = val_del;
	return Dict_Create( dtype, dtype );
}

void StrDict_Release( Dict *d )
{
	free( d->privdata );
	d->privdata = NULL;
	Dict_Release( d );
}

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

int pathjoin( char *path, const char *path1, const char *path2 )
{
	int len = strlen( path1 );
	strcpy( path, path1 );
	if( path1[len-1] != PATH_SEP ) {
		path[len++] = PATH_SEP;
		path[len] = 0;
	}
	strcpy( path + len, path2 );
	len = strlen( path );
	if( path1[len-1] == PATH_SEP ) {
		--len;
		path[len] = 0;
	}
	return len;
}

int wpathjoin( wchar_t *path, const wchar_t *path1, const wchar_t *path2 )
{
	int len = wcslen( path1 );
	wcscpy( path, path1 );
	if( path1[len-1] != PATH_SEP ) {
		path[len++] = PATH_SEP;
		path[len] = 0;
	}
	wcscpy( path + len, path2 );
	len = wcslen( path );
	if( path1[len-1] == PATH_SEP ) {
		--len;
		path[len] = 0;
	}
	return len;
}
