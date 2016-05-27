/* ***************************************************************************
* ui.h -- ui managment module
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
* ui.h -- 图形界面管理模块
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

#ifndef LCFINDER_UI_H
#define LCFINDER_UI_H

/** 初始化用户界面 */
void UI_Init( void );

void UI_Exit( void );

/** 让用户界面开始工作 */
int UI_Run( void );

/** 初始化侧边栏 */
void UI_InitSidebar( void );

/** 初始化“设置”视图 */
void UI_InitSettingsView( void );

/** 初始化“文件夹”视图 */
void UI_InitFoldersView( void );

void UI_ExitFolderView( void );

/** 初始化文件同步时的提示框 */
void UI_InitFileSyncTip( void );

/** 初始化首页集锦视图 */
void UI_InitHomeView( void );

void UI_ExitHomeView( void );

/** 初始化图片视图 */
void UI_InitPictureView( void );

void UI_ExitPictureView( void );

/** 在图片视图中打开一张图片 */
void UIPictureView_Open( const char *filepath );

#endif
