/* ***************************************************************************
* common.c -- common function set.
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
* common.c -- 一些通用的基础功能集
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
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include "sha1.h"
#include "common.h"

void EncodeSHA1( char *hash_out, const char *str, int len )
{
	int i;
	SHA1_CTX ctx;
	uint8_t results[20];
	char elem[4];

	SHA1Init( &ctx );
	SHA1Update( &ctx, (unsigned char*)str, len );
	SHA1Final( results, &ctx );
	hash_out[0] = 0;
	for( i = 0; i < 20; ++i ) {
		sprintf( elem, "%02x", results[i] );
		strcat( hash_out, elem );
	}
}

void WEncodeSHA1( wchar_t *hash_out, const wchar_t *wstr, int len )
{
	int i;
	SHA1_CTX ctx;
	uint8_t results[20];
	wchar_t elem[4];

	SHA1Init( &ctx );
	len *= sizeof( wchar_t ) / sizeof( unsigned char );
	SHA1Update( &ctx, (unsigned char*)wstr, len );
	SHA1Final( results, &ctx );
	hash_out[0] = 0;
	for( i = 0; i < 20; ++i ) {
		swprintf( elem, 4, L"%02x", results[i] );
		wcscat( hash_out, elem );
	}
}

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

const char *getfilename( const char *path )
{
	int i;
	const char *p = path;
	for( i = 0; path[i]; ++i ) {
		if( path[i] == PATH_SEP ) {
			p = path + i + 1;
		}
	}
	return p;
}

const wchar_t *wgetfilename( const wchar_t *path )
{
	int i;
	const wchar_t *p = path;
	for( i = 0; path[i]; ++i ) {
		if( path[i] == PATH_SEP ) {
			p = path + i + 1;
		}
	}
	return p;
}

int wgetdirpath( wchar_t *outpath, int max_len, const wchar_t *inpath )
{
	int i, pos;
	const wchar_t *p = inpath;
	for( i = 0, pos = -1; inpath[i] && i < max_len; ++i ) {
		outpath[i] = inpath[i];
		if( outpath[i] == PATH_SEP ) {
			pos = i;
		}
	}
	if( pos > 0 ) {
		outpath[pos] = 0;
		return pos + 1;
	}
	return i;
}

time_t wgetfilectime( const wchar_t *path )
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

time_t wgetfilemtime( const wchar_t *path )
{
	struct stat buf;
	int fd, ctime = 0;
	fd = _wopen( path, _O_RDONLY );
	if( fstat( fd, &buf ) == 0 ) {
		ctime = (int)buf.st_mtime;
	}
	_close( fd );
	return ctime;
}

int64_t wgetfilesize( const wchar_t *path )
{
	int fd;
	int64_t size;
	struct stat buf;
	fd = _wopen( path, _O_RDONLY );
	if( fd > 0 && fstat( fd, &buf ) == 0 ) {
		size = buf.st_size;
	} else {
		size = 0;
	}
	_close( fd );
	return size;
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

void wgetcurdir( wchar_t *path, int max_len )
{
#ifdef _WIN32
	GetCurrentDirectoryW( max_len, path );
#endif
}

void wopenbrowser( const wchar_t *url )
{
#ifdef _WIN32
	ShellExecuteW( NULL, L"open", url, NULL, NULL, SW_SHOW );
#endif
}

int wgettimestr( wchar_t *str, int max_len, time_t time )
{
	struct tm *t;
	wchar_t *days[7] = {
		L"星期一", L"星期二", L"星期三", L"星期四",
		L"星期五", L"星期六", L"星期天"
	};
	t = localtime( &time );
	return swprintf( str, max_len, L"%d年%d月%d日，%s %d:%d",
			 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday,
			 days[t->tm_wday], t->tm_hour, t->tm_min );
}

int getsizestr( wchar_t *str, int max_len, int64_t size )
{
	int i;
	double num;
	int64_t tmp, prev_tmp;
	wchar_t *units[5] = {L"B", L"KB", L"MB", L"GB", L"TB"};

	if( size < 1024 ) {
		return swprintf( str, max_len, L"%d%s", (int)size, units[0] );
	}
	for( i = 0, tmp = size; i < 5; ++i ) {
		if( tmp < 1024 ) {
			break;
		}
		prev_tmp = tmp;
		tmp = tmp / 1024;
	}
	num = 1.0 * prev_tmp / 1024.0;
	return swprintf( str, max_len, L"%0.2f%s", num, units[i] );
}

int wgetcharcount( const wchar_t *wstr, const wchar_t *chars )
{
	int i, j, count;
	for( count = i = 0; wstr[i]; ++i ) {
		for( j = 0; chars[j]; ++j ) {
			if( wstr[i] == chars[j] ) {
				++count;
			}
		}
	}
	return count;
}
