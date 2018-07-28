/* ***************************************************************************
 * textview_i18n.c -- textview internationalization version
 *
 * Copyright (C) 2016-2018 by Liu Chao <lc-soft@live.cn>
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
 * textview_i18n.c -- 文本显示部件的国际化版本
 *
 * 版权所有 (C) 2016-2018 归属于 刘超 <lc-soft@live.cn>
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

#ifndef LCFINDER_TEXTVIEW_H
#define LCFINDER_TEXTVIEW_H

/** 格式化器输出的字符串的最大长度 */
#define TXTFMT_BUF_MAX_LEN 512

typedef void (*TextFormatter)(wchar_t*, const wchar_t*, void*);

void LCUIWidget_AddTextViewI18n( void );

/** 刷新内容 */
void TextViewI18n_Refresh( LCUI_Widget w );

/** 设置 key，内容将被替换成与 key 对应的文本 */
void TextViewI18n_SetKey( LCUI_Widget w, const char *key );

/** 设置格式化器，在呈现文本前，会调用格式化器来格式化内容 */
void TextViewI18n_SetFormater( LCUI_Widget w, TextFormatter fmt, void *data );

/**< 刷新全部 textview-i18n 部件显示的文本 */
void LCUIWidget_RefreshTextViewI18n( void );

#endif
