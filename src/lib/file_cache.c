/* ***************************************************************************
 * file_cache.c -- file list cache, it used for file list changes detection.
 *
 * Copyright (C) 2015-2017 by Liu Chao <lc-soft@live.cn>
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
 * 版权所有 (C) 2015-2017 归属于 刘超 <lc-soft@live.cn>
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
#define DEBUG
#include <errno.h>
#include <stdio.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/font/charset.h>
#include "unqlite.h"
#include "common.h"
#include "file_cache.h"

#define MAX_PATH_LEN	2048
#define WCSLEN(STR)	(sizeof( STR ) / sizeof( wchar_t ))
#define GetDirStats(T)	(DirStats)(((char*)(T)) + sizeof(SyncTaskRec))

 /** 文件状态信息 */
typedef struct FileStatusRec_ {
	unsigned int ctime;	/**< 创建时间 */
	unsigned int mtime;	/**< 修改时间 */
} FileStatusRec, *FileStatus;

/** 文件夹内的文件变更状态统计 */
typedef struct DirStatsRec_ {
	unqlite *db;
	Dict *files;		/**< 之前已缓存的文件列表 */
	Dict *added_files;	/**< 新增的文件 */
	Dict *changed_files;	/**< 已改变的文件 */
	Dict *deleted_files;	/**< 删除的文件 */
} DirStatsRec, *DirStats;

static unsigned int Dict_KeyHash( const wchar_t *buf )
{
	unsigned int hash = 5381;
	while( *buf ) {
		hash = ((hash << 5) + hash) + (*buf++);
	}
	return hash;
}

static int Dict_KeyCompare( void *privdata, const void *key1, const void *key2 )
{
	if( wcscmp( key1, key2 ) == 0 ) {
		return 1;
	}
	return 0;
}

static void *Dict_KeyDup( void *privdata, const void *key )
{
	size_t len;
	wchar_t *newkey;
	len = wcslen( key ) + 1;
	newkey = malloc( len * sizeof( wchar_t ) );
	if( !newkey ) {
		return NULL;
	}
	wcsncpy( newkey, key, len );
	return newkey;
}

static void Dict_KeyDestructor( void *privdata, void *key )
{
	free( key );
}

static void FilesDict_ValDestructor( void *privdata, void *val )
{
	free( val );
}

static DictType FilePathsDict = {
	Dict_KeyHash,
	Dict_KeyDup,
	NULL,
	Dict_KeyCompare,
	Dict_KeyDestructor,
	NULL
};

static DictType FilesDict = {
	Dict_KeyHash,
	Dict_KeyDup,
	NULL,
	Dict_KeyCompare,
	Dict_KeyDestructor,
	FilesDict_ValDestructor
};

SyncTask SyncTask_New( const char *data_dir, const char *scan_dir )
{
	SyncTask t;
	wchar_t *wcs_data_dir, *wcs_scan_dir;
	size_t len1 = strlen( data_dir ) + 1;
	size_t len2 = strlen( scan_dir ) + 1;
	wcs_data_dir = malloc( sizeof( wchar_t )*len1 );
	wcs_scan_dir = malloc( sizeof( wchar_t )*len2 );
	LCUI_DecodeString( wcs_data_dir, data_dir, len1, ENCODING_UTF8 );
	LCUI_DecodeString( wcs_scan_dir, scan_dir, len2, ENCODING_UTF8 );
	t = SyncTask_NewW( wcs_data_dir, wcs_scan_dir );
	free( wcs_data_dir );
	free( wcs_scan_dir );
	return t;
}

