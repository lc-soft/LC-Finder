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

#include <stdio.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/font/charset.h>
#include "common.h"
#include "file_cache.h"

#define MAX_PATH_LEN	2048
#define FILE_HEAD_TAG	"[LC-Finder Files Cache]"
#define WCSLEN(STR)	(sizeof( STR ) / sizeof( wchar_t ))
#define GetDirStats(T)	(DirStats)(((char*)(T)) + sizeof(SyncTaskRec))

/** 文件夹内的文件变更状态统计 */
typedef struct DirStatsRec_ {
	Dict *files;		/**< 之前已缓存的文件列表 */
	Dict *added_files;	/**< 新增的文件 */
	Dict *deleted_files;	/**< 删除的文件 */
} DirStatsRec, *DirStats;

/** 忽略大小写对比宽字符串 */
static int wcscasecmp( const wchar_t *str1, const wchar_t *str2 )
{
	wchar_t ch1 = 0, ch2 = 0;
	const wchar_t *p1 = str1, *p2 = str2;
	while( *p1 && *p2 ) {
		ch1 = *p1;
		ch2 = *p2;
		if( ch1 >= L'a' && ch1 <= L'z' ) {
			ch1 = ch1 - 32;
		}
		if( ch2 >= L'a' && ch2 <= L'z' ) {
			ch2 = ch2 - 32;
		}
		if( ch1 != ch2 ) {
			break;
		}
		++p1;
		++p2;
	}
	return ch1 - ch2;
}

static LCUI_BOOL IsImageFile( const wchar_t *path )
{
	int i;
	const wchar_t *p, *suffixs[] = {L"png", L"bmp", L"jpg", L"jpeg"};

	for( p = path; *p; ++p );
	for( --p; p != path; --p ) {
		if( *p == L'.' ) {
			break;
		}
	}
	if( *p != L'.' ) {
		return FALSE;
	}
	++p;
	for( i = 0; i < 4; ++i ) {
		if( wcscasecmp( p, suffixs[i] ) == 0 ) {
			return TRUE;
		}
	}
	return FALSE;
}

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

static DictType DictType_Files = {
	Dict_KeyHash,
	Dict_KeyDup,
	NULL,
	Dict_KeyCompare,
	Dict_KeyDestructor,
	NULL
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
	size_t len, len1, len2;
	const wchar_t suffix[] = L".tmp";

	t = malloc( sizeof( SyncTaskRec ) + sizeof( DirStatsRec ) );
	ds = GetDirStats( t );
	len1 = wcslen( data_dir ) + 1;
	len2 = wcslen( scan_dir ) + 1;
	ds->deleted_files = Dict_Create( &DictType_Files, NULL );
	ds->added_files = Dict_Create( &DictType_Files, NULL );
	ds->files = Dict_Create( &DictType_Files, NULL );
	t->data_dir = malloc( sizeof( wchar_t ) * len1 );
	t->scan_dir = malloc( sizeof( wchar_t ) * len2 );
	wcsncpy( t->data_dir, data_dir, len1 );
	wcsncpy( t->scan_dir, scan_dir, len2 );
	WEncodeSHA1( name, t->scan_dir, len2 );
	len = len1 + WCSLEN(name) + WCSLEN( suffix );
	t->tmpfile = malloc( len * sizeof( wchar_t ) );
	t->file = malloc( len * sizeof( wchar_t ) );
	wcsncpy( t->tmpfile, t->data_dir, len1 );
	if( t->tmpfile[len1 - 1] != PATH_SEP ) {
		t->tmpfile[len1++] = PATH_SEP;
		t->tmpfile[len1] = 0;
	}
	swprintf( t->file, len, L"%s%s", t->tmpfile, name );
	swprintf( t->tmpfile, len, L"%s%s", t->file, suffix );
	t->state = STATE_NONE;
	t->deleted_files = 0;
	t->total_files = 0;
	t->added_files = 0;
	return t;
}

void SyncTask_ClearCache( SyncTask t )
{
	_wremove( t->file );
	_wremove( t->tmpfile );
}

void SyncTask_Delete( SyncTask *tptr )
{
	SyncTask t = *tptr;
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
	Dict_Release( ds->deleted_files );
	free( t );
	*tptr = NULL;
}

