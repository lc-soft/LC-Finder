#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/font/charset.h>
#include "finder.h"
#include "ui.h"
#define EVENT_TOTAL 2

#ifdef _WIN32
#include <direct.h>
#define mkdir(PATH) _wmkdir(PATH)
#endif

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

typedef struct FileSyncStatusRec_ {
	int state;
	int count;
	SyncTask task;
} FileSyncStatusRec, *FileSyncStatus;

int LCFinder_SyncFiles( FileSyncStatus s )
{
	int i, len;
	DB_Dir dir;
	wchar_t *path;
	s->count = 0;
	s->state = STATE_STARTED;
	for( i = 0; i < finder.n_dirs; ++i ) {
		dir = finder.dirs[i];
		len = strlen( dir->path ) + 1;
		path = malloc( sizeof( wchar_t )*len );
		len = LCUI_DecodeString( path, dir->path, len, ENCODING_UTF8 );
		path[len] = 0;
		wprintf(L"scan path: %s\n", path);
		s->task = SyncTask_NewW( finder.fileset_dir, path );
		SyncTask_Start( s->task );
		s->count += s->task->count;
		free( path );
	}
	return s->count;
}

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>

static void InitConsoleWindow( void )
{
	int hCrt;
	FILE *hf;
	AllocConsole();
	hCrt = _open_osfhandle( (long)GetStdHandle( STD_OUTPUT_HANDLE ), _O_TEXT );
	hf = _fdopen( hCrt, "w" );
	*stdout = *hf;
	setvbuf( stdout, NULL, _IONBF, 0 );
	printf( "InitConsoleWindow OK!\n" );
}

#endif

static LCFinder_InitWorkDir( void )
{
	int len;
	wchar_t *dirs[2] = {L"fileset", L"thumbs"};
	wchar_t data_dir[1024] = L"F:\\代码库\\GitHub\\LC-Finder\\data\\";
	len = wcslen( data_dir );
	if( data_dir[len - 1] != PATH_SEP ) {
		data_dir[len++] = PATH_SEP;
		data_dir[len] = 0;
	}
	finder.data_dir = NEW( wchar_t, len + 2 );
	finder.fileset_dir = NEW( wchar_t, len + 2 + wcslen(dirs[0]) );
	finder.thumbs_dir = NEW( wchar_t, len + 2 +wcslen(dirs[1]) );
	wcscpy( finder.data_dir, data_dir );
	wsprintf( finder.fileset_dir, L"%s%s", data_dir, dirs[0] );
	wsprintf( finder.thumbs_dir, L"%s%s", data_dir, dirs[1] );
	mkdir( finder.data_dir );
	mkdir( finder.fileset_dir );
	mkdir( finder.thumbs_dir );
}

int main( int argc, char **argv )
{
	FileSyncStatusRec status;

#ifdef LCUI_BUILD_IN_WIN32
	InitConsoleWindow();
#endif
	_wchdir( L"F:\\代码库\\GitHub\\LC-Finder" );
	DB_Init();
	finder.n_dirs = DB_GetDirs( &finder.dirs );
	finder.n_tags = DB_GetTags( &finder.tags );
	LCFinder_InitWorkDir();
	LCFinder_SyncFiles( &status );
	_DEBUG_MSG( "scan files: %d\n", status.count );
	return 0;
	LCFinder_InitEvents();
	UI_Init();
	return UI_Run();
}
