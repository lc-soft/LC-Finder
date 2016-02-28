/**
* view_folders.c -- “文件夹”视图
* 版权所有 (C) 2016 归属于 刘超 <root@lc-soft.io>
*/

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <string.h>
#include "finder.h"

static struct FoldersViewData {
	LCUI_Widget items;
} this_view;

static const char *getdirname( const char *path )
{
	int i;
	const char *p = NULL;
	for( i = 0; path[i]; ++i ) {
		if( path[i] == PATH_SEP ) {
			p = path + i + 1;
		}
	}
	return p;
}

static LCUI_Widget NewFolderItem( DB_Dir dir )
{
	LCUI_Widget item = LCUIWidget_New( NULL );
	LCUI_Widget infobar = LCUIWidget_New( NULL );
	LCUI_Widget name = LCUIWidget_New( "textview" );
	LCUI_Widget path = LCUIWidget_New( "textview" );
	LCUI_Widget icon = LCUIWidget_New( "textview" );
	Widget_AddClass( item, "folder-list-item" );
	Widget_AddClass( infobar, "info" );
	Widget_AddClass( name, "name" );
	Widget_AddClass( path, "path" );
	Widget_AddClass( icon, "icon ion ion-android-folder-open" );
	TextView_SetText( name, getdirname( dir->path ) );
	TextView_SetText( path, dir->path );
	Widget_Append( item, infobar );
	Widget_Append( infobar, name );
	Widget_Append( infobar, path );
	Widget_Append( infobar, icon );
	return item;
}

static void OnAddDir( void *privdata, void *data )
{
	LCUI_Widget item = NewFolderItem( data );
	Widget_Append( this_view.items, item );
}

static void OnBtnSyncClick( LCUI_Widget w, LCUI_WidgetEvent *e, void *arg )
{
	LCFinder_SendEvent( "sync", NULL );
}

void UI_InitFoldersView( void )
{
	int i;
	LCUI_Widget btn = LCUIWidget_GetById( "btn-sync-folder-files" );
	LCUI_Widget list = LCUIWidget_GetById( "current-folder-list" );
	for( i = 0; i < finder.n_dirs; ++i ) {
		LCUI_Widget item = NewFolderItem( finder.dirs[i] );
		Widget_Append( list, item );
	}
	this_view.items = list;
	Widget_BindEvent( btn, "click", OnBtnSyncClick, NULL, NULL );
}
