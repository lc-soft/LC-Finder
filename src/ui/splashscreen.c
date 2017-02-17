/* ***************************************************************************
 * splashscreen.c -- splash screen
 *
 * Copyright (C) 2016 by Liu Chao <lc-soft@live.cn>
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
 * splashscreen.c -- 启动画面
 *
 * 版权所有 (C) 2016 归属于 刘超 <lc-soft@live.cn>
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

#include "finder.h"
#include "ui.h"
#include <LCUI/timer.h>
#include <LCUI/gui/widget.h>

#define SPLASH_IMG_PATH		L"assets/splashscreen.png"
#define FADEOUT_DURATION	500

static struct SplashScreenModule {
	LCUI_Widget screen;
	LCUI_Graph img;
} self = {0};

static void OnTimer( void *arg )
{
	Widget_Destroy( self.screen );
	Graph_Free( &self.img );
}

void UI_InitSplashScreen( void )
{
	char *path;
	wchar_t wpath[PATH_LEN];
	LCUI_Widget window;

	Graph_Init( &self.img );
	wpathjoin( wpath, finder.work_dir, SPLASH_IMG_PATH );
	window = LCUIWidget_GetById( ID_WINDOW_MAIN );
	path = EncodeANSI( wpath );
	if( LCUI_ReadPNGFile( path, &self.img ) == 0 ) {
		self.screen = LCUIWidget_New( NULL );
		Widget_SetStyle( self.screen, key_width, 1.0, scale );
		Widget_SetStyle( self.screen, key_height, 1.0, scale );
		Widget_SetStyle( self.screen, key_background_position, 
				 SV_CENTER_CENTER, style );
		Widget_SetStyle( self.screen, key_background_image, 
				 &self.img, image );
		Widget_SetStyle( self.screen, key_background_color, 
				 RGB( 255, 255, 255 ), color );
		Widget_SetStyle( self.screen, key_position, 
				 SV_ABSOLUTE, style );
		Widget_SetStyle( self.screen, key_z_index, 1000, int );
		Widget_Append( window, self.screen );
		LCUITimer_Set( 1500, OnTimer, NULL, FALSE );
	}
	free( path );
}
