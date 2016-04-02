
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <string.h>
#include "finder.h"
#include "tileitem.h"

typedef struct PictureItemDataRec_ {
	char *path;
	ThumbCache cache;
} PictureItemDataRec, *PictureItemData;

LCUI_Widget CreateFolderItem( const char *filepath )
{
	LCUI_Widget item = LCUIWidget_New( NULL );
	LCUI_Widget infobar = LCUIWidget_New( NULL );
	LCUI_Widget name = LCUIWidget_New( "textview" );
	LCUI_Widget path = LCUIWidget_New( "textview" );
	LCUI_Widget icon = LCUIWidget_New( "textview" );
	Widget_AddClass( item, "file-list-item-folder" );
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

static void OnScrollLoad( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	PictureItemData data = e->data;
	LCUI_Graph *thumb = ThumbCache_Get( data->cache, data->path );
	if( thumb ) {
		
	} else {
	
	}
	_DEBUG_MSG("on scrollload event\n");
}

LCUI_Widget CreatePictureItem( ThumbCache cache, const char *filepath )
{
	int len = strlen( filepath ) + 1;
	LCUI_Widget item = LCUIWidget_New( NULL );
	PictureItemData data = NEW( PictureItemDataRec, 1 );
	data->path = malloc( sizeof( char )*len );
	strncpy( data->path, filepath, len );
	data->cache = cache;
	Widget_AddClass( item, "file-list-item-picture" );
	Widget_BindEvent( item, "scrollload", OnScrollLoad, data->path, free );
	return item;
}
