/* ***************************************************************************
* view_picture.c -- picture view
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
* view_picture.c -- "图片" 视图，用于显示单张图片，并提供缩放、切换等功能
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
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/cursor.h>
#include <LCUI/gui/builder.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>

#define MAX_SCALE 5.0
#define XML_PATH "res/ui-view-picture.xml"

/** 图片查看器相关数据 */
static struct PictureViewer {
	char *filepath;				/**< 当前需加载的图片文件路径  */
	LCUI_Graph *img;			/**< 当前已经加载的图片数据 */
	LCUI_Widget window;			/**< 图片查看器窗口 */
	LCUI_Widget target;			/**< 用于显示目标图片的部件 */
	LCUI_Widget tip;			/**< 图片载入提示框 */
	LCUI_BOOL is_loading;			/**< 是否正在载入图片 */
	LCUI_BOOL is_working;			/**< 当前视图是否在工作 */
	LCUI_BOOL is_opening;			/**< 当前视图是否为打开状态 */
	LCUI_BOOL is_zoom_mode;			/**< 当前视图是否为放大/缩小模式 */
	double scale;				/**< 图片缩放比例 */
	double min_scale;			/**< 图片最小缩放比例 */
	LCUI_Cond cond;				/**< 条件变量 */
	LCUI_Mutex mutex;			/**< 互斥锁 */
	LCUI_Thread th_picloader;		/**< 图片加载器线程 */
	int focus_x, focus_y;			/**< 当前焦点位置，相对于按比例缩放后的图片 */
	int origin_focus_x, origin_focus_y;	/**< 当前焦点位置，相对于原始尺寸的图片 */
	struct PictureDrag {
		int focus_x, focus_y;		/**< 拖动开始时的图片坐标，相对于按比例缩放后的图片 */
		int mouse_x, mouse_y;		/**< 拖动开始时的鼠标坐标 */
		LCUI_BOOL is_running;		/**< 是否正在执行拖动操作 */
		LCUI_BOOL is_started;		/**< 是否已经拖动过一次 */
		LCUI_BOOL with_x;		/**< 拖动时是否需要改变图片X坐标 */
		LCUI_BOOL with_y;		/**< 拖动时是否需要改变图片Y坐标 */
		int eids[2];			/**< 事件绑定ID */
	} drag;					/**< 实现图片拖动浏览功能所需的数据 */
} this_view;

/** 更新图片位置 */
static void UpdatePicturePosition( void )
{
	LCUI_Style sheet;
	int x, y, width, height;
	sheet = this_view.target->custom_style->sheet;
	width = (int)(this_view.img->width * this_view.scale);
	height = (int)(this_view.img->height * this_view.scale);
	/** 若缩放后的图片宽度小于图片查看器的宽度 */
	if( width <= this_view.target->width ) {
		/* 设置拖动时不需要改变X坐标，且图片水平居中显示 */
		this_view.drag.with_x = FALSE;
		this_view.focus_x = width / 2;
		this_view.origin_focus_x = this_view.img->width / 2;
		x = (this_view.target->width - width) / 2;
		SetStyle( this_view.target->custom_style,
			  key_background_position_x, x, px );
	} else {
		this_view.drag.with_x = TRUE;
		x = this_view.origin_focus_x;
		this_view.focus_x = (int)(x * this_view.scale);
		x = this_view.focus_x - this_view.target->width / 2;
		/* X坐标调整，确保查看器的图片浏览范围不超出图片 */
		if( x < 0 ) {
			x = 0;
			this_view.focus_x = this_view.target->width / 2;
		}
		if( x + this_view.target->width > width ) {
			x = width - this_view.target->width;
			this_view.focus_x = x + this_view.target->width / 2;
		}
		SetStyle( this_view.target->custom_style,
			  key_background_position_x, -x, px );
		/* 根据缩放后的焦点坐标，计算出相对于原始尺寸图片的焦点坐标 */
		x = (int)(this_view.focus_x / this_view.scale);
		this_view.origin_focus_x = x;
	}
	/* 原理同上 */
	if( height <= this_view.target->height ) {
		this_view.drag.with_y = FALSE;
		this_view.focus_y = height / 2;
		this_view.origin_focus_y = this_view.img->height / 2;
		sheet[key_background_position_y].is_valid = FALSE;
		y = (this_view.target->height - height) / 2;
		SetStyle( this_view.target->custom_style,
			  key_background_position_y, y, px );
	} else {
		this_view.drag.with_y = TRUE;
		y = this_view.origin_focus_y;
		this_view.focus_y = (int)(y * this_view.scale);
		y = this_view.focus_y - this_view.target->height / 2;
		if( y < 0 ) {
			y = 0;
			this_view.focus_y = this_view.target->height / 2;
		}
		if( y + this_view.target->height > height ) {
			y = width - this_view.target->height;
			this_view.focus_y = y + this_view.target->height / 2;
		}
		SetStyle( this_view.target->custom_style,
			  key_background_position_y, -y, px );
		y = (int)(this_view.focus_y / this_view.scale);
		this_view.origin_focus_y = y;
	}
	Widget_UpdateStyle( this_view.target, FALSE );
}

