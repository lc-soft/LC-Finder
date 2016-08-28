/* ***************************************************************************
* sidebar.h -- sidebar widget
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
* sidebar.c -- 侧边栏部件
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

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/gui/widget.h>
#include "ui.h"

#define MAX_VIEWS 4

static const char *btn_view_ids[MAX_VIEWS][2] = {
	{"sidebar-btn-folders", "view-folders"},
	{"sidebar-btn-home", "view-home"},
	{"sidebar-btn-settings", "view-settings"},
	{"sidebar-btn-search", "view-search"}
};

static void OnSidebarBtnClick( LCUI_Widget self, LCUI_WidgetEvent e, void *unused )
{
	int i;
	LCUI_Widget sidebar;
	LCUI_Widget btn, view = e->data;
	const char *view_id = view->id;
	Widget_RemoveClass( view, "hide" );
	Widget_Show( view );
	for( i = 0; i < MAX_VIEWS; ++i ) {
		if( strcmp( view_id, btn_view_ids[i][1] ) == 0 ) {
			continue;
		}
		btn = LCUIWidget_GetById( btn_view_ids[i][0] );
		view = LCUIWidget_GetById( btn_view_ids[i][1] );
		Widget_RemoveClass( btn, "active" );
		Widget_Hide( view );
	}
	sidebar = LCUIWidget_GetById( ID_VIEW_MAIN_SIDEBAR );
	Widget_AddClass( sidebar, "sidebar-mini" );
	Widget_AddClass( self, "active" );
}

void UI_InitSidebar(void)
{
	int i;
	LCUI_Widget btn, view;
	for( i = 0; i < MAX_VIEWS; ++i ) {
		btn = LCUIWidget_GetById( btn_view_ids[i][0] );
		view = LCUIWidget_GetById( btn_view_ids[i][1] );
		Widget_BindEvent( btn, "click", OnSidebarBtnClick, view, NULL );
	}
}
