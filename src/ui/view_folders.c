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
#include "tile_item.h"

static struct FoldersViewData {
	LCUI_Widget items;
} this_view;


static void OnAddDir( void *privdata, void *data )
{
	LCUI_Widget item = CreateFolderItem( data );
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
		LCUI_Widget item = CreateFolderItem( finder.dirs[i]->path );
		Widget_Append( list, item );
	}
	this_view.items = list;
	Widget_BindEvent( btn, "click", OnBtnSyncClick, NULL, NULL );
}
