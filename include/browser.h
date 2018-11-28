/* ***************************************************************************
 * browser.c -- file browser
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
 * browser.c -- 文件浏览器
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

#ifndef LCFINDER_BROWSER_H
#define LCFINDER_BROWSER_H

typedef struct FileSortMethodRec_ {
	char *name_key;
	enum {
		CREATE_TIME_DESC,
		CREATE_TIME_ASC,
		MODIFY_TIME_DESC,
		MODIFY_TIME_ASC,
		SCORE_DESC,
		SCORE_ASC
	} value;
} FileSortMethodRec, *FileSortMethod;

/** 文件浏览器相关数据 */
typedef struct FileBrowserRec_ {
	LCUI_Widget view;			/**< 视图 */
	LCUI_Widget items;			/**< 缩略图列表 */
	LCUI_Widget txt_selection_stats;	/**< 已选文件的统计文本 */
	LCUI_Widget btn_select;			/**< “选择”按钮 */
	LCUI_Widget btn_cancel;			/**< “取消”按钮 */
	LCUI_Widget btn_delete;			/**< “删除”按钮 */
	LCUI_Widget btn_tag;			/**< “标签”按钮 */
	LCUI_Widget tip_empty;			/**< 当内容为空时显示的提示 */
	Dict *file_indexes;			/**< 文件索引，以路径进行索引 */
	LinkedList dirs;			/**< 目录列表 */
	LinkedList files;			/**< 文件列表 */
	LinkedList selected_files;		/**< 已经选中的文件列表 */
	LCUI_BOOL is_selection_mode;		/**< 是否为选择模式 */
	void( *after_deleted )(LCUI_Widget);	/**< 回调函数，在删除一个文件后调用 */
} FileBrowserRec, *FileBrowser;

void FileBrowser_Empty( FileBrowser browser );

void FileBrowser_SetScroll( FileBrowser browser, int y );

void FileBrowser_SetButtonsDisabled( FileBrowser browser, LCUI_BOOL disabled );

void FileBrowser_Append( FileBrowser browser, LCUI_Widget widget );

LCUI_Widget FileBrowser_AppendPicture( FileBrowser browser, const DB_File file );

LCUI_Widget FileBrowser_AppendFolder( FileBrowser browser, const char *path,
				      LCUI_BOOL show_path );

void FileBrowser_Create( FileBrowser browser );

#endif
