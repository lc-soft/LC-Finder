/* ***************************************************************************
* view_picture.c -- picture view,
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
* view_picture.c -- "图片" 视图，用于显示单个图片，并提供缩放、切换等功能
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

#include <time.h>
#include <stdio.h>
#include <string.h>
#include "finder.h"
#include <LCUI/display.h>
#include <LCUI/gui/builder.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>

#define XML_PATH "res/ui-view-picture.xml"

/** 图片查看器相关数据 */
static struct PictureViewer {
	char *filepath;			/**< 当前需加载的图片文件路径  */
	LCUI_Widget window;		/**< 图片查看器窗口 */
	LCUI_Widget target;		/**< 用于显示目标图片的部件 */
	LCUI_BOOL is_loading;		/**< 是否正在载入图片 */
	LCUI_BOOL is_working;		/**< 当前视图是否在工作 */
	LCUI_Cond cond;			/**< 条件变量 */
	LCUI_Mutex mutex;		/**< 互斥锁 */
	LCUI_Thread th_picloader;	/**< 图片加载器线程 */
} this_view;

static void PictureLoader( void *arg )
{
	char *filepath;
	LCUI_Graph *img = NULL;
	LCUI_Graph *old_img = NULL;
	LCUI_StyleSheet sheet = this_view.target->custom_style;
	while( this_view.is_working ) {
		LCUICond_Wait( &this_view.cond, &this_view.mutex );
		if( !this_view.filepath ) {
			continue;
		}
		filepath = this_view.filepath;
		_DEBUG_MSG("start load picture: %s\n", filepath);
		this_view.filepath = NULL;
		LCUIMutex_Unlock( &this_view.mutex );
		if( old_img ) {
			Graph_Free( old_img );
			free( old_img );
		}
		old_img = img;
		img = Graph_New();
		Graph_LoadImage( filepath, img );
		this_view.is_loading = TRUE;
		SetStyle( sheet, key_background_image, img, image );
		Widget_AddClass( this_view.target, "contain-mode" );
		Widget_UpdateStyle( this_view.target, FALSE );
		free( filepath );
		_DEBUG_MSG("end load picture\n");
		this_view.is_loading = FALSE;
	}
	LCUIThread_Exit( NULL );
}

void UI_InitPictureView( void )
{
	LCUI_Widget box;
	box = LCUIBuilder_LoadFile( XML_PATH );
	if( box ) {
		Widget_Top( box );
		Widget_Unwrap( box );
	}
	this_view.filepath = NULL;
	this_view.is_working = TRUE;
	LCUICond_Init( &this_view.cond );
	LCUIMutex_Init( &this_view.mutex );
	this_view.window = LCUIWidget_GetById("picture-viewer-window");
	this_view.target = LCUIWidget_GetById("picture-viewer-target");
	Widget_Hide( this_view.window );
	LCUIThread_Create( &this_view.th_picloader, PictureLoader, NULL );
}

void UIPictureView_Open( const char *filepath )
{
	wchar_t *wpath;
	int len = strlen( filepath );
	LCUIMutex_Lock( &this_view.mutex );
	wpath = calloc( len + 1, sizeof( wchar_t ) );
	this_view.filepath = calloc( len + 1, sizeof( char ) );
	LCUI_DecodeString( wpath, filepath, len, ENCODING_UTF8 );
	LCUI_EncodeString( this_view.filepath, wpath, len, ENCODING_ANSI );
	LCUIMutex_Unlock( &this_view.mutex );
	LCUICond_Signal( &this_view.cond );
	Widget_Show( this_view.window );
}
