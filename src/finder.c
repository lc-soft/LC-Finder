/* ***************************************************************************
* finder.c -- main code of LC-Finder, responsible for the initialization of 
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
* finder.c -- LCUI-Finder 主程序代码，负责整个程序的初始化和其它功能的调度。
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#define mkdir(PATH) _wmkdir(PATH)
#endif
#include "finder.h"
#include "ui.h"
#include <LCUI/font/charset.h>

#define EVENT_TOTAL 2
#define EncodeUTF8(STR, WSTR, LEN) LCUI_EncodeString( STR, WSTR, LEN, ENCODING_UTF8 )

Finder finder;

typedef struct DirStatusDataPackRec_ {
	FileSyncStatus status;
	DB_Dir dir;
} DirStatusDataPackRec, *DirStatusDataPack;

typedef struct EventPackRec_ {
	EventHandler handler;
	void *data;
} EventPackRec, *EventPack;

static void OnEvent( LCUI_Event e, void *arg )
{
	EventPack pack = e->data;
	pack->handler( pack->data, arg );
}

int LCFinder_BindEvent( int event_id, EventHandler handler, void *data )
{
	EventPack pack = NEW( EventPackRec, 1 );
	pack->handler = handler;
	pack->data = data;
	return EventTrigger_Bind( finder.trigger, event_id, OnEvent, pack, free );
}

int LCFinder_TriggerEvent( int event_id, void *data )
{
	return  EventTrigger_Trigger( finder.trigger, event_id, data );
}

DB_Dir LCFinder_GetDir( const char *dirpath )
{
	int i;
	DB_Dir dir;
	for( i = 0; i < finder.n_dirs; ++i ) {
		dir = finder.dirs[i];
		if( dir && strcmp(dir->path, dirpath) == 0 ) {
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
		if( dir && strstr( dir->path, path ) ) {
			free( path );
			_DEBUG_MSG("1\n");
			return NULL;
		}
	}
	dir = DB_AddDir( dirpath );
	if( !dir ) {
		_DEBUG_MSG("2\n");
		free( path );
		return NULL;
	}
	finder.n_dirs += 1;
	dirs = realloc( finder.dirs, sizeof( DB_Dir )*finder.n_dirs );
	if( !dirs ) {
		finder.n_dirs -= 1;
		_DEBUG_MSG("3\n");
		return NULL;
	}
	dirs[finder.n_dirs - 1] = dir;
	finder.dirs = dirs;
	return dir;
}

void LCFinder_DeleteDir( DB_Dir dir )
{
	int i;
	DB_DeleteDir( dir->path );
	LCFinder_TriggerEvent( EVENT_DIR_DEL, dir );
	for( i = 0; i < finder.n_dirs; ++i ) {
		dir = finder.dirs[i];
		if( dir == finder.dirs[i] ) {
			finder.dirs[i] = NULL;
			free( dir->path );
			free( dir );
		}
	}
}

static int getfilectime( const wchar_t *path )
{
	struct stat buf;
	int fd, ctime = 0;
	fd = _wopen( path, _O_RDONLY );
	if( fstat( fd, &buf ) == 0 ) {
		ctime = (int)buf.st_ctime;
	}
	_close( fd );
	return ctime;
}

static void SyncAddedFile( void *data, const wchar_t *wpath )
{
	static char path[PATH_LEN];
	DirStatusDataPack pack = data;
	int ctime = getfilectime( wpath );
	pack->status->synced_files += 1;
	LCUI_EncodeString( path, wpath, PATH_LEN, ENCODING_UTF8 );
	DB_AddFile( pack->dir, path, ctime );
	wprintf(L"sync: add file: %s, ctime: %d\n", wpath, ctime);
}

static void SyncDeletedFile( void *data, const wchar_t *wpath )
{
	static char path[PATH_LEN];
	DirStatusDataPack pack = data;
	pack->status->synced_files += 1;
	LCUI_EncodeString( path, wpath, PATH_LEN, ENCODING_UTF8 );
	DB_DeleteFile( pack->dir, path );
	wprintf(L"sync: delete file: %s\n", wpath);
}

DB_Dir LCFinder_GetSourceDir( const char *filepath )
{
	int i;
	DB_Dir dir;
	for( i = 0; i < finder.n_dirs; ++i ) {
		dir = finder.dirs[i];
		if( dir && strstr(filepath, dir->path) == filepath ) {
			return dir;
		}
	}
	return NULL;
}

int LCFinder_SyncFiles( FileSyncStatus s )
{
	int i, len;
	DB_Dir dir;
	wchar_t *path;
	s->task = NULL;
	s->tasks = NULL;
	s->added_files = 0;
	s->synced_files = 0;
	s->scaned_files = 0;
	s->deleted_files = 0;
	s->state = STATE_STARTED;
	s->tasks = NEW( SyncTask, finder.n_dirs );
	for( i = 0; i < finder.n_dirs; ++i ) {
		dir = finder.dirs[i];
		if( !dir ) {
			continue;
		}
		len = strlen( dir->path ) + 1;
		path = malloc( sizeof( wchar_t )*len );
		len = LCUI_DecodeString( path, dir->path, len, ENCODING_UTF8 );
		path[len] = 0;
		s->task = SyncTask_NewW( finder.fileset_dir, path );
		SyncTask_Start( s->task );
		s->added_files += s->task->added_files;
		s->scaned_files += s->task->total_files;
		s->deleted_files += s->task->deleted_files;
		s->tasks[i] = s->task;
		free( path );
	}
	DB_Begin();
	s->state = STATE_SAVING;
	wprintf(L"\n\nstart sync\n");
	for( i = 0; i < finder.n_dirs; ++i ) {
		DirStatusDataPackRec pack;
		pack.dir = finder.dirs[i];
		if( !pack.dir ) {
			continue;
		}
		pack.status = s;
		s->task = s->tasks[i];
		SyncTask_InAddedFiles( s->task, SyncAddedFile, &pack );
		SyncTask_InDeletedFiles( s->task, SyncDeletedFile, &pack );
		SyncTask_Commit( s->task );
		SyncTask_Delete( &s->task );
	}
	DB_Commit();
	wprintf(L"\n\nend sync\n");
	s->state = STATE_FINISHED;
	s->task = NULL;
	free( s->tasks );
	s->tasks = NULL;
	return s->synced_files;
}

#ifdef _WIN32

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

static void LCFinder_InitWorkDir( void )
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
	_wsetlocale(LC_ALL, L"chs");
}

static ThumbDBDict_ValDel( void *privdata, void *val )
{
	ThumbDB_Close( val );
}

static void LCFinder_InitThumbDB( void )
{
	int i;
	ThumbDB db;
	char dirpath[PATH_LEN], path[PATH_LEN], name[44];
	finder.thumb_dbs = StrDict_Create( NULL, ThumbDBDict_ValDel );
	printf("[thumbdb] init...\n");
	for( i = 0; i < finder.n_dirs; ++i ) {
		strcpy( path, finder.dirs[i]->path );
		EncodeSHA1( name, path, strlen(path) );
		EncodeUTF8( dirpath, finder.thumbs_dir, PATH_LEN );
		pathjoin( path, dirpath, name );
		db = ThumbDB_Open( path );
		Dict_Add( finder.thumb_dbs, finder.dirs[i]->path, db );
	}
	printf("[thumbdb] init done\n");
}

static void LCFinder_ExitThumbDB( void )
{
	ThumbDB db;
	DictEntry *entry;
	DictIterator *iter;
	printf("[thumbdb] exit ..\n");
	iter = Dict_GetIterator( finder.thumb_dbs );
	entry = Dict_Next( iter );
	while( entry ) {
		db = DictEntry_GetVal( entry );
		ThumbDB_Close( db );
		entry = Dict_Next( iter );
	}
	Dict_ReleaseIterator( iter );
	Dict_Release( finder.thumb_dbs );
	finder.thumb_dbs = NULL;
	printf("[thumbdb] exit done\n");
}
static void LCFinder_Exit( LCUI_SysEvent e, void *arg )
{
	printf("exit\n");
	LCFinder_ExitThumbDB();
}

int main( int argc, char **argv )
{
#ifdef LCUI_BUILD_IN_WIN32
	InitConsoleWindow();
#endif
	_wchdir( L"F:\\代码库\\GitHub\\LC-Finder" );
	DB_Init();
	finder.n_dirs = DB_GetDirs( &finder.dirs );
	finder.n_tags = DB_GetTags( &finder.tags );
	finder.trigger = EventTrigger();
	LCFinder_InitWorkDir();
	LCFinder_InitThumbDB();
	UI_Init();
	LCUI_BindEvent( LCUI_QUIT, LCFinder_Exit, NULL, NULL );
	return UI_Run();
}
