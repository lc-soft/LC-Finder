/**
 * view_settings.c -- “设置”视图
 * 版权所有 (C) 2016 归属于 刘超 <root@lc-soft.io>
 */

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif
#include <stdio.h>
#include "finder.h"
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
	LCUI_Widget window = LCUIWidget_GetById( "main-window" );
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
		_DEBUG_MSG("destroy item: %p\n", item);
		Dict_Delete( this_view.dirpaths, dir->path );
		Widget_Destroy( item );
	}
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
	wprintf( L"wdirpath: ==%s==\n", wdirpath );
	len = wcslen( wdirpath );
	if( len <= 0 ) {
		return;
	}
	LCUI_EncodeString( dirpath, wdirpath, PATH_LEN, ENCODING_UTF8 );
	if( LCFinder_GetDir( dirpath ) ) {
		return;
	}
	dir = LCFinder_AddDir( dirpath );
	_DEBUG_MSG( "add dir: %s, address: %p\n", dirpath, dir );
	if( dir ) {
		LCFinder_TriggerEvent( EVENT_DIR_ADD, dir );
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