SyncTask SyncTask_NewW( const wchar_t *data_dir, const wchar_t *scan_dir )
{
	SyncTask t;
	DirStats ds;
	wchar_t name[44];
	size_t max_len, len1, len2;
	const wchar_t suffix[] = L".tmp";

	t = malloc( sizeof( SyncTaskRec ) + sizeof( DirStatsRec ) );
	ds = GetDirStats( t );
	len1 = wcslen( data_dir ) + 1;
	len2 = wcslen( scan_dir ) + 1;
	ds->files = Dict_Create( &FilesDict, NULL );
	ds->added_files = Dict_Create( &FilesDict, NULL );
	ds->changed_files = Dict_Create( &FilePathsDict, NULL );
	ds->deleted_files = Dict_Create( &FilePathsDict, NULL );
	t->data_dir = malloc( sizeof( wchar_t ) * len1 );
	t->scan_dir = malloc( sizeof( wchar_t ) * len2 );
	wcsncpy( t->data_dir, data_dir, len1 );
	wcsncpy( t->scan_dir, scan_dir, len2 );
	WEncodeSHA1( name, t->scan_dir, len2 );
	max_len = len1 + WCSLEN( name ) + WCSLEN( suffix ) + 1;
	t->tmpfile = malloc( max_len * sizeof( wchar_t ) );
	t->file = malloc( max_len * sizeof( wchar_t ) );
	wcsncpy( t->tmpfile, t->data_dir, len1 );
	wpathjoin( t->file, data_dir, name );
	swprintf( t->tmpfile, max_len, L"%ls%ls", t->file, suffix );
	t->state = STATE_NONE;
	t->changed_files = 0;
	t->deleted_files = 0;
	t->total_files = 0;
	t->added_files = 0;
	return t;
}

void SyncTask_ClearCache( SyncTask t )
{
#ifdef _WIN32
	_wremove( t->file );
	_wremove( t->tmpfile );
#else
	char *file = EncodeUTF8( t->file );
	char *tmpfile = EncodeUTF8( t->tmpfile );
	remove( file );
	remove( tmpfile );
	free( file );
	free( tmpfile );
#endif
}

void SyncTask_Delete( SyncTask t )
{
	DirStats ds = GetDirStats( t );
	free( t->scan_dir );
	free( t->data_dir );
	free( t->file );
	free( t->tmpfile );
	t->file = NULL;
	t->tmpfile = NULL;
	t->scan_dir = NULL;
	t->data_dir = NULL;
	Dict_Release( ds->files );
	Dict_Release( ds->added_files );
	Dict_Release( ds->changed_files );
	Dict_Release( ds->deleted_files );
	free( t );
}

static int FileDict_ForEach( Dict *d, FileInfoHanlder func, void *data )
{
	int count = 0;
	DictEntry *entry;
	DictIterator *iter = Dict_GetIterator( d );
	while( (entry = Dict_Next( iter )) ) {
		func( data, DictEntry_GetVal( entry ) );
		++count;
	}
	Dict_ReleaseIterator( iter );
	return count;
}

int SyncTask_InAddedFiles( SyncTask t, FileInfoHanlder func, void *func_data )
{
	DirStats ds = GetDirStats( t );
	return FileDict_ForEach( ds->added_files, func, func_data );
}

int SyncTask_InChangedFiles( SyncTask t, FileInfoHanlder func, void *func_data )
{
	DirStats ds = GetDirStats( t );
	return FileDict_ForEach( ds->changed_files, func, func_data );
}

int SyncTask_InDeletedFiles( SyncTask t, FileInfoHanlder func, void *func_data )
{
	DirStats ds = GetDirStats( t );
	return FileDict_ForEach( ds->deleted_files, func, func_data );
}

int SyncTask_OpenCacheW( SyncTask t, const wchar_t *path )
{
	DirStats ds;
	int len, rc;
	char *dbfile;

	ds = GetDirStats( t );
	path = path ? path : t->file;
	len = LCUI_EncodeString( NULL, path, 0, ENCODING_UTF8 ) + 1;
	dbfile = malloc( sizeof( char )*len );
	if( !dbfile ) {
		return -ENOMEM;
	}
	LCUI_EncodeString( dbfile, path, len, ENCODING_UTF8 );
	rc = unqlite_open( &ds->db, dbfile, UNQLITE_OPEN_CREATE );
	free( dbfile );
	if( rc != UNQLITE_OK ) {
		return rc;
	}
	return 0;
}

void SyncTask_CloseCache( SyncTask t )
{
	DirStats ds = GetDirStats( t );
	unqlite_close( ds->db );
}

