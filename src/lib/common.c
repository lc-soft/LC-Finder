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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/font/charset.h>
#include "sha1.h"
#include "common.h"

static char *EncodeUTF8( const wchar_t *wstr )
{
	int len = LCUI_EncodeString( NULL, wstr, 0, ENCODING_UTF8 ) + 1;
	char *str = malloc( len * sizeof(wchar_t) );
	LCUI_EncodeString( str, wstr, len, ENCODING_UTF8 );
	return str;
}

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

static void Dict_KeyDestructor( void *privdata, void *key )
{
	free( key );
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
	void *privdata = d->privdata;
	Dict_Release( d );
	free( privdata );
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

time_t wgetfilectime( const wchar_t *wpath )
{
	int fd;
	time_t ctime = 0;
	struct stat buf;
#ifdef _WIN32
	fd = _wopen( path, _O_RDONLY );
	if( fd > 0 ) {
		if( fstat( fd, &buf ) == 0 ) {
			ctime = buf.st_ctime;
		}
		_close( fd );
	}
#else
	char *path = EncodeUTF8( wpath );
	fd = open( path, O_RDONLY);
	if( fd > 0 ) {
		if( fstat( fd, &buf ) == 0 ) {
			ctime = buf.st_ctime;
		}
		close( fd );
	}
	free( path );
#endif
	return ctime;
}

time_t wgetfilemtime( const wchar_t *wpath )
{
	int fd;
	time_t mtime = 0;
	struct stat buf;
#ifdef _WIN32
	fd = _wopen( path, _O_RDONLY );
	if( fd > 0 ) {
		if( fstat( fd, &buf ) == 0 ) {
			mtime = buf.st_mtime;
		}
		_close( fd );
	}
#else
	char *path = EncodeUTF8( wpath );
	fd = open( path, O_RDONLY);
	if( fd > 0 ) {
		if( fstat( fd, &buf ) == 0 ) {
			mtime = buf.st_mtime;
		}
		close( fd );
	}
	free( path );
#endif
	return mtime;
}

int64_t wgetfilesize( const wchar_t *wpath )
{
	int fd;
	int64_t size = 0;
	struct stat buf;
#ifdef _WIN32
	fd = _wopen( path, _O_RDONLY );
	if( fd > 0 ) {
		if( fstat( fd, &buf ) == 0 ) {
			size = buf.st_size;
		}
		_close( fd );
	}
#else
	char *path = EncodeUTF8( wpath );
	fd = open( path, O_RDONLY);
	if( fd > 0 ) {
		if( fstat( fd, &buf ) == 0 ) {
			size = buf.st_size;
		}
		close( fd );
	}
	free( path );
#endif
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

void wgetcurdir( wchar_t *wpath, int max_len )
{
#ifdef _WIN32
	GetCurrentDirectoryW( max_len, path );
#else
	char *path = malloc( sizeof(char) * (max_len + 1) );
	getcwd( path, max_len );
	LCUI_DecodeString( wpath, path, max_len, ENCODING_UTF8 );
	free( path );
#endif
}

void wopenbrowser( const wchar_t *url )
{
#ifdef _WIN32
	ShellExecuteW( NULL, L"open", url, NULL, NULL, SW_SHOW );
#endif
}

void wopenfilemanger( const wchar_t *filepath )
{
#ifdef _WIN32
	wchar_t args[4096];
	swprintf( args, 4095, L"/select,\"%s\"", filepath );
	ShellExecuteW( NULL, L"open", L"explorer.exe", args, NULL, SW_SHOW );
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
	return swprintf( str, max_len, L"%d年%d月%d日，%S %d:%d",
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

int wcscasecmp( const wchar_t *str1, const wchar_t *str2 )
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

int wmovefiletotrash( const wchar_t *wfilepath )
{
#ifdef _WIN32
	int ret;
	SHFILEOPSTRUCT sctFileOp = {0};
	size_t len = wcslen( wfilepath ) + 2;
	wchar_t *path = malloc( sizeof( wchar_t ) * len );
	wcsncpy( path, wfilepath, len );
	path[len - 1] = 0;
	sctFileOp.pTo = NULL;
	sctFileOp.pFrom = path;
	sctFileOp.wFunc = FO_DELETE;
	sctFileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
	ret = SHFileOperationW( &sctFileOp );
	free( path );
	return ret;
#else
	return -1;
#endif
}

int movefiletotrash( const char *filepath )
{
	int ret, len = strlen( filepath ) + 1;
	wchar_t *wfilepath = malloc( sizeof( wchar_t ) * len );
	LCUI_DecodeString( wfilepath, filepath, len, ENCODING_UTF8 );
	ret = wmovefiletotrash( wfilepath );
	free( wfilepath );
	return ret;
}
