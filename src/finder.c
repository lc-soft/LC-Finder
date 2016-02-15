#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include "finder.h"
#include "ui.h"
#define EVENT_TOTAL 2

Finder finder;
static LCUI_EventBox finder_events;

typedef struct EventPackRec_ {
	EventHandler handler;
	void *data;
} EventPackRec, *EventPack;

static const char *event_names[EVENT_TOTAL] = { "dir.add", "dir.del" };

void LCFinder_InitEvents( void )
{
	int i;
	LCUI_EventBox *events;
	events = LCUIEventBox_Create();
	for( i = 0; i < EVENT_TOTAL; ++i ) {
		LCUIEventBox_AddEvent( events, event_names[i], i );
	}
	finder_events = events;
}

static void OnEvent( LCUI_Event *e, void *arg )
{
	EventPack pack = arg;
	pack->handler( pack->data, e->data );
}

int LCFinder_BindEvent( const char *name, EventHandler handler, void *data )
{
	EventPack pack = NEW( EventPackRec, 1 );
	pack->handler = handler;
	pack->data = data;
	return LCUIEventBox_Bind( finder_events, name, OnEvent, pack, free );
}

int LCFinder_SendEvent( const char *name, void *data )
{
	return LCUIEventBox_Send( finder_events, name, data );
}

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
	int i, len;
	char *path;
	DB_Dir dir, *dirs;
	len = strlen( dirpath );
	path = malloc( (len + 2) * sizeof( char ) );
	strcpy( path, dirpath );
	if( path[len - 1] != PATH_SEP ) {
		path[len] = PATH_SEP;
		path[len + 1] = 0;
	}
	for( i = 0; i < finder.n_dirs; ++i ) {
		dir = finder.dirs[i];
		if( strstr( dir->path, path ) ) {
			return NULL;
		}
	}
	dir = DB_AddDir( dirpath );
	if( !dir ) {
		return NULL;
	}
	finder.n_dirs += 1;
	dirs = realloc( finder.dirs, sizeof( DB_Dir )*finder.n_dirs );
	if( !dirs ) {
		finder.n_dirs -= 1;
		return dir;
	}
	finder.dirs = dirs;
	return dir;
}

static void CheckAddedFile( void *data, const wchar_t *path )
{
	wprintf(L"add file: %s\n", path);
}

static void CheckDeletedFile( void *data, const wchar_t *path )
{
	wprintf(L"delete file: %s\n", path);
}

int main( int argc, char **argv )
{
	_wchdir( L"F:\\代码库\\GitHub\\LC-Finder" );
	DB_Init();
	finder.n_dirs = DB_GetDirs( &finder.dirs );
	finder.n_tags = DB_GetTags( &finder.tags );
	LCFinder_InitEvents();
	UI_Init();
	return UI_Run();
}
