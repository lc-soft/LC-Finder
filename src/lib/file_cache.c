/* ***************************************************************************
* file_cache.c -- file list cache, it used for file list changes detect.
*
* Copyright (C) 2015 by Liu Chao <lc-soft@live.cn>
*
* This file is part of the LC-Finder project, and may only be used, modified,
* and distributed under the terms of the GPLv2.
*
* (GPLv2 is abbreviation of GNU General Public License Version 2)
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
* 版权所有 (C) 2015 归属于 刘超 <lc-soft@live.cn>
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

static const char filename[] = ".lc-finder-files.cache";
static const char *match_suffixs[] = {".png", ".bmp", ".jpg", ".jpeg"};

CacheTask NewCacheTask( const char *dirpath )
{
	CacheTask t = NEW( CacheTaskRec, 1 );
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

void DeleteCacheTask( CacheTask *tptr )
{
	CacheTask t = *tptr;
	if( t->dirpath ) {
		free( t->dirpath );
		t->dirpath = NULL;
	}
	free( t );
	*tptr = NULL;
}

/** 扫描文件 */
static int LCFinder_ScanFiles( CacheTask t, const char *dirpath, FILE *fp )
{
	LCUI_Dir dir;
	LCUI_DirEntry *entry;
	char filepath[2048], *name;
	int i, n, len = strlen( dirpath );
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
			LCFinder_ScanFiles( t, filepath, fp );
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
		fputs( filepath, fp );
		fputc( '\n', fp );
		++t->count;
	}
	LCUI_CloseDir( &dir );
	return t->count;
}

int LCFinder_StartCache( CacheTask t )
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
	sprintf( tmpfile + n, "%s%s", filename, suffix );
	fp = fopen( tmpfile, "w" );
	if( !fp ) {
		return -1;
	}
	t->state = STATE_STARTED;
	n = LCFinder_ScanFiles( t, t->dirpath, fp );
	t->state = STATE_FINISHED;
	fclose( fp );
	remove( file );
	rename( tmpfile, file );
	return n;
}

void LCFinder_StopCache( CacheTask t )
{
	t->state = STATE_NONE;
}
