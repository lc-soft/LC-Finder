/* ***************************************************************************
 * common.h -- common function set.
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
 * common.h -- 一些通用的基础功能集
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

#ifndef LCFINDER_COMMON_H
#define LCFINDER_COMMON_H

#define PATH_LEN 2048

#ifdef _WIN32
#include <io.h>
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define PATH_SEP '/'
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <wchar.h>
#include <stdint.h>

LCUI_BEGIN_HEADER

char *EncodeUTF8( const wchar_t *wstr );

char *EncodeANSI( const wchar_t *wstr );

wchar_t *DecodeUTF8( const char *str );

void EncodeSHA1( char *hash_out, const char *str, int len );

void WEncodeSHA1( wchar_t *hash_out, const wchar_t *wstr, int len );

const char *getdirname( const char *path );

const char *getfilename( const char *path );

const wchar_t *wgetfilename( const wchar_t *path );

int wgetfilestat( const wchar_t *wpath, struct stat *buf );

int pathjoin( char *path, const char *path1, const char *path2 );

int wpathjoin( wchar_t *path, const wchar_t *path1, const wchar_t *path2 );

/** 获取程序当前所在目录 */
int wgetcurdir( wchar_t *path, int max_len );

int wmkdir( wchar_t *wpath );

Dict *StrDict_Create( void *(*val_dup)(void*, const void*),
		      void (*val_del)(void*, void*) );

void StrDict_Release( Dict *d );

/** 打开浏览器 */
void wopenbrowser( const wchar_t *url );

void wopenfilemanger( const wchar_t *filepath );

/** 获取数字字符串，格式为：1,234,567,890 */
int wgetnumberstr( wchar_t *str, int max_len, size_t number );

int wgettimestr( wchar_t *str, int max_len, time_t time );

int getsizestr( char *str, int64_t size );

int wgetsizestr( wchar_t *str, int max_len, int64_t size );

int wgetdirpath( wchar_t *outpath, int max_len, const wchar_t *inpath );

/** 统计一个字符串中的指定字符的数量 */
int wgetcharcount( const wchar_t *wstr, const wchar_t *chars );

/** 忽略大小写对比宽字符串 */
int wcscasecmp( const wchar_t *str1, const wchar_t *str2 );

/** 移动文件至回收站 */
int wmovefiletotrash( const wchar_t *wfilepath );

int movefiletotrash( const char *filepath );

LCUI_END_HEADER

#endif
