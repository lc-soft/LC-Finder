#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/builder.h>
#include <stdlib.h>

#define XML_PATH "resources/ui.xml"

#ifdef LCUI_BUILD_IN_WIN32
#include <io.h>
#include <fcntl.h>

static void InitConsoleWindow(void)
{
	int hCrt;
	FILE *hf;
	AllocConsole();
	hCrt=_open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE),_O_TEXT );
	hf=_fdopen( hCrt, "w" );
	*stdout = *hf;
	setvbuf (stdout, NULL, _IONBF, 0);
	printf ("InitConsoleWindow OK!\n");
}

#endif

static void onTimer( void *arg )
{
	Widget_PrintTree( NULL );
	//LCUI_PrintStyleLibrary();
}

int main( int argc, char *argv[] )
{
	LCUI_Widget box;
#ifdef LCUI_BUILD_IN_WIN32
	InitConsoleWindow();
#endif
	_wchdir( L"F:/代码库/GitHub/LC-Finder" );
	LCUI_Init();
	LCUIDisplay_SetMode( LDM_WINDOWED );
	LCUIDisplay_SetSize( 960, 540 );
	box = LCUIBuilder_LoadFile( XML_PATH );
	if( box ) {
		Widget_Top( box );
		Widget_Unwrap( &box );
	}
	Widget_Update( LCUIWidget_GetRoot(), TRUE );
	LCUITimer_Set( 1000, onTimer, NULL, FALSE );
	return LCUI_Main();
}
