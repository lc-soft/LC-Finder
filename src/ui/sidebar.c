#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/gui/widget.h>
#include "ui.h"

#define MAX_VIEWS 3

static const char *view_ids[MAX_VIEWS] = {"view-folders", "view-home", "view-settings"};

static void OnSidebarBtnClick( LCUI_Widget btn, LCUI_WidgetEvent *e, void *unused )
{
	int i;
	LCUI_Widget sidebar;
	LCUI_Widget view = e->data;
	const char *view_id = view->id;
	Widget_RemoveClass( view, "hide" );
	Widget_Update( view, TRUE );
	Widget_Show( view );
	for( i = 0; i < MAX_VIEWS; ++i ) {
		if( strcmp( view_id, view_ids[i]) != 0 ) {
			view = LCUIWidget_GetById( view_ids[i] );
			Widget_AddClass( view, "hide" );
			Widget_Update( view, TRUE );
			Widget_Hide( view );
		}
	}
	sidebar = LCUIWidget_GetById( "main-sidebar" );
	Widget_AddClass( sidebar, "sidebar-mini" );
	Widget_AddStatus( btn, "active" );
	Widget_Update( btn, TRUE );
	Widget_Update( sidebar, TRUE );
}

void UI_InitSidebar(void)
{
	LCUI_Widget btn, view;
	btn = LCUIWidget_GetById( "sidebar-btn-folders" );
	view = LCUIWidget_GetById( "view-folders" );
	Widget_BindEvent( btn, "click", OnSidebarBtnClick, view, NULL );
	btn = LCUIWidget_GetById( "sidebar-btn-settings" );
	view = LCUIWidget_GetById( "view-settings" );
	Widget_BindEvent( btn, "click", OnSidebarBtnClick, view, NULL );
}