/** 重置当前显示的图片的尺寸 */
static void ResetPictureSize( void )
{
	LCUI_Style sheet;
	double scale_x, scale_y;
	LCUI_Graph *img = this_view.img;
	if( !img ) {
		return;
	}
	/* 如果尺寸小于图片查看器尺寸 */
	if( img && img->width < this_view.target->width && 
	    img->height < this_view.target->height ) {
		Widget_RemoveClass( this_view.target, "contain-mode" );
		this_view.scale = 1.0;
	} else {
		Widget_AddClass( this_view.target, "contain-mode" );
		scale_x = 1.0 * this_view.target->width / img->width;
		scale_y = 1.0 * this_view.target->height / img->height;
		if( scale_y < scale_x ) {
			this_view.scale = scale_y;
		} else {
			this_view.scale = scale_x;
		}
		if( this_view.scale < 0.05 ) {
			this_view.scale = 0.05;
		}
		this_view.min_scale = this_view.scale;
	}
	sheet = this_view.target->custom_style->sheet;
	sheet[key_background_size].is_valid = FALSE;
	sheet[key_background_size_width].is_valid = FALSE;
	sheet[key_background_size_height].is_valid = FALSE;
	Widget_UpdateStyle( this_view.target, FALSE );
	UpdatePicturePosition();
}

/** 在返回按钮被点击的时候 */
static OnBtnClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	LCUI_Widget root = LCUIWidget_GetRoot();
	LCUI_Widget main_window = LCUIWidget_GetById( "main-window" );
	Widget_SetTitleW( root, L"LC-Finder" );
	ResetPictureSize();
	Widget_Show( main_window );
	Widget_Hide( this_view.window );
	this_view.is_opening = FALSE;
	LCUICond_Signal( &this_view.cond );
}

/** 在图片视图尺寸发生变化的时候 */
static void OnPictureResize( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.is_zoom_mode ) {
		UpdatePicturePosition();
	} else {
		ResetPictureSize();
	}
}

/** 设置当前图片缩放比例 */
static void SetPictureScale( double scale )
{
	int width, height;
	LCUI_StyleSheet sheet;
	if( scale < this_view.min_scale ) {
		scale = this_view.min_scale;
		this_view.is_zoom_mode = FALSE;
	} else {
		this_view.is_zoom_mode = TRUE;
	}
	if( scale > MAX_SCALE ) {
		scale = MAX_SCALE;
	}
	sheet = this_view.target->custom_style;
	width = (int)(scale * this_view.img->width);
	height = (int)(scale * this_view.img->height);
	SetStyle( sheet, key_background_size_width, width, px );
	SetStyle( sheet, key_background_size_height, height, px );
	Widget_RemoveClass( this_view.target, "contain-mode" );
	Widget_UpdateStyle( this_view.target, FALSE );
	this_view.scale = scale;
	UpdatePicturePosition();
}