static int FileDict_ForEach( Dict *d, FileHanlder func, void *data )
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

int SyncTask_InAddedFiles( SyncTask t, FileHanlder func, void *func_data )
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
	wchar_t *path;
	char buf[MAX_PATH_LEN];
	int count = 0, len, size;
	DirStats ds = GetDirStats( t );
	if( !fgets( buf, MAX_PATH_LEN, fp ) ) {
		return 0;
	}
	if( !strstr( buf, FILE_HEAD_TAG ) ) {
		return 0;
	}
	while( !feof( fp ) ) {
		size = fread( &len, sizeof( int ), 1, fp );
		if( size < 1 || len < 1 ) {
			break;
		}
		size = fread( buf, sizeof( wchar_t ), len, fp );
		if( size < len || size > MAX_PATH_LEN ) {
			((wchar_t*)buf)[size] = 0;
			break;
		}
		path = (wchar_t*)buf;
		path[len] = 0;
		Dict_Add( ds->files, path, (void*)1 );
		Dict_Add( ds->deleted_files, path, (void*)1 );
		++count;
	}
	t->deleted_files = count;
	return count;
}

/** 扫描文件 */
static int SyncTask_ScanFilesW( SyncTask t, const wchar_t *dirpath, FILE *fp )
{
	LCUI_Dir dir;
	LCUI_DirEntry *entry;
	wchar_t filepath[2048], *name;
	int i, len, dir_len = wcslen( dirpath );
	DirStats ds = GetDirStats( t );
	wcscpy( filepath, dirpath );
	if( filepath[dir_len - 1] != PATH_SEP ) {
		filepath[dir_len++] = PATH_SEP;
		filepath[dir_len] = 0;
	}
	LCUI_OpenDir( filepath, &dir );
	while( (entry = LCUI_ReadDir( &dir )) && t->state == STATE_STARTED ) {
		name = LCUI_GetFileName( entry );
		/* 忽略 . 和 .. 文件夹 */
		if( name[0] == '.' ) {
			if( name[1] == 0 || (name[1] == '.' && name[2] == 0) ) {
				continue;
			}
		}
		wcscpy( filepath + dir_len, name );
		len = wcslen( filepath );
		if( LCUI_FileIsDirectory( entry ) ) {
			SyncTask_ScanFilesW( t, filepath, fp );
			continue;
		}
		if( !LCUI_FileIsArchive( entry ) ) {
			continue;
		}
		if( !IsImageFile( name ) ) {
			continue;
		}
		/* 若该文件路径存在于之前的缓存中，说明未被删除，否则将之
		 * 视为新增的文件。
		 */
		if( Dict_FetchValue( ds->files, filepath ) ) {
			Dict_Delete( ds->deleted_files, filepath );
			--t->deleted_files;
			//wprintf(L"unchange: %s\n", filepath);
		} else {
			//wprintf(L"added: %s\n", filepath);
			Dict_Add( ds->added_files, filepath, (void*)1 );
			++t->added_files;
		}
		fwrite( &len, sizeof( int ), 1, fp );
		i = fwrite( filepath, sizeof( wchar_t ), len, fp );
		//wprintf(L"scan file: %s, len: %d, writed: %d\n", filepath, len, i);
		++t->total_files;
	}
	LCUI_CloseDir( &dir );
	return t->total_files;
}

int SyncTask_Start( SyncTask t )
{
	int n;
	FILE *fp;
	//wprintf( L"\n\nscan dir: %s\n", t->scan_dir );
	fp = _wfopen( t->file, L"rb" );
	if( fp ) {
		SyncTask_LoadCache( t, fp );
		fclose( fp );
	}
	fp = _wfopen( t->tmpfile, L"wb" );
	if( !fp ) {
		return -1;
	}
	t->state = STATE_STARTED;
	fputs( FILE_HEAD_TAG"\n", fp );
	n = SyncTask_ScanFilesW( t, t->scan_dir, fp );
	t->state = STATE_FINISHED;
	fclose( fp );
	return n;
}

void SyncTask_Commit( SyncTask t )
{
	_wremove( t->file );
	_wrename( t->tmpfile, t->file );
}

void LCFinder_StopSync( SyncTask t )
{
	t->state = STATE_NONE;
}
