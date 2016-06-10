/* ***************************************************************************
 * view_settings.c -- settings view
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
 * view_settings.c -- “设置”视图
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

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif
#include <stdio.h>
#include "finder.h"
#include "ui.h"
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "dialog_confirm.h"

#define DIALOG_TITLE	L"确定要移除该源文件夹？"
#define DIALOG_TEXT	L"一旦移除后，该源文件夹内的文件相关信息将一同被移除。"

static struct SettingsViewData {
	LCUI_Widget dirlist;
	Dict *dirpaths;
} this_view;

static void OnBtnRemoveClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	DB_Dir dir = e->data;
	LCUI_Widget window = LCUIWidget_GetById( ID_WINDOW_MAIN );
	if( !LCUIDialog_Confirm( window, DIALOG_TITLE, DIALOG_TEXT ) ) {
		return;
	}
	LCFinder_DeleteDir( dir );
}

static LCUI_Widget NewDirListItem( DB_Dir dir )
{
	LCUI_Widget item, icon, text, btn;
	item = LCUIWidget_New( NULL );
	icon = LCUIWidget_New( "textview" );
	text = LCUIWidget_New( "textview" );
	btn = LCUIWidget_New( "textview" );
	Widget_AddClass( item, "source-list-item" );
	Widget_AddClass( icon, "icon mdi mdi-folder-outline" );
	Widget_AddClass( text, "text" );
	Widget_AddClass( btn, "btn-delete mdi mdi-close" );
	TextView_SetText( text, dir->path );
	Widget_BindEvent( btn, "click", OnBtnRemoveClick, dir, NULL );
	Widget_Append( item, icon );
	Widget_Append( item, text );
	Widget_Append( item, btn );
	return item;
}

static void OnDelDir( void *privdata, void *arg )
{
	DB_Dir dir = arg;
	LCUI_Widget item = Dict_FetchValue( this_view.dirpaths, dir->path );
	if( item ) {
		DEBUG_MSG("destroy item: %p\n", item);
		Dict_Delete( this_view.dirpaths, dir->path );
		Widget_Destroy( item );
	}
}

static void OnAddDir( void *privdata, void *arg )
{
	DB_Dir dir = arg;
	LCUI_Widget item = NewDirListItem( dir );
	Widget_Append( this_view.dirlist, item );
	Dict_Add( this_view.dirpaths, dir->path, item );
}

/** 初始化文件夹目录控件 */
static void UI_InitDirList( LCUI_Widget view )
{
	int i;
	LCUI_Widget item;
	this_view.dirpaths = StrDict_Create( NULL, NULL );
	for( i = 0; i < finder.n_dirs; ++i ) {
		item = NewDirListItem( finder.dirs[i] );
		Widget_Append( view, item );
		Dict_Add( this_view.dirpaths, finder.dirs[i]->path, item );
	}
	LCFinder_BindEvent( EVENT_DIR_ADD, OnAddDir, NULL );
	LCFinder_BindEvent( EVENT_DIR_DEL, OnDelDir, NULL );
}

static void OnSelectDir( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	int len;
	HWND hwnd;
	DB_Dir dir;
	BROWSEINFOW bi;
	LCUI_Surface s;
	LPITEMIDLIST iids;
	char dirpath[PATH_LEN] = "";
	wchar_t wdirpath[PATH_LEN] = L"";
	s = LCUIDisplay_GetSurfaceOwner( w );
	hwnd = Surface_GetHandle( s );
	memset( &bi, 0, sizeof( bi ) );
	bi.hwndOwner = hwnd;
	bi.pidlRoot = NULL;
	bi.lpszTitle = L"选择文件夹";
	bi.ulFlags = BIF_NEWDIALOGSTYLE;
	iids = SHBrowseForFolder( &bi );
	_DEBUG_MSG( "iids: %p\n", iids );
	if( !iids ) {
		return;
	}
	SHGetPathFromIDList( iids, wdirpath );
	len = wcslen( wdirpath );
	if( len <= 0 ) {
		return;
	}
	LCUI_EncodeString( dirpath, wdirpath, PATH_LEN, ENCODING_UTF8 );
	if( LCFinder_GetDir( dirpath ) ) {
		return;
	}
	dir = LCFinder_AddDir( dirpath );
	if( dir ) {
		LCFinder_TriggerEvent( EVENT_DIR_ADD, dir );
	}
}

