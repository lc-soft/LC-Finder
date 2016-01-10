/* ***************************************************************************
* file_cache.c -- file list cache, it used for file list changes detection.
*
* Copyright (C) 2015-2016 by Liu Chao <lc-soft@live.cn>
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
* file_cache.c -- 文件列表的缓存，方便检测文件列表的增删状态。
*
* 版权所有 (C) 2015-2016 归属于 刘超 <lc-soft@live.cn>
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

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include "file_cache.h"

#define MAX_PATH_LEN 2048

/** 文件夹内的文件变更状态统计 */
typedef struct DirStatsRec_ {
	Dict *files;		/** 之前已缓存的文件列表 */
	Dict *added_files;	/** 新增的文件 */
	Dict *deleted_files;	/** 删除的文件 */
} DirStatsRec, *DirStats;

#define GetDirStats(T) (DirStats)(((char*)(T)) + sizeof(SyncTaskRec))

static const char filename[] = ".lc-finder-files.cache";
static const char *match_suffixs[] = {".png", ".bmp", ".jpg", ".jpeg"};

static unsigned int Dict_KeyHash( const unsigned char *buf )
{
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

static void Dict_KeyDestructor( void *privdata, void *key )
{
	free( key );
}

static DictType DictType_Files = {
	Dict_KeyHash,
	Dict_KeyDup,
	NULL,
	Dict_KeyCompare,
	Dict_KeyDestructor,
	NULL
};

SyncTask NewSyncTask( const char *dirpath )
{
	SyncTask t = malloc( sizeof(SyncTaskRec) + sizeof(DirStatsRec) );
	DirStats ds = GetDirStats( t );
	ds->deleted_files = Dict_Create( &DictType_Files, NULL );
	ds->added_files = Dict_Create( &DictType_Files, NULL );
	ds->files = Dict_Create( &DictType_Files, NULL );
	if( dirpath ) {
		t->dirpath = malloc( strlen( dirpath ) + 1 );
		strcpy( t->dirpath, dirpath );
	} else {
		t->dirpath = NULL;
	}
	t->state = STATE_NONE;
	t->count = 0;
	return t;
}

void DeleteSyncTask( SyncTask *tptr )
{
	SyncTask t = *tptr;
	DirStats ds = GetDirStats( t );
	if( t->dirpath ) {
		free( t->dirpath );
		t->dirpath = NULL;
	}
	Dict_Release( ds->files );
	Dict_Release( ds->added_files );
	Dict_Release( ds->deleted_files );
	free( t );
	*tptr = NULL;
}

static int FileDict_ForEach( Dict *d, void( *func )(void*, const char*), void *data )
{
	int count = 0;
	DictEntry *entry;
	DictIterator *iter = Dict_GetIterator( d );
	while( (entry = Dict_Next( iter )) ) {
		func( data, DictEntry_GetKey( entry ) );
		++count;
	}
	Dict_ReleaseIterator( iter );
	return count;
}

int SyncTask_InAddFiles( SyncTask t, FileHanlder func, void *func_data )
{
	DirStats ds = GetDirStats( t );
	return FileDict_ForEach( ds->added_files, func, func_data );
}

int SyncTask_InDeletedFiles( SyncTask t, FileHanlder func, void *func_data )
{
	DirStats ds = GetDirStats( t );
	return FileDict_ForEach( ds->deleted_files, func, func_data );
}

/** 从缓存记录中载入文件列表 */
static int SyncTask_LoadCache( SyncTask t, FILE *fp )
{
	int count = 0;
	char *path, buf[MAX_PATH_LEN];
	DirStats ds = GetDirStats( t );
	while( feof( fp ) ) {
		path = fgets( buf, MAX_PATH_LEN, fp );
		if( path ) {
			Dict_Add( ds->files, path, (void*)1 );
			Dict_Add( ds->deleted_files, path, (void*)1 );
			++count;
		}
	}
	return count;
}

/** 扫描文件 */
static int SyncTask_ScanFiles( SyncTask t, const char *dirpath, FILE *fp )
{
	LCUI_Dir dir;
	LCUI_DirEntry *entry;
	char filepath[2048], *name;
	int i, n, len = strlen( dirpath );
	DirStats ds = GetDirStats( t );
	strcpy( filepath, dirpath );
	if( filepath[len - 1] != '/' ) {
		filepath[len++] = '/';
		filepath[len] = 0;
	}
	LCUI_OpenDirA( filepath, &dir );
	n = sizeof( match_suffixs ) / sizeof( *match_suffixs );
	while( (entry = LCUI_ReadDirA( &dir )) && t->state == STATE_STARTED ) {
		name = LCUI_GetFileNameA( entry );
		strcpy( filepath + len, name );
		if( LCUI_FileIsDirectory( entry ) ) {
			SyncTask_ScanFiles( t, filepath, fp );
			continue;
		}
		if( !LCUI_FileIsArchive( entry ) ) {
			continue;
		}
		for( i = 0; i < n; ++i ) {
			if( strstr( name, match_suffixs[i] ) ) {
				break;
			}
		}
		if( i >= n ) {
			continue;
		}
		/* 若该文件路径存在于之前的缓存中，说明未被删除，否则将之
		 * 视为新增的文件。
		 */
		if( Dict_FetchValue( ds->files, filepath ) ) {
			Dict_Delete( ds->deleted_files, filepath );
		} else {
			Dict_Add( ds->added_files, filepath, (void*)1 );
		}
		fputs( filepath, fp );
		fputc( '\n', fp );
		++t->count;
	}
	LCUI_CloseDir( &dir );
	return t->count;
}

int LCFinder_StartSyncFiles( SyncTask t )
{
	FILE *fp;
	char *tmpfile, *file;
	const char suffix[] = ".tmp";
	int len, n = strlen( t->dirpath );
	len = n + sizeof( filename ) / sizeof( char );
	file = malloc( len * sizeof( char ) );
	len += sizeof( suffix ) / sizeof( char );
	tmpfile = malloc( len * sizeof( char ) );
	strcpy( tmpfile, t->dirpath );
	if( tmpfile[n - 1] != '/' ) {
		tmpfile[n++] = '/';
		tmpfile[n] = 0;
	}
	sprintf( file, "%s%s", tmpfile, filename );
	fp = fopen( file, "r" );
	if( fp ) {
		SyncTask_LoadCache( t, fp );
		fclose( fp );
	}
	sprintf( tmpfile + n, "%s%s", filename, suffix );
	fp = fopen( tmpfile, "w" );
	if( !fp ) {
		return -1;
	}
	t->state = STATE_STARTED;
	n = SyncTask_ScanFiles( t, t->dirpath, fp );
	t->state = STATE_FINISHED;
	fclose( fp );
	remove( file );
	rename( tmpfile, file );
	return n;
}

void LCFinder_StopSync( SyncTask t )
{
	t->state = STATE_NONE;
}
