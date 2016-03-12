/**
 * view_settings.c -- “设置”视图
 * 版权所有 (C) 2016 归属于 刘超 <root@lc-soft.io>
 */

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "finder.h"

static struct SettingsViewData {
	LCUI_Widget dirlist;
} this_view;

static LCUI_Widget NewDirListItem( DB_Dir dir )
{
	LCUI_Widget item, icon, text, btn;
	item = LCUIWidget_New( NULL );
	icon = LCUIWidget_New( "textview" );
	text = LCUIWidget_New( "textview" );
	btn = LCUIWidget_New( "textview" );
	Widget_AddClass( item, "source-list-item" );
	Widget_AddClass( icon, "icon ion ion-android-folder-open" );
	Widget_AddClass( text, "text" );
	Widget_AddClass( btn, "btn-delete ion ion-close-round" );
	TextView_SetText( text, dir->path );
	Widget_Append( item, icon );
	Widget_Append( item, text );
	Widget_Append( item, btn );
	return item;
}

static void OnAddDir( void *privdata, void *arg )
{
	DB_Dir dir = arg;
	LCUI_Widget item = NewDirListItem( dir );
	Widget_Append( this_view.dirlist, item );
}

/** 初始化文件夹目录控件 */
static void UI_InitDirList( LCUI_Widget view )
{
	int i;
	LCUI_Widget item;
	for( i = 0; i < finder.n_dirs; ++i ) {
		item = NewDirListItem( finder.dirs[i] );
		Widget_Append( view, item );
	}
	LCFinder_BindEvent( "dir.add", OnAddDir, NULL );
}

static void OnSelectDir( LCUI_Widget w, LCUI_WidgetEvent *e, void *arg )
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
	wprintf( L"%s\n", wdirpath );
	len = LCUI_EncodeString( dirpath, wdirpath, PATH_LEN, ENCODING_UTF8 );
	if( len <= 0 ) {
		return;
	}
	if( LCFinder_GetDir( dirpath ) ) {
		return;
	}
	_DEBUG_MSG( "add dir: %s\n", dirpath );
	dir = LCFinder_AddDir( dirpath );
	if( dir ) {
		LCFinder_SendEvent( "dir.add", dir );
	}
}

void UI_InitSettingsView( void )
{
	LCUI_Widget btn = LCUIWidget_GetById( "btn-add-source" );
	LCUI_Widget dirlist = LCUIWidget_GetById( "current-source-list" );
	Widget_BindEvent( btn, "click", OnSelectDir, NULL, NULL );
	UI_InitDirList( dirlist );
	this_view.dirlist = dirlist;
}