/** 从缓存记录中载入文件列表 */
static int SyncTask_LoadCache( SyncTask t )
{
	int rc, count;
	wchar_t buf[MAX_PATH_LEN];
	unqlite_kv_cursor *cur;
	FileStatusRec status;
	DirStats ds;

	ds = GetDirStats( t );
	SyncTask_OpenCacheW( t, t->file );
	rc = unqlite_kv_cursor_init( ds->db, &cur );
	if( rc != UNQLITE_OK ) {
		unqlite_close( ds->db );
		return 0;
	}
	rc = unqlite_kv_cursor_first_entry( cur );
	if( rc != UNQLITE_OK ) {
		unqlite_kv_cursor_release( ds->db, cur );
		unqlite_close( ds->db );
		return 0;
	}
	count = 0;
	while( unqlite_kv_cursor_valid_entry( cur ) ) {
		int key_size = MAX_PATH_LEN;
		FileInfo info = NEW( FileInfoRec, 1 );
		unqlite_int64 data_size = sizeof( FileStatusRec );
		unqlite_kv_cursor_key( cur, buf, &key_size );
		unqlite_kv_cursor_data( cur, &status, &data_size );
		unqlite_kv_cursor_next_entry( cur );
		info->path = malloc( key_size + sizeof( wchar_t ) );
		info->mtime = status.mtime;
		info->ctime = status.ctime;
		key_size /= sizeof( wchar_t );
		buf[(int)key_size] = 0;
		wcsncpy( info->path, buf, key_size + 1 );
		Dict_Add( ds->files, info->path, info );
		Dict_Add( ds->deleted_files, info->path, info );
		++count;
	}
	unqlite_kv_cursor_release( ds->db, cur );
	unqlite_close( ds->db );
	t->deleted_files = count;
	return count;
}

int SyncTask_AddFileW( SyncTask t, const wchar_t *path,
		       unsigned int ctime, unsigned int mtime )
{
	size_t len;
	FileInfo info;
	FileStatusRec status;
	DirStats ds = GetDirStats( t );
	if( t->state != STATE_STARTED ) {
		return -1;
	}
	len = wcslen( path );
	/* 若该文件路径存在于之前的缓存中，说明未被删除，否则将之
	 * 视为新增的文件。
	 */
	info = Dict_FetchValue( ds->files, path );
	if( info ) {
		if( ctime != info->ctime || mtime != info->mtime ) {
			info->ctime = ctime;
			info->mtime = mtime;
			Dict_Add( ds->changed_files, info->path, info );
			DEBUG_MSG( "changed file: %ls\n", path );
			++t->changed_files;
		} else {
			DEBUG_MSG( "unchanged file: %ls\n", path );
		}
		Dict_Delete( ds->deleted_files, path );
		--t->deleted_files;
	} else {
		info = NEW( FileInfoRec, 1 );
		info->path = NEW( wchar_t, len + 1 );
		wcsncpy( info->path, path, len + 1 );
		info->ctime = ctime;
		info->mtime = mtime;
		Dict_Add( ds->added_files, info->path, info );
		DEBUG_MSG( "added file: %ls\n", path );
		++t->added_files;
	}
	status.ctime = info->ctime;
	status.mtime = info->mtime;
	len = sizeof( wchar_t ) / sizeof( char ) * len;
	unqlite_kv_store( ds->db, path, len, &status,
			  sizeof( FileStatusRec ) );
	++t->total_files;
	return 0;
}

