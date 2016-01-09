/* ***************************************************************************
* main.c -- The code of main function, and other related code.
*
* Copyright (C) 2015 by Liu Chao <lc-soft@live.cn>
*
* This file is part of the LC-Finder project, and may only be used, modified,
* and distributed under the terms of the GPLv2.
*
* (GPLv2 is abbreviation of GNU General Public License Version 2)
*
* By continuing to use, modify, or distribute this file you indicate that you
* have read the license and understand and accept it fully.
*
* The LC-Finder project is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GPL v2 for more details.
*
* You should have received a copy of the GPLv2 along with this file. It is
* usually in the LICENSE.TXT file, If not, see <http://www.gnu.org/licenses/>.
* ****************************************************************************/

/* ****************************************************************************
* main.c -- main 函数的实现代码，以及相关功能代码。
*
* 版权所有 (C) 2015 归属于 刘超 <lc-soft@live.cn>
*
* 这个文件是 LC-Finder 项目的一部分，并且只可以根据GPLv2许可协议来使用、更改和
* 发布。
*
* 继续使用、修改或发布本文件，表明您已经阅读并完全理解和接受这个许可协议。
*
* LC-Finder 项目是基于使用目的而加以散布的，但不负任何担保责任，甚至没有适销
* 性或特定用途的隐含担保，详情请参照GPLv2许可协议。
*
* 您应已收到附随于本文件的GPLv2许可协议的副本，它通常在 LICENSE 文件中，如果
* 没有，请查看：<http://www.gnu.org/licenses/>.
* ****************************************************************************/

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/builder.h>
#include "file_search.h"

#define XML_PATH "res/ui.xml"

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
	DB_Init();
	DB_AddDir( "F:/代码库/GitHub/LC-Finder" );
	//LCUITimer_Set( 1000, onTimer, NULL, FALSE );
	return LCUI_Main();
}
