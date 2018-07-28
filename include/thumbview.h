/* ***************************************************************************
 * thumbview.h -- thumbnail list view
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
 * thumbview.h -- 缩略图列表视图部件，主要用于以缩略图形式显示文件夹和文件列表
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

#ifndef LCFINDER_THUMBVIEW_H
#define LCFINDER_THUMBVIEW_H

void ThumbView_Lock( LCUI_Widget w );

void ThumbView_Unlock( LCUI_Widget w );

void ThumbView_Empty( LCUI_Widget w );

/** 追加子部件 */
void ThumbView_Append( LCUI_Widget w, LCUI_Widget child );

/** 追加文件夹 */
LCUI_Widget ThumbView_AppendFolder( LCUI_Widget w, const char *filepath,
				    LCUI_BOOL show_path );

/** 追加图片 */
LCUI_Widget ThumbView_AppendPicture( LCUI_Widget w, DB_File file );

void ThumbViewItem_AppendToCover( LCUI_Widget item, LCUI_Widget child );

/** 更新缩略图列表的布局 */
void ThumbView_UpdateLayout( LCUI_Widget w, LCUI_Widget start_child );

/** 绑定回调函数，在布局开始时调用 */
void ThumbView_OnLayout( LCUI_Widget w, void( *func )(LCUI_Widget) );

/** 设置缩略图缓存 */
void ThumbView_SetCache( LCUI_Widget w, ThumbCache cache );

/** 设置文件存储服务的连接标识符 */
void ThumbView_SetStorage( LCUI_Widget w, int storage );

/** 启用缩略图滚动加载功能 */
void ThumbView_EnableScrollLoading( LCUI_Widget w );

/** 禁用缩略图滚动加载功能 */
void ThumbView_DisableScrollLoading( LCUI_Widget w );

/** 为缩略图列表项绑定文件 */
void ThumbViewItem_BindFile( LCUI_Widget item, DB_File file );

/** 获取与缩略图列表项绑定的文件 */
DB_File ThumbViewItem_GetFile( LCUI_Widget item );

/** 为缩略图列表项设置相关操作函数 */
void ThumbViewItem_SetFunction( LCUI_Widget item,
				void( *setthumb )(LCUI_Widget, LCUI_Graph*),
				void( *unsetthumb )(LCUI_Widget),
				void( *updatesize )(LCUI_Widget) );

void LCUIWidget_AddThumbView( void );


#endif