/** 在”放大“按钮被点击的时候 */
static void OnBtnZoomInClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	SetPictureScale( this_view.scale + 0.5 );
}

/** 在”缩小“按钮被点击的时候 */
static void OnBtnZoomOutClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	SetPictureScale( this_view.scale - 0.5 );
}

/** 在”实际大小“按钮被点击的时候 */
static void OnBtnResetClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	SetPictureScale( 1.0 );
}

/** 图片加载器 */
static void PictureLoader( void *arg )
{
	int timer;
	char *filepath;
	LCUI_Graph *img = NULL;
	LCUI_Graph *old_img = NULL;
	LCUI_StyleSheet sheet = this_view.target->custom_style;
	LCUIMutex_Lock( &this_view.mutex );
	while( this_view.is_working ) {
		LCUICond_Wait( &this_view.cond, &this_view.mutex );
		_DEBUG_MSG("wakeup\n");
		if( !this_view.is_opening && img ) {
			Widget_Lock( this_view.target );
			Graph_Free( img );
			sheet->sheet[key_background_image].is_valid = FALSE;
			Widget_UpdateStyle( this_view.target, FALSE );
			Widget_Unlock( this_view.target );
			continue;
		}
		if( !this_view.filepath ) {
			continue;
		}
		filepath = this_view.filepath;
		this_view.filepath = NULL;
		if( old_img ) {
			Graph_Free( old_img );
			free( old_img );
		}
		old_img = img;
		img = Graph_New();
		this_view.is_loading = TRUE;
		this_view.is_zoom_mode = FALSE;
		/** 500毫秒后显示 "图片载入中..." 的提示 */
		timer = LCUITimer_Set( 500, (FuncPtr)Widget_Show, 
				       this_view.tip, FALSE );
		Graph_LoadImage( filepath, img );
		SetStyle( sheet, key_background_image, img, image );
		Widget_UpdateStyle( this_view.target, FALSE );
		this_view.img = img;
		ResetPictureSize();
		free( filepath );
		this_view.is_loading = FALSE;
		LCUITimer_Free( timer );
		Widget_Hide( this_view.tip );
	}
	LCUIMutex_Unlock( &this_view.mutex );
	LCUIThread_Exit( NULL );
}

/** 当鼠标按钮在图片容器上释放的时候 */
static void OnMouseUp( LCUI_SysEvent e, void *arg )
{
	this_view.drag.is_running = FALSE;
}

/** 当鼠标在图片容器上移动的时候 */
static void OnMouseMove( LCUI_SysEvent e, void *arg )
{
	LCUI_Pos pos;
	LCUI_Style sheet;
	if( !this_view.drag.is_running ) {
		return;
	}
	LCUICursor_GetPos( &pos );
	sheet = this_view.target->custom_style->sheet;
	if( this_view.drag.with_x ) {
		int x, width;
		width = (int)(this_view.img->width * this_view.scale);
		this_view.focus_x = this_view.drag.focus_x;
		this_view.focus_x -= pos.x - this_view.drag.mouse_x;
		x = this_view.focus_x - this_view.target->width / 2;
		if( x < 0 ) {
			x = 0;
			this_view.focus_x = this_view.target->width / 2;
		}
		if( x + this_view.target->width > width ) {
			x = width - this_view.target->width;
			this_view.focus_x = x + this_view.target->width / 2;
		}
		SetStyle( this_view.target->custom_style,
			  key_background_position_x, -x, px );
		x = (int)(this_view.focus_x / this_view.scale);
		this_view.origin_focus_x = x;
	} else {
		sheet[key_background_position_x].is_valid = FALSE;
	}
	if( this_view.drag.with_y ) {
		int y, height;
		height = (int)(this_view.img->height * this_view.scale);
		this_view.focus_y = this_view.drag.focus_y;
		this_view.focus_y -= pos.y - this_view.drag.mouse_y;
		y = this_view.focus_y - this_view.target->height / 2;
		if( y < 0 ) {
			y = 0;
			this_view.focus_y = this_view.target->height / 2;
		}
		if( y + this_view.target->height > height ) {
			y = height - this_view.target->height;
			this_view.focus_y = y + this_view.target->height / 2;
		}
		SetStyle( this_view.target->custom_style,
			  key_background_position_y, -y, px );
		y = (int)(this_view.focus_y / this_view.scale);
		this_view.origin_focus_y = y;
	} else {
		sheet[key_background_position_y].is_valid = FALSE;
	}
	Widget_UpdateStyle( this_view.target, FALSE );
}

