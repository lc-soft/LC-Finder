/* ***************************************************************************
 * ui.c -- ui managment module
 *
 * Copyright (C) 2016-2017 by Liu Chao <lc-soft@live.cn>
 *
 * This file is part of the LC-Finder project, and may only be used, modified,
 * and distributed under the terms of the GPLv2.
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
 * ui.c -- 图形界面管理模块
 *
 * 版权所有 (C) 2016-2017 归属于 刘超 <lc-soft@live.cn>
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

#include <stdlib.h>
#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/font/charset.h>
#include <LCUI/gui/builder.h>
#include "ui.h"
#include "thumbview.h"
#include "starrating.h"
#include "timeseparator.h"
#include "progressbar.h"
#include "textview_i18n.h"
#include "dropdown.h"
#include "dialog.h"
#include "switch.h"

#define XML_PATH "assets/views/main.xml"

typedef struct ArgumentsRec_ {
	const char *filepath;
	const char *appname;
	const char *servername;
} ArgumentsRec, *Arguments;

typedef struct ArgumentMappingRec_ {
	size_t offset;
	const char *name;
} ArgumentMappingRec, *ArgumentMapping;

#define MAPPING_NUM 1
#ifdef offsetof
#undef offsetof
#endif
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE)0)->MEMBER)  
#define setmember(STRUCT, OFFSET, MEMBER, TYPE) \
	*((TYPE*)(((char*)STRUCT) + OFFSET)) = MEMBER

static void ParseCommandArguments( Arguments args, int argc, char **argv )
{
	int i;
	const char *arg;
	ArgumentMappingRec mappings[1] = {
		{ offsetof(Arguments, servername), "-ServerName:" }
	};
	for( argc -= 1; argc > 0; -- argc ) {
		arg = argv[argc];
		for( i = 0; i < MAPPING_NUM; ++i ) {
			if( strstr( arg, mappings[i].name ) == arg  ) {
				arg += strlen( mappings[i].name );
				setmember( args, mappings[i].offset,
					   arg, const char* );
				break;
			}
		}
		if( i >= MAPPING_NUM ) {
			args->filepath = argv[argc];
		}
	}
	args->appname = argv[0];
}

static void onTimer( void *arg )
{
	//LCUI_Widget w = LCUIWidget_GetById( "debug-widget" );
	//LCUI_PrintStyleSheet( w->style );
	//LCUI_Widget w = LCUIWidget_GetById( "sidebar-btn-search" );
	//LCUI_PrintStyleSheet( w->style );
	Widget_PrintTree( NULL );
}

void UI_InitMainView( void )
{
	UI_InitSidebar();
	UI_InitHomeView();
	UI_InitSettingsView();
	UI_InitFoldersView();
	UI_InitFileSyncTip();
	UI_InitSearchView();
}

#ifdef PLATFORM_WIN32_DESKTOP
#include "../resource.h"

/** 在 surface 准备好后，设置与 surface 绑定的窗口的图标 */
static void OnSurfaceReady( LCUI_Event e, void *arg )
{
	HWND hwnd;
	HICON icon;
	HINSTANCE instance;
	LCUI_DisplayEvent dpy_ev = arg;
	LCUI_Widget window = LCUIWidget_GetById( ID_WINDOW_MAIN );
	LCUI_Surface surface = LCUIDisplay_GetSurfaceOwner( window );
	if(surface != dpy_ev->surface ) {
		return;
	}
	instance = (HINSTANCE)LCUI_GetAppData();
	hwnd = (HWND)Surface_GetHandle( surface );
	icon = LoadIcon( instance, MAKEINTRESOURCE( IDI_ICON_MAIN ) );
	SetClassLong( hwnd, GCL_HICON, (LONG)icon );
}
#endif

static void UI_SetWindowIcon( void )
{
#ifdef PLATFORM_WIN32_DESKTOP
	LCUIDisplay_BindEvent( DET_READY, OnSurfaceReady, NULL, NULL, NULL );
#endif
}

int UI_Init( int argc, char **argv )
{
	LCUI_Widget box, root;
	ArgumentsRec args = {0};
#ifndef PLATFORM_WIN32_PC_APP
	LCUI_Init();
#endif
	LCUIWidget_AddThumbView();
	LCUIWidget_AddStarRating();
	LCUIWidget_AddProgressBar();
	LCUIWidget_AddTimeSeparator();
	LCUIWidget_AddTextViewI18n();
	LCUIWidget_AddSwitch();
	LCUIWidget_AddDropdown();
	LCUIDisplay_SetMode( LCDM_WINDOWED );
#ifndef PLATFORM_WIN32_PC_APP
	LCUIDisplay_SetSize( 960, 640 );
#endif
	box = LCUIBuilder_LoadFile( FILE_MAIN_VIEW );
	if( !box ) {
		return -1;
	}
	Widget_Top( box );
	Widget_Unwrap( box );
	root = LCUIWidget_GetRoot();
	Widget_SetTitleW( root, LCFINDER_NAME );
	Widget_UpdateStyle( root, TRUE );
	UI_SetWindowIcon();
	ParseCommandArguments( &args, argc, argv );
	//LCUITimer_Set( 2000, onTimer, NULL, FALSE );
	if( !args.filepath ) {
		UI_InitSplashScreen();
		UI_InitMainView();
		UI_InitPictureView( MODE_FULL );
		return 0;
	}
	UI_InitPictureView( MODE_SINGLE_PICVIEW );
#ifdef _WIN32
	{
		char *filepath;
		wchar_t *wfilepath;
		wfilepath = DecodeANSI( args.filepath );
		filepath = EncodeUTF8( wfilepath );
		strtrim( filepath, filepath, "\"" );
		UI_OpenPictureView( filepath );
		free( wfilepath );
		free( filepath );
	}
#else
	UI_OpenPictureView( args.filepath );
#endif
	return 0;
}

int UI_Run( void )
{
	return LCUI_Main();
}

void UI_Exit( void )
{
	UI_ExitHomeView();
	UI_ExitFolderView();
	UI_ExitPictureView();
}
