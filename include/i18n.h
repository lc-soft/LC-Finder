/* ***************************************************************************
 * i18n.h -- internationalization suport module.
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
 * i18n.h -- 国际化支持模块。
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

#ifndef LCFINDER_I18N_H
#define LCFINDER_I18N_H

typedef struct LanguageRec_ {
	char *name;		/**< 语言名称  */
	char *code;		/**< 语言代码，如：zh-cn、en */
	char *filename;		/**< 语言文件 */
} LanguageRec, *Language;

/** 从文件中载入数据 */
Dict *I18n_LoadFile( const char *path );

/** 获取与 key 对应的文本 */
const char *I18n_GetText( const char *key );

/** 从语言文件中载入数据 */
Dict *I18n_LoadFile( const char *path );

/** 获取全部语言信息列表 */
int I18n_GetLanguages( Language **languages );

/** 载入语言文件 */
Language I18n_LoadLanguage( const char *filename );

/** 设置当前语言 */
Language I18n_SetLanguage( const char *lang_code );

#endif
