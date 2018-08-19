/* ***************************************************************************
 * i18n_datetime.c -- internationalization datetime processing.
 *
 * Copyright (C) 2018 by Liu Chao <lc-soft@live.cn>
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
 * i18n_detetime.c -- 国际化的日期处理
 *
 * 版权所有 (C) 2018 归属于 刘超 <lc-soft@live.cn>
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

#include <wchar.h>
#include <time.h>
#include <stdio.h>
#include <LCUI_Build.h>
#include <LCUI/util/dict.h>
#include <LCUI/util/string.h>

#include "i18n.h"
#include "i18n_datetime.h"

size_t FormatYearString(wchar_t *str, size_t max_len, struct tm *t)
{
	size_t n;
	char key[64], month_key[4];
	const wchar_t *format, *month_str;
	wchar_t buf[256], year_str[6], mday_str[4];

	format = I18n_GetText(KEY_YEAR_FORMAT);
	if (!format) {
		wcscpy(str, L"<translation missing>");
		return 0;
	}
	wcscpy(buf, format);
	swprintf(year_str, 5, L"%d", 1900 + t->tm_year);
	swprintf(mday_str, 3, L"%d", t->tm_mday);
	snprintf(month_key, 3, "%d", t->tm_mon);
	snprintf(key, 63, KEY_MONTHS ".%s", month_key);
	month_str = I18n_GetText(key);
	if (!month_str) {
		wcscpy(str, L"<translation missing>");
		return 0;
	}
	n = wcsreplace(buf, 255, L"YYYY", year_str);
	n += wcsreplace(buf, 255, L"MM", month_str);
	wcsncpy(str, buf, max_len);
	return n;
}

size_t FormatMonthString(wchar_t *str, size_t max_len, struct tm *t)
{
	size_t n;
	char key[64], month_key[4];
	const wchar_t *format, *month_str;
	wchar_t buf[256], mday_str[4];

	format = I18n_GetText(KEY_MONTH_FORMAT);
	if (!format) {
		wcscpy(str, L"<translation missing>");
		return 0;
	}
	wcscpy(buf, format);
	swprintf(mday_str, 3, L"%d", t->tm_mday);
	snprintf(month_key, 3, "%d", t->tm_mon);
	snprintf(key, 63, KEY_MONTHS ".%s", month_key);
	month_str = I18n_GetText(key);
	if (!month_str) {
		wcscpy(str, L"<translation missing>");
		return 0;
	}
	n = wcsreplace(buf, 255, L"MM", month_str);
	n += wcsreplace(buf, 255, L"DD", mday_str);
	wcsncpy(str, buf, max_len);
	return n;
}
