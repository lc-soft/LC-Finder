/* ***************************************************************************
 * common.c -- common function set.
 *
 * Copyright (C) 2016-2017 by Liu Chao <lc-soft@live.cn>
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
 * 版权所有 (C) 2016-2017 归属于 刘超 <lc-soft@live.cn>
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
#include <math.h>
#include <time.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/font/charset.h>
#include "sha1.h"
#include "common.h"

char *EncodeUTF8( const wchar_t *wstr )
{
	int len = LCUI_EncodeString( NULL, wstr, 0, ENCODING_UTF8 ) + 1;
	char *str = malloc( len * sizeof( char ) );
	LCUI_EncodeString( str, wstr, len, ENCODING_UTF8 );
	str[len - 1] = 0;
	return str;
}

char *EncodeANSI( const wchar_t *wstr )
{
	int len;
	char *str;
	len = LCUI_EncodeString( NULL, wstr, 0, ENCODING_ANSI ) + 1;
	str = malloc( len * sizeof( char ) );
	LCUI_EncodeString( str, wstr, len, ENCODING_ANSI );
	str[len - 1] = 0;
	return str;
}

wchar_t *DecodeUTF8( const char *str )
{
	int len = strlen( str ) + 1;
	wchar_t *wstr = malloc( len * sizeof( wchar_t ) );
	LCUI_DecodeString( wstr, str, len, ENCODING_UTF8 );
	return wstr;
}

wchar_t *DecodeANSI( const char *str )
{
	int len = strlen( str ) + 1;
	wchar_t *wstr = malloc( len * sizeof( wchar_t ) );
	len = LCUI_DecodeString( wstr, str, len, ENCODING_ANSI );
	return wstr;
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

int IsImageFile( const wchar_t *path )
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
	char *newkey = malloc( (strlen( key ) + 1) * sizeof( char ) );
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

char *getdirname( const char *path )
{
	int i, len = strlen( path );
	char *dirname = malloc( sizeof(char) * len );
	for( i = len - 1; i >= 0; --i ) {
		if( path[i] == PATH_SEP ) {
			dirname[i] = 0;
			break;
		}
	}
	for( ; i >= 0; --i ) {
		dirname[i] = path[i];
	}
	return dirname;
}

wchar_t *wgetdirname( const wchar_t *path )
{
	int i, len = wcslen( path ) + 1;
	wchar_t *dirname = malloc( sizeof( wchar_t ) * len );
	for( i = len - 1, dirname[i] = 0; i >= 0; --i ) {
		if( path[i] == PATH_SEP ) {
			dirname[i] = 0;
			break;
		}
	}
	for( --i; i >= 0; --i ) {
		dirname[i] = path[i];
	}
	return dirname;
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

int wgetfilestat( const wchar_t *wpath, struct stat *buf )
{
	int ret;
	char *path;
	path = EncodeANSI( wpath );
	ret = stat( path, buf );
	free( path );
	return ret;
}

int pathjoin( char *path, const char *path1, const char *path2 )
{
	int len = strlen( path1 );
	if( path != path1 ) {
		strcpy( path, path1 );
	}
	if( path[len - 1] != PATH_SEP ) {
		path[len++] = PATH_SEP;
		path[len] = 0;
	}
	strcpy( path + len, path2 );
	len = strlen( path );
	if( path[len - 1] == PATH_SEP ) {
		--len;
		path[len] = 0;
	}
	return len;
}

int wpathjoin( wchar_t *path, const wchar_t *path1, const wchar_t *path2 )
{
	int len = wcslen( path1 );
	if( path != path1 ) {
		wcscpy( path, path1 );
	}
	if( path[len - 1] != PATH_SEP ) {
		path[len++] = PATH_SEP;
		path[len] = 0;
	}
	wcscpy( path + len, path2 );
	len = wcslen( path );
	if( path[len - 1] == PATH_SEP ) {
		--len;
		path[len] = 0;
	}
	return len;
}

int wgetcurdir( wchar_t *wpath, int max_len )
{
#ifdef _WIN32
	return GetCurrentDirectoryW( max_len, wpath );
#else
	int len;
	char *path = malloc( sizeof(char) * (max_len + 1) );
	getcwd( path, max_len );
	len = LCUI_DecodeString( wpath, path, max_len, ENCODING_UTF8 );
	free( path );
	return len;
#endif
}

int wmkdir( wchar_t *wpath )
{
#ifdef _WIN32
	return _wmkdir( wpath );
#else
	char *path = EncodeUTF8( wpath );
	int ret = mkdir( path, S_IRWXU );
	free( path );
	return ret;
#endif
}

int wchdir( wchar_t *wpath )
{
#ifdef _WIN32
	return _wchdir( wpath );
#else
	char *path = EncodeUTF8( wpath );
	int ret = chdir( path );
	free( path );
	return ret;
#endif
}

int wgetnumberstr( wchar_t *str, int max_len, size_t number )
{
	int right, j, k, len, buf_len, count;
	wchar_t *buf = malloc( sizeof( wchar_t ) * (max_len + 1) );
	len = swprintf( buf, max_len, L"%lu", number );
	count = (int)ceil( len / 3.0 - 1.0 );
	buf_len = len + count;
	max_len = max_len > buf_len ? buf_len : max_len;
	for( k = 1, right = max_len - 1, j = len - 1; right >= 0; --right ) {
		if( right < max_len - 1 && count > 0 && k % 4 == 0 ) {
			str[right] = L',';
			--count;
			k = 1;
		} else {
			str[right] = buf[j--];
			++k;
		}
	}
	str[max_len] = 0;
	free( buf );
	return len;
}

int wgettimestr( wchar_t *str, int max_len, time_t time )
{
	struct tm *t;
	wchar_t *days[7] = {
		L"星期一", L"星期二", L"星期三", L"星期四",
		L"星期五", L"星期六", L"星期天"
	};
	t = localtime( &time );
	if( !t ) {
		return -1;
	}
	return swprintf( str, max_len, L"%d年%d月%d日，%ls %d:%d",
			 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday,
			 days[t->tm_wday], t->tm_hour, t->tm_min );
}

int getsizestr( char *str, int64_t size )
{
	int i;
	double num;
	int64_t tmp, prev_tmp;
	char *units[5] = {"B", "KB", "MB", "GB", "TB"};

	if( size < 1024 ) {
		return sprintf( str, "%d%s", (int)size, units[0] );
	}
	for( i = 0, tmp = size; i < 5; ++i ) {
		if( tmp < 1024 ) {
			break;
		}
		prev_tmp = tmp;
		tmp = tmp / 1024;
	}
	num = 1.0 * prev_tmp / 1024.0;
	return sprintf( str, "%0.2f%s", num, units[i] );
}

int wgetsizestr( wchar_t *str, int max_len, int64_t size )
{
	int i;
	double num;
	int64_t tmp, prev_tmp;
	wchar_t *units[5] = {L"B", L"KB", L"MB", L"GB", L"TB"};

	if( size < 1024 ) {
		return swprintf( str, max_len, L"%d%ls", (int)size, units[0] );
	}
	for( i = 0, tmp = size; i < 5; ++i ) {
		if( tmp < 1024 ) {
			break;
		}
		prev_tmp = tmp;
		tmp = tmp / 1024;
	}
	num = 1.0 * prev_tmp / 1024.0;
	return swprintf( str, max_len, L"%0.2f%ls", num, units[i] );
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
