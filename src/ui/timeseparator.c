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
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "timeseparator.h"

#define TEXT_TIME_TITLE		L"%d年%d月"
#define TEXT_TIME_SUBTITLE	L"%d月%d日 %d张照片"
#define TEXT_TIME_SUBTITLE2	L"%d月%d日 - %d月%d日 %d张照片"

 /** 时间分割线功能的数据 */
typedef struct TimeSeparatorRec_ {
	int count;		/**< 当前时间段内的记录数量 */
	struct tm time;		/**< 当前时间段的起始时间 */
	LCUI_Widget subtitle;	/**< 子标题 */
	LCUI_Widget title;	/**< 标题 */
} TimeSeparatorRec, *TimeSeparator;

static void TimeSeparator_OnInit( LCUI_Widget w )
{
	TimeSeparator sep;
	sep = Widget_NewPrivateData( w, TimeSeparatorRec );
	sep->count = 0;
	sep->time.tm_mon = 0;
	sep->time.tm_mday = 0;
	sep->time.tm_year = 0;
	sep->title = LCUIWidget_New( "textview" );
	sep->subtitle = LCUIWidget_New( "textview" );
	Widget_AddClass( sep->title, "time-separator-title" );
	Widget_AddClass( sep->subtitle, "time-separator-subtitle" );
	Widget_Append( w, sep->title );
	Widget_Append( w, sep->subtitle );
}

static void TimeSeparator_OnDestroy( LCUI_Widget  w )
{
	free( w->private_data );
}

LCUI_BOOL TimeSeparator_CheckTime( LCUI_Widget w, struct tm *t )
{
	TimeSeparator sep = w->private_data;
	return t->tm_year == sep->time.tm_year &&
		t->tm_mon == sep->time.tm_mon;
}

void TimeSeparator_SetTime( LCUI_Widget w, const struct tm *t )
{
	wchar_t title[128];
	TimeSeparator sep = w->private_data;
	swprintf( title, 128, TEXT_TIME_TITLE, 
		  1900 + t->tm_year, t->tm_mon + 1 );
	TextView_SetTextW( sep->title, title );
	sep->time = *t;
}

void TimeSeparator_AddTime( LCUI_Widget w, struct tm *t )
{
	wchar_t text[128];
	TimeSeparator sep = w->private_data;

	sep->count += 1;
	/** 如果时间跨度不超过一天 */
	if( t->tm_year == sep->time.tm_year &&
	    t->tm_mon == sep->time.tm_mon &&
	    t->tm_mday == sep->time.tm_mday ) {
		swprintf( text, 128, TEXT_TIME_SUBTITLE, t->tm_mon + 1,
			  t->tm_mday, sep->count );
	} else {
		swprintf( text, 128, TEXT_TIME_SUBTITLE2,
			  t->tm_mon + 1, t->tm_mday, sep->time.tm_mon + 1,
			  sep->time.tm_mday, sep->count );
	}
	TextView_SetTextW( sep->subtitle, text );
}

void LCUIWidget_AddTimeSeparator( void )
{
	LCUI_WidgetClass *wc = LCUIWidget_AddClass( "time-separator" );
	wc->methods.init = TimeSeparator_OnInit;
	wc->methods.destroy = TimeSeparator_OnDestroy;
}
