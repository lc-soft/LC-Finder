#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/gui/widget.h>
#include "ui.h"

#define MAX_VIEWS 3

static const char *btn_view_ids[MAX_VIEWS][2] = {
	{"sidebar-btn-folders", "view-folders"},
	{"sidebar-btn-home", "view-home"},
	{"sidebar-btn-settings", "view-settings"},
};

static void OnSidebarBtnClick( LCUI_Widget self, LCUI_WidgetEvent *e, void *unused )
{
	int i;
	LCUI_Widget sidebar;
	LCUI_Widget btn, view = e->data;
	const char *view_id = view->id;
	Widget_RemoveClass( view, "hide" );
	Widget_UpdateStyle( view, TRUE );
	Widget_Show( view );
	for( i = 0; i < MAX_VIEWS; ++i ) {
		if( strcmp( view_id, btn_view_ids[i][1] ) == 0 ) {
			continue;
		}
		btn = LCUIWidget_GetById( btn_view_ids[i][0] );
		view = LCUIWidget_GetById( btn_view_ids[i][1] );
		Widget_AddClass( view, "hide" );
		Widget_RemoveClass( btn, "active" );
		Widget_Hide( view );
	}
	sidebar = LCUIWidget_GetById( "main-sidebar" );
	Widget_AddClass( sidebar, "sidebar-mini" );
	Widget_AddClass( self, "active" );
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