int SyncTask_ScanFileW( SyncTask t, const wchar_t *path )
{
	int rc, len;
	struct stat buf;
	FileInfo info;
	FileStatusRec status;
	DirStats ds = GetDirStats( t );
	if( t->state != STATE_STARTED ) {
		return -1;
	}
	rc = wgetfilestat( path, &buf );
	if( rc != 0 ) {
		return 0;
	}
	len = wcslen( path );
	/* 若该文件路径存在于之前的缓存中，说明未被删除，否则将之
	 * 视为新增的文件。
	 */
	info = Dict_FetchValue( ds->files, path );
	if( info ) {
		if( buf.st_ctime != (time_t)info->ctime ||
		    buf.st_mtime != (time_t)info->mtime ) {
			info->ctime = (uint32_t)buf.st_ctime;
			info->mtime = (uint32_t)buf.st_mtime;
			Dict_Add( ds->changed_files, info->path, info );
			DEBUG_MSG( "changed file: %ls\n", path );
			++t->changed_files;
		} else {
			DEBUG_MSG( "unchanged file: %ls\n", path );
		}
		Dict_Delete( ds->deleted_files, path );
		--t->deleted_files;
	} else {
		info = NEW( FileInfoRec, 1 );
		info->path = NEW( wchar_t, len + 1 );
		wcsncpy( info->path, path, len + 1 );
		info->ctime = (uint32_t)buf.st_ctime;
		info->mtime = (uint32_t)buf.st_mtime;
		Dict_Add( ds->added_files, info->path, info );
		DEBUG_MSG( "added file: %ls\n", path );
		++t->added_files;
	}
	status.ctime = info->ctime;
	status.mtime = info->mtime;
	len = sizeof( wchar_t ) / sizeof( char ) * len;
	unqlite_kv_store( ds->db, path, len, &status,
			  sizeof( FileStatusRec ) );
	++t->total_files;
	return 0;
}

/** 扫描文件 */
static int SyncTask_ScanFilesW( SyncTask t, const wchar_t *dirpath )
{
	DirStats ds;
	LCUI_Dir dir;
	LCUI_DirEntry *entry;
	wchar_t *name, path[MAX_PATH_LEN];
	int len, dir_len;

	ds = GetDirStats( t );
	dir_len = wcslen( dirpath );
	wcscpy( path, dirpath );
	if( path[dir_len - 1] != PATH_SEP ) {
		path[dir_len++] = PATH_SEP;
		path[dir_len] = 0;
	}
	if( LCUI_OpenDirW( path, &dir ) != 0 ) {
		return 0;
	}
	while( (entry = LCUI_ReadDirW( &dir )) && t->state == STATE_STARTED ) {
		name = LCUI_GetFileNameW( entry );
		DEBUG_MSG( "scan file: %ls\n", name );
		/* 忽略 . 和 .. 文件夹 */
		if( name[0] == '.' ) {
			if( name[1] == 0 || (name[1] == '.' && name[2] == 0) ) {
				continue;
			}
		}
		wcscpy( path + dir_len, name );
		len = wcslen( path );
		if( LCUI_FileIsDirectory( entry ) ) {
			SyncTask_ScanFilesW( t, path );
			continue;
		}
		if( !LCUI_FileIsArchive( entry ) ) {
			continue;
		}
		if( !IsImageFile( name ) ) {
			continue;
		}
		SyncTask_ScanFileW( t, path );
	}
	LCUI_CloseDir( &dir );
	return t->total_files;
}

int SyncTask_DeleteFileW( SyncTask t, const wchar_t *filepath )
{
	DirStats ds = GetDirStats( t );
	size_t size = sizeof( wchar_t ) * wcslen( filepath );
	int rc = unqlite_kv_delete( ds->db, filepath, size );
	if( rc == UNQLITE_OK ) {
		return 0;
	}
	return -1;
}

int SyncTask_Start( SyncTask t )
{
	SyncTask_LoadCache( t );
	if( 0 != SyncTask_OpenCacheW( t, t->tmpfile ) ) {
		return -1;
	}
	t->state = STATE_STARTED;
	return 0;
}

void SyncTask_Finish( SyncTask t )
{
	t->state = STATE_FINISHED;
	SyncTask_CloseCache( t );
}

void SyncTask_Commit( SyncTask t )
{
	DirStats ds = GetDirStats( t );
#ifdef _WIN32
	unqlite_close( ds->db );
	_wremove( t->file );
	_wrename( t->tmpfile, t->file );
#else
	char *file = EncodeUTF8( t->file );
	char *tmpfile = EncodeUTF8( t->tmpfile );
	unqlite_close( ds->db );
	remove( file );
	rename( tmpfile, file );
	free( file );
	free( tmpfile );
#endif
}
