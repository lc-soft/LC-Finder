
#ifndef __THUMBVIEW_H__
#define __THUMBVIEW_H__

void ThumbView_Reset( LCUI_Widget w );

LCUI_Widget ThumbView_AppendFolder( LCUI_Widget w, const char *filepath,
				    LCUI_BOOL show_path );

LCUI_Widget ThumbView_AppendPicture( LCUI_Widget w, const char *path );

void LCUIWidget_AddTThumbView( void );


#endif
