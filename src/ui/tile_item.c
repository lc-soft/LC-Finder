
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <string.h>
#include "tile_item.h"
#include "finder.h"
#include "ui.h"

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

LCUI_Widget CreateFolderItem( const char *filepath )
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
	TextView_SetText( name, getdirname( filepath ) );
	TextView_SetText( path, filepath );
	Widget_Append( item, infobar );
	Widget_Append( infobar, name );
	Widget_Append( infobar, path );
	Widget_Append( infobar, icon );
	return item;
}

LCUI_Widget CreatePictureItem( const char *filepath, LCUI_Graph *pic )
{
	LCUI_Widget item = LCUIWidget_New( NULL );
	return item;
}
