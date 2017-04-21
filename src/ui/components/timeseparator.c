/* ***************************************************************************
 * timeseparator.c -- time separator, used to separate the file list by time
 * range.
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
 * timeseparator.c -- 时间分割器，用于将文件列表按时间区间分离开来
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

#include <time.h>
#include <stdio.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "textview_i18n.h"
#include "timeseparator.h"
#include "i18n.h"

#define KEY_YEAR_FORMAT		"datetime.year_format"
#define KEY_MONTH_FORMAT	"datetime.month_format"
#define KEY_STATS		"home.pictures_count_stats"
#define KEY_MONTHS		"datetime.months"

 /** 时间分割线功能的数据 */
typedef struct TimeSeparatorRec_ {
	int count;		/**< 当前时间段内的记录数量 */
	struct tm time;		/**< 当前时间段的起始时间 */
	struct tm end_time;	/**< 当前时间段的结束时间 */
	LCUI_Widget subtitle;	/**< 子标题 */
	LCUI_Widget title;	/**< 标题 */
} TimeSeparatorRec, *TimeSeparator;

static LCUI_WidgetPrototype prototype = NULL;

static int FormatYearString( wchar_t *str, int max_len, struct tm *t )
{
	int n;
	char key[64], month_key[4];
	const wchar_t *format, *month_str;
	wchar_t buf[256], year_str[6], mday_str[4];
	format = I18n_GetText( KEY_YEAR_FORMAT );
	if( !format ) {
		wcscpy( str, L"<translation missing>" );
		return 0;
	}
	wcscpy( buf, format );
	swprintf( year_str, 5, L"%d", 1900 + t->tm_year );
	swprintf( mday_str, 3, L"%d", t->tm_mday );
	snprintf( month_key, 3, "%d", t->tm_mon );
	snprintf( key, 63, KEY_MONTHS".%s", month_key );
	month_str = I18n_GetText( key );
	if( !month_str ) {
		wcscpy( str, L"<translation missing>" );
		return 0;
	}
	n = wcsreplace( buf, 255, L"YYYY", year_str );
	n += wcsreplace( buf, 255, L"MM", month_str );
	wcsncpy( str, buf, max_len );
	return n;
}

static int FormatMonthString( wchar_t *str, int max_len, struct tm *t )
{
	int n;
	char key[64], month_key[4];
	const wchar_t *format, *month_str;
	wchar_t buf[256], mday_str[4];
	format = I18n_GetText( KEY_MONTH_FORMAT );
	if( !format ) {
		wcscpy( str, L"<translation missing>" );
		return 0;
	}
	wcscpy( buf, format );
	swprintf( mday_str, 3, L"%d", t->tm_mday );
	snprintf( month_key, 3, "%d", t->tm_mon );
	snprintf( key, 63, KEY_MONTHS".%s", month_key );
	month_str = I18n_GetText( key );
	if( !month_str ) {
		wcscpy( str, L"<translation missing>" );
		return 0;
	}
	n = wcsreplace( buf, 255, L"MM", month_str );
	n += wcsreplace( buf, 255, L"DD", mday_str );
	wcsncpy( str, buf, max_len );
	return n;
}

static void RenderTitle( wchar_t *buf, const wchar_t *text, void *privdata )
{
	TimeSeparator sep = privdata;
	if( !text ) {
		wcscpy( buf, L"<translation missing>" );
		return;
	}
	FormatYearString( buf, TXTFMT_BUF_MAX_LEN - 1, &sep->time );
}

static void RenderSubtitle( wchar_t *buf, const wchar_t *text, void *privdata )
{
	TimeSeparator sep = privdata;
	wchar_t start_str[64], end_str[64], stats[64];
	if( !text ) {
		wcscpy( buf, L"<translation missing>" );
		return;
	}
	wcscpy( end_str, text );
	wcscpy( start_str, text );
	text = I18n_GetText( KEY_STATS );
	if( !text ) {
		wcscpy( buf, L"<translation missing>" );
		return;
	}
	swprintf( stats, 63, text, sep->count );
	FormatMonthString( start_str, 63, &sep->time );
	/* 如果时间跨度超过一天 */
	if( sep->end_time.tm_year != sep->time.tm_year ||
	    sep->end_time.tm_mon != sep->time.tm_mon ||
	    sep->end_time.tm_mday != sep->time.tm_mday ) {
		FormatMonthString( end_str, 63, &sep->end_time );
		swprintf( buf, TXTFMT_BUF_MAX_LEN - 1, L"%s - %s %s",
			  start_str, end_str, stats );
		return;
	}
	swprintf( buf, TXTFMT_BUF_MAX_LEN - 1, L"%s %s", start_str, stats );
}

static void TimeSeparator_OnInit( LCUI_Widget w )
{
	const size_t data_size = sizeof( TimeSeparatorRec );
	TimeSeparator sep = Widget_AddData( w, prototype, data_size );

	sep->count = 0;
	sep->time.tm_mon = 0;
	sep->time.tm_mday = 0;
	sep->time.tm_year = 0;
	sep->title = LCUIWidget_New( "textview-i18n" );
	sep->subtitle = LCUIWidget_New( "textview-i18n" );
	TextViewI18n_SetKey( sep->title, KEY_YEAR_FORMAT );
	TextViewI18n_SetKey( sep->subtitle, KEY_MONTH_FORMAT );
	TextViewI18n_SetFormater( sep->title, RenderTitle, sep );
	TextViewI18n_SetFormater( sep->subtitle, RenderSubtitle, sep );
	Widget_AddClass( sep->title, "time-separator-title btn btn-link" );
	Widget_AddClass( sep->subtitle, "time-separator-subtitle" );
	Widget_Append( w, sep->title );
	Widget_Append( w, sep->subtitle );
}

LCUI_BOOL TimeSeparator_CheckTime( LCUI_Widget w, struct tm *t )
{
	TimeSeparator sep = Widget_GetData( w, prototype );
	return t->tm_year == sep->time.tm_year &&
		t->tm_mon == sep->time.tm_mon;
}

void TimeSeparator_SetTime( LCUI_Widget w, const struct tm *t )
{
	TimeSeparator sep = Widget_GetData( w, prototype );
	sep->time = *t;
	TextViewI18n_Refresh( sep->title );
	TextViewI18n_Refresh( sep->subtitle );
}

void TimeSeparator_AddTime( LCUI_Widget w, struct tm *t )
{
	TimeSeparator sep = Widget_GetData( w, prototype );
	sep->count += 1;
	sep->end_time = *t;
	TextViewI18n_Refresh( sep->subtitle );
}

void TimeSeparator_Reset( LCUI_Widget w )
{
	TimeSeparator sep = Widget_GetData( w, prototype );
	sep->count = 0;
}

LCUI_Widget TimeSeparator_GetTitle( LCUI_Widget w )
{
	TimeSeparator sep = Widget_GetData( w, prototype );
	return sep->title;
}

void LCUIWidget_AddTimeSeparator( void )
{
	prototype = LCUIWidget_NewPrototype( "time-separator", NULL );
	prototype->init = TimeSeparator_OnInit;
}