/** 当鼠标按钮在图片容器上点住的时候 */
static void OnPictureMouseDown( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	int *eids;
	LCUI_Pos pos;
	LCUICursor_GetPos( &pos );
	eids = this_view.drag.eids;
	this_view.drag.mouse_x = pos.x;
	this_view.drag.mouse_y = pos.y;
	this_view.drag.is_started = TRUE;
	this_view.drag.is_running = TRUE;
	this_view.drag.focus_x = this_view.focus_x;
	this_view.drag.focus_y = this_view.focus_y;
	eids[0] = LCUI_BindEvent( LCUI_MOUSEMOVE, OnMouseMove, NULL, NULL );
	eids[1] = LCUI_BindEvent( LCUI_MOUSEUP, OnMouseUp, NULL, NULL );
}

void UI_InitPictureView( void )
{
	LCUI_Widget box, btn[3];
	box = LCUIBuilder_LoadFile( XML_PATH );
	if( box ) {
		Widget_Top( box );
		Widget_Unwrap( box );
	}
	this_view.img = NULL;
	this_view.filepath = NULL;
	this_view.is_working = TRUE;
	this_view.is_opening = FALSE;
	LCUICond_Init( &this_view.cond );
	LCUIMutex_Init( &this_view.mutex );
	btn[0] = LCUIWidget_GetById( "btn-picture-reset-size" );
	btn[1] = LCUIWidget_GetById( "btn-picture-zoom-in" );
	btn[2] = LCUIWidget_GetById( "btn-picture-zoom-out" );
	this_view.tip = LCUIWidget_GetById( "picture-loading-tip" );
	this_view.window = LCUIWidget_GetById( "picture-viewer-window" );
	this_view.target = LCUIWidget_GetById( "picture-viewer-target" );
	Widget_BindEvent( btn[0], "click", OnBtnResetClick, NULL, NULL );
	Widget_BindEvent( btn[1], "click", OnBtnZoomInClick, NULL, NULL );
	Widget_BindEvent( btn[2], "click", OnBtnZoomOutClick, NULL, NULL );
	Widget_BindEvent( this_view.target, "resize", OnPictureResize,
			  NULL, NULL );
	Widget_BindEvent( this_view.target, "mousedown", OnPictureMouseDown,
			  NULL, NULL );
	LCUIThread_Create( &this_view.th_picloader, PictureLoader, NULL );
	Widget_Hide( this_view.window );
}

void UIPictureView_Open( const char *filepath )
{
	int len;
	wchar_t *wpath;
	LCUI_Widget main_window, btn, root;

	len = strlen( filepath );
	root = LCUIWidget_GetRoot();
	LCUIMutex_Lock( &this_view.mutex );
	wpath = calloc( len + 1, sizeof( wchar_t ) );
	this_view.filepath = calloc( len + 1, sizeof( char ) );
	LCUI_DecodeString( wpath, filepath, len, ENCODING_UTF8 );
	LCUI_EncodeString( this_view.filepath, wpath, len, ENCODING_ANSI );
	btn = LCUIWidget_GetById( "btn-close-picture-viewer" );
	main_window = LCUIWidget_GetById( "main-window" );
	Widget_SetTitleW( root, wgetfilename( wpath ) );
	LCUIMutex_Unlock( &this_view.mutex );
	LCUICond_Signal( &this_view.cond );
	Widget_Show( this_view.window );
	Widget_Hide( main_window );
	this_view.is_opening = TRUE;
	Widget_BindEvent( btn, "click", OnBtnClick, NULL, NULL );
}