/** 更新缩略图数据库的空间占用量 */
static void UpdateThumbDBSpaceUsage( void )
{
	int64_t sum_size;
	wchar_t text[128];
	LCUI_Widget widget;
	sum_size = LCFinder_GetThumbDBTotalSize();
	widget = LCUIWidget_GetById( ID_TXT_THUMB_DB_SPACE_USAGE );
	getsizestr( text, 128, sum_size );
	TextView_SetTextW( widget, text );
}

static void OnBtnSettingsClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	UpdateThumbDBSpaceUsage();
}

static void OnThumbDBDelDone( LCUI_Event e, void *arg )
{
	UpdateThumbDBSpaceUsage();
}

/** 在“清除”按钮被点击时 */
static void OnBtnClearThumbDBClick( LCUI_Widget w, LCUI_WidgetEvent e, 
				    void *arg )
{
	LCUI_Widget text;
	LinkedListNode *node;
	LinkedList_ForEach( node, &w->children ) {
		LCUI_Widget child = node->data;
		if( child->type && strcmp(child->type, "textview") == 0 ) {
			text = child;
			break;
		}
	}
	Widget_AddClass( w, "disabled" );
	TextView_SetTextW( text, L"正在清除..." );
	Widget_UnbindEvent( w, "click", OnBtnClearThumbDBClick );
	LCFinder_ClearThumbDB();
	Widget_RemoveClass( w, "disabled" );
	TextView_SetTextW( text, L"清除" );
	Widget_BindEvent( w, "click", OnBtnClearThumbDBClick, NULL, NULL );
}

/** 在“许可协议”按钮被点击时 */
static void OnBtnLicenseClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	wopenbrowser( L"http://www.gnu.org/licenses/gpl-2.0.html" );
}

/** 在“官方网站”按钮被点击时 */
static void OnBtnWebSiteClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	wopenbrowser( L"https://lc-soft.io/" );
}

/** 在“问题反馈”按钮被点击时 */
static void OnBtnFeedbackClick( LCUI_Widget w, LCUI_WidgetEvent e,  void *arg )
{
	wopenbrowser( L"https://github.com/lc-soft/LC-Finder/issues/new" );
}

void UI_InitSettingsView( void )
{
	LCUI_Widget btn = LCUIWidget_GetById( ID_BTN_ADD_SOURCE );
	LCUI_Widget dirlist = LCUIWidget_GetById( ID_VIEW_SOURCE_LIST );
	Widget_BindEvent( btn, "click", OnSelectDir, NULL, NULL );
	UI_InitDirList( dirlist );
	UpdateThumbDBSpaceUsage();
	this_view.dirlist = dirlist;
	btn = LCUIWidget_GetById( ID_BTN_SIDEBAR_SETTINGS );
	Widget_BindEvent( btn, "click", OnBtnSettingsClick, NULL, NULL );
	LCFinder_BindEvent( EVENT_THUMBDB_DEL_DONE, OnThumbDBDelDone, NULL );
	btn = LCUIWidget_GetById( ID_BTN_CLEAR_THUMB_DB );
	Widget_BindEvent( btn, "click", OnBtnClearThumbDBClick, NULL, NULL );
	btn = LCUIWidget_GetById( ID_BTN_OPEN_LICENSE );
	Widget_BindEvent( btn, "click", OnBtnLicenseClick, NULL, NULL );
	btn = LCUIWidget_GetById( ID_BTN_OPEN_WEBSITE );
	Widget_BindEvent( btn, "click", OnBtnWebSiteClick, NULL, NULL );
	btn = LCUIWidget_GetById( ID_BTN_OPEN_FEEDBACK );
	Widget_BindEvent( btn, "click", OnBtnFeedbackClick, NULL, NULL );
}
