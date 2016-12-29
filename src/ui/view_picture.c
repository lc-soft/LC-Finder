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

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "finder.h"
#include "ui.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/cursor.h>
#include <LCUI/graph.h>
#include <LCUI/input.h>
#include <LCUI/gui/builder.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "dialog.h"

#define MAX_SCALE	5.0
#define XML_PATH	"assets/ui-view-picture.xml"
#define TEXT_DELETE	L"删除此文件？"

#define ClearPositionStyle(W) do { \
	W->custom_style->sheet[key_left].is_valid = FALSE; \
	Widget_UpdateStyle( W, FALSE ); \
} while(0);

#define HideSiwtchButtons() do { \
	Widget_Hide( this_view.btn_prev ); \
	Widget_Hide( this_view.btn_next ); \
} while(0);

#define OnMouseDblClick OnBtnResetClick

enum SliderDirection {
	LEFT,
	RIGHT
};

enum SliderAction {
	SWITCH,
	RESTORE
};

/** 图片实例 */
typedef struct PcitureRec_ {
	wchar_t *file;			/**< 当前已加载的图片的路径  */
	wchar_t *file_for_load;		/**< 当前需加载的图片的路径  */
	LCUI_BOOL is_loading;		/**< 是否正在载入图片 */
	LCUI_BOOL is_valid;		/**< 是否有效 */
	LCUI_Graph *data;		/**< 当前已经加载的图片数据 */
	LCUI_Widget view;		/**< 视图，用于呈现该图片 */
	double scale;			/**< 图片缩放比例 */
	double min_scale;		/**< 图片最小缩放比例 */
	int timer;			/**< 定时器，用于延迟显示“载入中...”提示框 */
} PictureRec, *Picture;

/** 文件索引记录 */
typedef struct FileIndexRec_ {
	wchar_t *name;		/**< 文件名 */	
	LinkedListNode node;	/**< 在列表中的节点 */
} FileIndexRec, *FileIndex;

/** 文件扫描器数据结构 */
typedef struct FileScannerRec_ {
	LCUI_BOOL is_running;	/**< 是否正在运行 */
	LCUI_BOOL is_ready;	/**< 是否已准备好开始扫描 */
	LCUI_Thread thread;	/**< 扫描线程 */
	LCUI_Mutex mutex;	/**< 互斥锁 */
	LCUI_Cond cond;		/**< 条件变量 */
	wchar_t *dirpath;	/**< 扫描目录 */
	wchar_t *file;		/**< 目标文件，当扫描到该文件后会创建文件迭代器 */
	LinkedList files;	/**< 已扫描到的文件列表 */
	FileIterator iterator;	/**< 文件迭代器 */
} FileScannerRec, *FileScanner;

typedef struct FileDataPackRTec_ {
	FileIndex fidx;
	FileScanner scanner;
} FileDataPackRec, *FileDataPack;

/** 图片查看器相关数据 */
static struct PictureViewer {
	LCUI_Widget window;			/**< 图片查看器窗口 */
	LCUI_Widget tip_loading;		/**< 图片载入提示框，当图片正在载入时显示 */
	LCUI_Widget tip_empty;			/**< 提示，当无内容时显示 */
	LCUI_Widget tip_unsupport;		/**< 提示，当图片内容不受支持时显示 */
	LCUI_Widget btn_reset;			/**< 按钮，重置图片尺寸 */
	LCUI_Widget btn_prev;			/**< “上一张”按钮 */
	LCUI_Widget btn_next;			/**< “下一张”按钮 */
	LCUI_Widget btn_zoomin;			/**< “放大”按钮 */
	LCUI_Widget btn_zoomout;		/**< “缩小”按钮 */
	LCUI_Widget view_pictures;		/**< 视图，用于容纳多个图片视图 */
	LCUI_BOOL is_working;			/**< 当前视图是否在工作 */
	LCUI_BOOL is_opening;			/**< 当前视图是否为打开状态 */
	LCUI_BOOL is_zoom_mode;			/**< 当前视图是否为放大/缩小模式 */
	LCUI_BOOL is_touch_mode;		/**< 当前是否为触屏模式 */
	Picture pictures[3];			/**< 三组图片实例 */
	int mode;				/**< 当前运行模式 */
	int picture_i;				/**< 当前需加载的图片的序号（下标） */
	Picture picture;			/**< 当前焦点图片 */
	LCUI_Cond cond;				/**< 条件变量 */
	LCUI_Mutex mutex;			/**< 互斥锁 */
	LCUI_Thread th_picloader;		/**< 图片加载器线程 */
	LCUI_Thread th_scanner;			/**< 图片文件扫描器线程 */
	FileIterator iterator;			/**< 图片文件列表迭代器 */
	int focus_x, focus_y;			/**< 当前焦点位置，相对于按比例缩放后的图片 */
	int origin_focus_x, origin_focus_y;	/**< 当前焦点位置，相对于原始尺寸的图片 */
	int offset_x, offset_y;			/**< 浏览区域的位置偏移量，相对于焦点位置 */
	struct PictureDraging {
		int focus_x, focus_y;		/**< 拖动开始时的图片坐标，相对于按比例缩放后的图片 */
		int mouse_x, mouse_y;		/**< 拖动开始时的鼠标坐标 */
		LCUI_BOOL is_running;		/**< 是否正在执行拖动操作 */
		LCUI_BOOL with_x;		/**< 拖动时是否需要改变图片X坐标 */
		LCUI_BOOL with_y;		/**< 拖动时是否需要改变图片Y坐标 */
	} drag;					/**< 图片拖拽功能的相关数据 */
	struct Gesture {
		int x, y;			/**< 当前坐标 */
		int start_x, start_y;		/**< 开始坐标 */
		int64_t timestamp;		/**< 上次坐标更新时的时间戳 */
		LCUI_BOOL is_running;		/**< 是否正在运行 */
	} gesture;				/**< 手势功能的相关数据 */
	struct PictureTouchZoom {
		int point_ids[2];		/**< 两个触点的ID */
		int distance;			/**< 触点距离 */
		double scale;			/**< 缩放开始前的缩放比例 */
		LCUI_BOOL is_running;		/**< 缩放是否正在进行 */
		int x, y;			/**< 缩放开始前的触点位置 */
	} zoom;					/**< 图片触控缩放功能的相关数据 */
	struct SlideTransition {
		int action;			/**< 在滑动完后的行为 */
		int direction;			/**< 滚动方向 */
		int src_x;			/**< 初始 X 坐标 */
		int dst_x;			/**< 目标 X 坐标 */
		int timer;			/**< 定时器 */
		unsigned int duration;		/**< 持续时间 */
		int64_t start_time;		/**< 开始时间 */
		LCUI_BOOL is_running;		/**< 是否正在运行 */
	} slide;				/**< 图片水平滑动功能的相关数据 */
	FileScannerRec scanner;
} this_view = {0};

static void FileIterator_Destroy( FileIterator iter )
{
	free( iter->filepath );
	iter->privdata = NULL;
	iter->filepath = NULL;
	iter->next = NULL;
	iter->prev = NULL;
	free( iter );
}

static void FileIterator_Update( FileIterator iter )
{
	wchar_t wpath[PATH_LEN];
	FileDataPack data = iter->privdata;
	wpathjoin( wpath, data->scanner->dirpath, data->fidx->name );
	iter->length = data->scanner->files.length;
	LCUI_EncodeString( iter->filepath, wpath, PATH_LEN, ENCODING_UTF8 );
}

static void FileIterator_Next( FileIterator iter )
{
	FileDataPack data = iter->privdata;
	if( data->fidx->node.next ) {
		iter->index += 1;
		data->fidx = data->fidx->node.next->data;
		FileIterator_Update( iter );
	}
}

static void FileIterator_Prev( FileIterator iter )
{
	FileDataPack data = iter->privdata;
	if( data->fidx->node.prev && 
	    &data->fidx->node != data->scanner->files.head.next ) {
		iter->index -= 1;
		data->fidx = data->fidx->node.prev->data;
		FileIterator_Update( iter );
	}
}

static FileIterator FileIterator_Create( FileScanner scanner, FileIndex fidx )
{
	LinkedListNode *node = &fidx->node;
	FileDataPack data = NEW( FileDataPackRec, 1 );
	FileIterator iter = NEW( FileIteratorRec, 1 );

	data->fidx = fidx;
	data->scanner = scanner;
	iter->index = 0;
	iter->privdata = data;
	iter->next = FileIterator_Next;
	iter->prev = FileIterator_Prev;
	iter->destroy = FileIterator_Destroy;
	iter->filepath = NEW( char, PATH_LEN );
	while( node != scanner->files.head.next ) {
		node = node->prev;
		iter->index += 1;
	}
	FileIterator_Update( iter );
	return iter;
}

static void FileIndex_Delete( FileIndex fidx )
{
	free( fidx->name );
	fidx->name = NULL;
}

static void GetViewerSize( int *width, int *height )
{
	if( this_view.picture->view->width > 200 ) {
		*width = this_view.picture->view->width - 120;
	} else {
		*width = 200;
	}
	if( this_view.picture->view->height > 200 ) {
		*height = this_view.picture->view->height - 120;
	} else {
		*height = 200;
	}
}

/** 更新图片切换按钮的状态 */
static void UpdateSwitchButtons( void )
{
	FileIterator iter = this_view.iterator;
	Widget_Hide( this_view.btn_prev );
	Widget_Hide( this_view.btn_next );
	if( this_view.is_zoom_mode ) {
		return;
	}
	if( iter ) {
		if( iter->index > 0 ) {
			Widget_Show( this_view.btn_prev );
		}
		if( iter->length >= 1 && iter->index < iter->length - 1 ) {
			Widget_Show( this_view.btn_next );
		}
	}
}

/** 更新图片缩放按钮的状态 */
static void UpdateZoomButtons( void )
{
	if( !this_view.picture->is_valid ) {
		Widget_SetDisabled( this_view.btn_zoomin, TRUE );
		Widget_SetDisabled( this_view.btn_zoomout, TRUE );
		return;
	}
	if( this_view.picture->scale > this_view.picture->min_scale ) {
		Widget_SetDisabled( this_view.btn_zoomout, FALSE );
	} else {
		Widget_SetDisabled( this_view.btn_zoomout, TRUE );
	}
	if( this_view.picture->scale < MAX_SCALE ) {
		Widget_SetDisabled( this_view.btn_zoomin, FALSE );
	} else {
		Widget_SetDisabled( this_view.btn_zoomin, TRUE );
	}
}

static void UpdateResetSizeButton( void )
{
	LCUI_Widget txt = LinkedList_Get( &this_view.btn_reset->children, 0 );
	if( this_view.picture->is_valid ) {
		Widget_SetDisabled( this_view.btn_reset, FALSE );
	} else {
		Widget_SetDisabled( this_view.btn_reset, TRUE );
	}
	if( this_view.picture->scale == this_view.picture->min_scale ) {
		Widget_RemoveClass( txt, "mdi-fullscreen-exit" );
		Widget_AddClass( txt, "mdi-fullscreen" );
	} else {
		Widget_RemoveClass( txt, "mdi-fullscreen" );
		Widget_AddClass( txt, "mdi-fullscreen-exit" );
	}
}

static int OpenPrevPicture( void )
{
	Picture pic;
	FileIterator iter = this_view.iterator;
	if( !iter || iter->index == 0 ) {
		return -1;
	}
	iter->prev( iter );
	UpdateSwitchButtons();
	pic = this_view.pictures[2];
	Widget_Prepend( this_view.view_pictures, pic->view );
	this_view.pictures[2] = this_view.pictures[1];
	this_view.pictures[1] = this_view.pictures[0];
	if( pic->file ) {
		free( pic->file );
		pic->file = NULL;
	}
	this_view.pictures[0] = pic;
	UI_OpenPictureView( iter->filepath );
	return 0;
}

static int OpenNextPicture( void )
{
	Picture pic;
	FileIterator iter = this_view.iterator;
	if( !iter || iter->index >= iter->length - 1 ) {
		return -1;
	}
	iter->next( iter );
	UpdateSwitchButtons();
	pic = this_view.pictures[0];
	Widget_Append( this_view.view_pictures, pic->view );
	this_view.pictures[0] = this_view.pictures[1];
	this_view.pictures[1] = this_view.pictures[2];
	if( pic->file ) {
		free( pic->file );
		pic->file = NULL;
	}
	this_view.pictures[2] = pic;
	UI_OpenPictureView( iter->filepath );
	return 0;
}

static void InitSlideTransition( void )
{
	struct SlideTransition *st;
	st = &this_view.slide;
	st->direction = 0;
	st->src_x = 0;
	st->dst_x = 0;
	st->timer = 0;
	st->duration = 200;
	st->start_time = 0;
	st->is_running = FALSE;
}

static void SetSliderPostion( int x )
{
	int width;
	width = this_view.picture->view->width;
	Widget_Move( this_view.pictures[0]->view, x - width, 0 );
	Widget_Move( this_view.pictures[1]->view, x, 0 );
	Widget_Move( this_view.pictures[2]->view, x + width, 0 );
}

/** 更新滑动过渡效果 */
static void OnSlideTransition( void *arg )
{
	int delta, x;
	unsigned int  delta_time;
	struct SlideTransition *st;

	st = &this_view.slide;
	delta = st->dst_x - st->src_x;
	delta_time = (unsigned int)LCUI_GetTimeDelta( st->start_time );
	if( delta_time < st->duration ) {
		x = st->src_x + delta * (int)delta_time / (int)st->duration;
	} else {
		x = st->dst_x;
	}
	SetSliderPostion( x );
	if( x == st->dst_x ) {
		st->timer = 0;
		st->is_running = FALSE;
		if( st->action != SWITCH ) {
			return;
		}
		if( st->direction == RIGHT ) {
			OpenPrevPicture();
		} else {
			OpenNextPicture();
		}
		ClearPositionStyle( this_view.pictures[0]->view );
		ClearPositionStyle( this_view.pictures[1]->view );
		ClearPositionStyle( this_view.pictures[2]->view );
		return;	
	}
	st->timer = LCUITimer_Set( 10, OnSlideTransition, NULL, FALSE );
}

/** 还原滑动区块的位置，一般用于将拖动后的图片移动回原来的位置 */
static void RestoreSliderPosition( void )
{
	struct SlideTransition *st;
	st = &this_view.slide;
	if( st->is_running ) {
		return;
	}
	st->action = RESTORE;
	st->src_x = this_view.picture->view->x;
	if( st->src_x == 0 ) {
		return;
	} else if( st->src_x < 0 ) {
		st->direction = RIGHT;
	} else {
		st->direction = LEFT;
	}
	st->dst_x = 0;
	st->is_running = TRUE;
	st->start_time = LCUI_GetTime();
	if( st->timer > 0 ) {
		return;
	}
	st->timer = LCUITimer_Set( 20, OnSlideTransition, NULL, FALSE );
}

static void StartSlideTransition( int direction )
{
	struct SlideTransition *st;
	st = &this_view.slide;
	if( st->is_running ) {
		return;
	}
	st->action = SWITCH;
	st->direction = direction;
	st->src_x = this_view.picture->view->x;
	if( st->direction == RIGHT ) {
		st->dst_x = this_view.picture->view->width;
	} else {
		st->dst_x = 0 - this_view.picture->view->width;
	}
	st->is_running = TRUE;
	st->start_time = LCUI_GetTime();
	if( st->timer > 0 ) {
		return;
	}
	st->timer = LCUITimer_Set( 20, OnSlideTransition, NULL, FALSE );
}

static int SwitchPrevPicture( void )
{
	FileIterator iter = this_view.iterator;
	if( !iter || iter->index == 0 ) {
		return -1;
	}
	StartSlideTransition( RIGHT );
	return 0;
}

static int SwitchNextPicture( void )
{
	FileIterator iter = this_view.iterator;
	if( !iter || iter->index >= iter->length - 1 ) {
		return -1;
	}
	StartSlideTransition( LEFT );
	return 0;
}

/** 更新图片位置 */
static void UpdatePicturePosition( Picture pic )
{
	LCUI_StyleSheet sheet;
	int x = 0, y = 0, width, height;
	sheet = pic->view->custom_style;
	width = (int)(pic->data->width * pic->scale);
	height = (int)(pic->data->height * pic->scale);
	if( width <= pic->view->width ) {
		SetStyle( sheet, key_background_position_x, 0.5, scale );
	}
	if( height <= pic->view->height ) {
		SetStyle( sheet, key_background_position_y, 0.5, scale );
	}
	if( pic != this_view.picture ) {
		Widget_UpdateStyle( pic->view, FALSE );
		return;
	}
	/* 若缩放后的图片宽度小于图片查看器的宽度 */
	if( width <= pic->view->width ) {
		/* 设置拖动时不需要改变X坐标，且图片水平居中显示 */
		this_view.drag.with_x = FALSE;
		this_view.focus_x = width / 2;
		this_view.origin_focus_x = pic->data->width / 2;
		SetStyle( sheet, key_background_position_x, 0.5, scale );
	} else {
		this_view.drag.with_x = TRUE;
		x = this_view.origin_focus_x;
		this_view.focus_x = (int)(x * pic->scale + 0.5);
		x = this_view.focus_x - this_view.offset_x;
		/* X坐标调整，确保查看器的图片浏览范围不超出图片 */
		if( x < 0 ) {
			x = 0;
			this_view.focus_x = this_view.offset_x;
		}
		if( x + pic->view->width > width ) {
			x = width - pic->view->width;
			this_view.focus_x = x + this_view.offset_x;
		}
		SetStyle( sheet, key_background_position_x, -x, px );
		/* 根据缩放后的焦点坐标，计算出相对于原始尺寸图片的焦点坐标 */
		x = (int)(this_view.focus_x / pic->scale + 0.5);
		this_view.origin_focus_x = x;
	}
	/* 原理同上 */
	if( height <= pic->view->height ) {
		this_view.drag.with_y = FALSE;
		this_view.focus_y = height / 2;
		this_view.origin_focus_y = pic->data->height / 2;
		SetStyle( sheet, key_background_position_y, 0.5, scale );
	} else {
		this_view.drag.with_y = TRUE;
		y = this_view.origin_focus_y;
		this_view.focus_y = (int)(y * pic->scale + 0.5);
		y = this_view.focus_y - this_view.offset_y;
		if( y < 0 ) {
			y = 0;
			this_view.focus_y = this_view.offset_y;
		}
		if( y + pic->view->height > height ) {
			y = height - pic->view->height;
			this_view.focus_y = y + this_view.offset_y;
		}
		SetStyle( sheet, key_background_position_y, -y, px );
		y = (int)(this_view.focus_y / pic->scale + 0.5);
		this_view.origin_focus_y = y;
	}
	Widget_UpdateStyle( pic->view, FALSE );
}

/** 重置浏览区域的位置偏移量 */
static void ResetOffsetPosition( void )
{
	this_view.offset_x = this_view.picture->view->width / 2;
	this_view.offset_y = this_view.picture->view->height / 2;
}

/** 设置当前图片缩放比例 */
static void DirectSetPictureScale( Picture pic, double scale )
{
	int width, height;
	LCUI_StyleSheet sheet;
	if( scale <= pic->min_scale ) {
		scale = pic->min_scale;
	}
	if( scale > MAX_SCALE ) {
		scale = MAX_SCALE;
	}
	pic->scale = scale;
	sheet = pic->view->custom_style;
	width = (int)(scale * pic->data->width);
	height = (int)(scale * pic->data->height);
	SetStyle( sheet, key_background_size_width, width, px );
	SetStyle( sheet, key_background_size_height, height, px );
	Widget_UpdateStyle( pic->view, FALSE );
	UpdatePicturePosition( pic );
	if( pic == this_view.picture ) {
		UpdateZoomButtons();
		UpdateSwitchButtons();
		UpdateResetSizeButton();
	}
}

static void SetPictureScale( Picture pic, double scale )
{
	if( scale <= pic->min_scale ) {
		this_view.is_zoom_mode = FALSE;
	} else {
		this_view.is_zoom_mode = TRUE;
	}
	DirectSetPictureScale( pic, scale );
}

/** 重置当前显示的图片的尺寸 */
static void ResetPictureSize( Picture pic )
{
	int width, height;
	double scale_x, scale_y;
	if( !Graph_IsValid( pic->data ) ) {
		return;
	}
	GetViewerSize( &width, &height );
	/* 如果尺寸小于图片查看器尺寸 */
	if( pic->data->width < width && pic->data->height < height ) {
		pic->scale = 1.0;
	} else {
		scale_x = 1.0 * width / pic->data->width;
		scale_y = 1.0 * height / pic->data->height;
		if( scale_y < scale_x ) {
			pic->scale = scale_y;
		} else {
			pic->scale = scale_x;
		}
		if( pic->scale < 0.05 ) {
			pic->scale = 0.05;
		}
	}
	pic->min_scale = pic->scale;
	ResetOffsetPosition();
	DirectSetPictureScale( pic, pic->scale );
}

/** 在返回按钮被点击的时候 */
static OnBtnBackClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.mode == MODE_SINGLE_PICVIEW ) {
		LCUI_Widget btn_back, btn_back2;
		SelectWidget( btn_back, ID_BTN_BROWSE_ALL );
		SelectWidget( btn_back2, ID_BTN_HIDE_PICTURE_VIEWER );
		Widget_UnsetStyle( btn_back2, key_display );
		Widget_Destroy( btn_back );
		Widget_Show( btn_back2 );
		this_view.mode = MODE_FULL;
		UI_InitMainView();
	}
	UI_ClosePictureView();
}

/** 在图片视图尺寸发生变化的时候 */
static void OnPictureResize( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.is_zoom_mode ) {
		UpdatePicturePosition( this_view.pictures[0] );
		UpdatePicturePosition( this_view.pictures[1] );
		UpdatePicturePosition( this_view.pictures[2] );
	} else {
		ResetPictureSize( this_view.pictures[0] );
		ResetPictureSize( this_view.pictures[1] );
		ResetPictureSize( this_view.pictures[2] );
	}
}

static void OnShowTipLoading( void *arg )
{
	Picture pic = arg;
	if( this_view.picture == pic ) {
		if( pic->is_loading ) {
			Widget_Show( this_view.tip_loading );
		}
	}
	pic->timer = 0;
}

static void StartGesture( int mouse_x, int mouse_y )
{
	struct Gesture *g;
	g = &this_view.gesture;
	g->x = mouse_x;
	g->y = mouse_y;
	g->start_x = mouse_x;
	g->start_y = mouse_y;
	g->timestamp = LCUI_GetTime();
	g->is_running = TRUE;
}

static void StopGesture( void )
{
	this_view.gesture.is_running = FALSE;
}

static void UpdateGesture( int mouse_x, int mouse_y )
{
	struct Gesture *g = &this_view.gesture;
	/* 只处理左右滑动 */
	if( g->x != mouse_x ) {
		/* 如果滑动方向不同 */
		if( g->x > g->start_x && mouse_x < g->x ||
		    g->x < g->start_x && mouse_x > g->x ) {
			g->start_x = mouse_x;
		}
		g->x = mouse_x;
		g->timestamp = LCUI_GetTime();
	}
	g->y = mouse_y;
}

static int HandleGesture( void )
{
	struct Gesture *g = &this_view.gesture;
	int delta_time = (int)LCUI_GetTimeDelta( g->timestamp );
	/* 如果坐标停止变化已经超过 100ms，或滑动距离太短，则视为无效手势 */
	if( delta_time > 100 || abs( g->x - g->start_x ) < 80 ) {
		return -1;
	}
	if( g->x > g->start_x ) {
		return SwitchPrevPicture();
		return 0;
	} else if( g->x < g->start_x ) {
		return SwitchNextPicture();
	}
	return -1;
}

static void DragPicture( int mouse_x, int mouse_y )
{
	LCUI_StyleSheet sheet;
	Picture pic = this_view.picture;
	if( !this_view.drag.is_running ) {
		return;
	}
	sheet = pic->view->custom_style;
	if( this_view.drag.with_x ) {
		int x, width;
		width = (int)(pic->data->width * pic->scale);
		this_view.focus_x = this_view.drag.focus_x;
		this_view.focus_x -= mouse_x - this_view.drag.mouse_x;
		x = this_view.focus_x - this_view.offset_x;
		if( x < 0 ) {
			x = 0;
			this_view.focus_x = this_view.offset_x;
		}
		if( x + pic->view->width > width ) {
			x = width - pic->view->width;
			this_view.focus_x = x + this_view.offset_x;
		}
		SetStyle( pic->view->custom_style,
			  key_background_position_x, -x, px );
		x = (int)(this_view.focus_x / pic->scale);
		this_view.origin_focus_x = x;
	} else {
		SetStyle( sheet, key_background_position_x, 0.5, scale );
	}
	if( this_view.drag.with_y ) {
		int y, height;
		height = (int)(pic->data->height * pic->scale);
		this_view.focus_y = this_view.drag.focus_y;
		this_view.focus_y -= mouse_y - this_view.drag.mouse_y;
		y = this_view.focus_y - this_view.offset_y;
		if( y < 0 ) {
			y = 0;
			this_view.focus_y = this_view.offset_y;
		}
		if( y + pic->view->height > height ) {
			y = height - pic->view->height;
			this_view.focus_y = y + this_view.offset_y;
		}
		SetStyle( pic->view->custom_style,
			  key_background_position_y, -y, px );
		y = (int)(this_view.focus_y / pic->scale);
		this_view.origin_focus_y = y;
	} else {
		SetStyle( sheet, key_background_position_y, 0.5, scale );
	}
	Widget_UpdateStyle( pic->view, FALSE );
}

/** 当鼠标在图片容器上移动的时候 */
static void OnPictureMouseMove( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	/* 如果是触控模式就不再处理鼠标事件了，因为触控事件中已经处理了一次图片拖动 */
	if( this_view.is_touch_mode ) {
		return;
	}
	DragPicture( e->motion.x, e->motion.y );
}

/** 当鼠标按钮在图片容器上释放的时候 */
static void OnPictureMouseUp( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.is_touch_mode ) {
		return;
	}
	this_view.drag.is_running = FALSE;
	Widget_UnbindEvent( w, "mouseup", OnPictureMouseUp );
	Widget_UnbindEvent( w, "mousemove", OnPictureMouseMove );
	Widget_ReleaseMouseCapture( w );
}

/** 当鼠标按钮在图片容器上点住的时候 */
static void OnPictureMouseDown( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.is_touch_mode ) {
		return;
	}
	this_view.drag.is_running = TRUE;
	this_view.drag.mouse_x = e->motion.x;
	this_view.drag.mouse_y = e->motion.y;
	this_view.drag.focus_x = this_view.focus_x;
	this_view.drag.focus_y = this_view.focus_y;
	Widget_BindEvent( w, "mousemove", OnPictureMouseMove, NULL, NULL );
	Widget_BindEvent( w, "mouseup",  OnPictureMouseUp, NULL, NULL );
	Widget_SetMouseCapture( w );
}

static void OnPictureTouchDown( LCUI_TouchPoint point )
{
	this_view.is_touch_mode = TRUE;
	this_view.drag.is_running = TRUE;
	if( !this_view.is_zoom_mode ) {
		StartGesture( point->x, point->y );
	}
	this_view.drag.mouse_x = point->x;
	this_view.drag.mouse_y = point->y;
	this_view.drag.focus_x = this_view.focus_x;
	this_view.drag.focus_y = this_view.focus_y;
	Widget_SetTouchCapture( this_view.picture->view, point->id );
}

static void OnPictureTouchUp( LCUI_TouchPoint point )
{
	this_view.is_touch_mode = FALSE;
	this_view.drag.is_running = FALSE;
	if( this_view.gesture.is_running ) {
		if( !this_view.zoom.is_running && !this_view.is_zoom_mode ) {
			if( HandleGesture() != 0 ) {
				RestoreSliderPosition();
			}
		}
		StopGesture();
	}
	Widget_ReleaseTouchCapture( this_view.picture->view, -1 );
}

static void OnPictureTouchMove( LCUI_TouchPoint point )
{
	DragPicture( point->x, point->y );
	if( this_view.gesture.is_running ) {
		UpdateGesture( point->x, point->y );
		SetSliderPostion( point->x - this_view.drag.mouse_x );
	}
}

static void OnPictureTouch( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	double distance, scale;
	int i, x, y, *point_ids;
	LCUI_TouchPoint points[2] = {NULL, NULL};
	if( e->touch.n_points == 0 ) {
		return;
	}
	point_ids = this_view.zoom.point_ids;
	if( e->touch.n_points == 1 ) {
		LCUI_TouchPoint point;
		point = &e->touch.points[0];
		switch( point->state ) {
		case WET_TOUCHDOWN: 
			OnPictureTouchDown( point );
			point_ids[0] = point->id;
			break;
		case WET_TOUCHMOVE:
			OnPictureTouchMove( point );
			break;
		case WET_TOUCHUP: 
			OnPictureTouchUp( point ); 
			point_ids[0] = -1;
			break;
		default: break;
		}
		return;
	}
	StopGesture();
	this_view.drag.is_running = FALSE;
	/* 遍历各个触点，确定两个用于缩放功能的触点 */
	for( i = 0; i < e->touch.n_points; ++i ) {
		if( point_ids[0] == -1 ) {
			points[0] = &e->touch.points[i];
			point_ids[0] = points[0]->id;
			continue;
		} else if( point_ids[0] == e->touch.points[i].id ) {
			points[0] = &e->touch.points[i];
			continue;
		}
		if( this_view.zoom.point_ids[1] == -1 ) {
			points[1] = &e->touch.points[i];
			point_ids[1] = points[1]->id;
		} else if( point_ids[1] == e->touch.points[i].id ) {
			points[1] = &e->touch.points[i];
		}
	}
	/* 计算两触点的距离 */
	distance = pow( points[0]->x - points[1]->x, 2 );
	distance += pow( points[0]->y - points[1]->y, 2 );
	distance = sqrt( distance );
	if( !this_view.zoom.is_running ) {
		x = points[0]->x - (points[0]->x - points[1]->x) / 2;
		y = points[0]->y - (points[0]->y - points[1]->y) / 2;
		this_view.zoom.distance = (int)distance;
		this_view.zoom.scale = this_view.picture->scale;
		this_view.zoom.is_running = TRUE;
		this_view.zoom.x = x;
		this_view.zoom.y = y;
	} else if( points[0]->state == WET_TOUCHMOVE ||
		   points[1]->state == WET_TOUCHMOVE ) {
		/* 重新计算焦点位置 */
		x = this_view.focus_x - this_view.offset_x;
		y = this_view.focus_y - this_view.offset_y;
		this_view.offset_x = this_view.zoom.x;
		this_view.offset_y = this_view.zoom.y;
		this_view.focus_x = x + this_view.offset_x;
		this_view.focus_y = y + this_view.offset_y;
		x = (int)(this_view.focus_x / this_view.picture->scale + 0.5);
		y = (int)(this_view.focus_y / this_view.picture->scale + 0.5);
		this_view.origin_focus_x = x;
		this_view.origin_focus_y = y;
		scale = this_view.zoom.scale;
		scale *= distance / this_view.zoom.distance;
		SetPictureScale( this_view.picture, scale );
	}
	for( i = 0; i < 2; ++i ) {
		int j, id;
		/* 如果当前用于控制缩放的触点已经释放，则找其它触点来代替 */
		if( points[i]->state != WET_TOUCHUP ) {
			continue;
		}
		point_ids[i] = -1;
		this_view.zoom.is_running = FALSE;
		id = point_ids[i == 0 ? 1 : 0];
		for( j = 0; j < e->touch.n_points; ++j ) {
			if( e->touch.points[j].state == WET_TOUCHUP ||
			    e->touch.points[j].id == id ) {
				continue;
			}
			points[i] = &e->touch.points[j];
			point_ids[i] = points[i]->id;
			i -= 1;
			break;
		}
	}
	if( point_ids[0] == -1 ) {
		point_ids[0] = point_ids[1];
		point_ids[1] = -1;
		this_view.zoom.is_running = FALSE;
	} else if( point_ids[1] == -1 ) {
		this_view.zoom.is_running = FALSE;
	}
}

/** 初始化图片实例 */
static Picture CreatePicture( void )
{
	ASSIGN( pic, Picture );
	pic->timer = 0;
	pic->file = NULL;
	pic->is_loading = FALSE;
	pic->file_for_load = NULL;
	pic->data = Graph_New();
	pic->view = LCUIWidget_New( "picture" );
	Widget_Append( this_view.view_pictures, pic->view );
	BindEvent( pic->view, "resize", OnPictureResize );
	BindEvent( pic->view, "touch", OnPictureTouch );
	BindEvent( pic->view, "mousedown", OnPictureMouseDown );
	return pic;
}

static void DeletePicture( Picture pic )
{
	if( pic->data ) {
		Graph_Free( pic->data );
		pic->data = NULL;
	}
	if( pic->file_for_load ) {
		free( pic->file_for_load );
		pic->file_for_load = NULL;
	}
	if( pic->file ) {
		free( pic->file );
		pic->file = NULL;
	}
	Widget_Destroy( pic->view );
	free( pic );
}

/** 设置图片 */
static void SetPicture( Picture pic, const wchar_t *file )
{
	int len;
	if( pic->timer > 0 ) {
		LCUITimer_Free( pic->timer );
		pic->timer = 0;
	}
	if( pic->file_for_load ) {
		free( pic->file_for_load );
		pic->file_for_load = NULL;
	}
	if( file ) {
		len = wcslen( file ) + 1;
		pic->file_for_load = malloc( sizeof( wchar_t ) * len );
		wcsncpy( pic->file_for_load, file, len );
	} else {
		pic->file_for_load = NULL;
	}
}

/** 设置图片预加载列表 */
static void SetPicturePreloadList( void )
{
	wchar_t *wpath;
	FileIterator iter;
	if( !this_view.iterator ) {
		return;
	}
	iter = this_view.iterator;
	/* 预加载上一张图片 */
	if( iter->index > 0 ) {
		iter->prev( iter );
		if( iter->filepath ) {
			wpath = DecodeUTF8( iter->filepath );
			SetPicture( this_view.pictures[0], wpath );
		}
		iter->next( iter );
	}
	/* 预加载下一张图片 */
	if( iter->index < iter->length - 1 ) {
		iter->next( iter );
		if( iter->filepath ) {
			wpath = DecodeUTF8( iter->filepath );
			SetPicture( this_view.pictures[2], wpath );
		}
		iter->prev( iter );
	}
}

/** 加载图片数据 */
static int LoadPicture( Picture pic )
{
	int len;
	wchar_t *wpath;
	char *path = NULL;
#ifdef _WIN32
	int encoding = ENCODING_ANSI;
#else
	int encoding = ENCODING_UTF8;
#endif
	LCUI_StyleSheet sheet = pic->view->custom_style;
	if( !pic->file_for_load || !this_view.is_working ) {
		return -1;
	}
	/* 避免重复加载 */
	if( pic->file && wcscmp( pic->file, pic->file_for_load ) == 0 ) {
		free( pic->file_for_load );
		pic->file_for_load = NULL;
		goto load_finished;
	}
	/** 300毫秒后显示 "图片载入中..." 的提示 */
	this_view.picture->timer = LCUITimer_Set( 300, OnShowTipLoading, 
						  this_view.picture, FALSE );
	if( Graph_IsValid( pic->data ) ) {
		Widget_Lock( pic->view );
		Graph_Free( pic->data );
		sheet->sheet[key_background_image].is_valid = FALSE;
		Widget_UpdateBackground( pic->view );
		Widget_UpdateStyle( pic->view, FALSE );
		Widget_Unlock( pic->view );
	}
	wpath = pic->file_for_load;
	pic->file_for_load = NULL;
	if( pic->file ) {
		free( pic->file );
	}
	pic->file = wpath;
	pic->is_valid = FALSE;
	pic->is_loading = TRUE;
	len = LCUI_EncodeString( NULL, wpath, 0, encoding ) + 1;
	path = malloc( sizeof( char ) * len );
	LCUI_EncodeString( path, wpath, len, encoding );
	if( Graph_LoadImage( path, pic->data ) == 0 ) {
		pic->is_valid = TRUE;
	}
	/* 如果在加载完后没有待加载的图片，则直接呈现到部件上 */
	if( pic->is_valid && !pic->file_for_load ) {
		SetStyle( sheet, key_background_image, pic->data, image );
		Widget_UpdateStyle( pic->view, FALSE );
		ResetPictureSize( pic );
	}
load_finished:
	pic->is_loading = FALSE;
	if( this_view.picture->view == pic->view ) {
		Widget_Hide( this_view.tip_loading );
		if( pic->is_valid ) {
			Widget_Hide( this_view.tip_unsupport );
		} else {
			Widget_Show( this_view.tip_unsupport );
		}
		UpdateResetSizeButton();
		UpdateZoomButtons();
	}
	if( path ) {
		free( path );
	}
	return 0;
}

/** 在”放大“按钮被点击的时候 */
static void OnBtnZoomInClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	ResetOffsetPosition();
	SetPictureScale( this_view.picture, this_view.picture->scale + 0.5 );
}

/** 在”缩小“按钮被点击的时候 */
static void OnBtnZoomOutClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	ResetOffsetPosition();
	SetPictureScale( this_view.picture, this_view.picture->scale - 0.5 );
}

static void OnBtnPrevClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	OpenPrevPicture();
}

static void OnBtnNextClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	OpenNextPicture();
}

static void OnBtnDeleteClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	char *path;
	wchar_t *wpath = this_view.picture->file;
	if( !LCUIDialog_Confirm( this_view.window, TEXT_DELETE, NULL ) ) {
		return;
	}
	path = EncodeUTF8( wpath );
	if( OpenNextPicture() != 0 ) {
		/** 如果前后都没有图片了，则提示没有可显示的内容 */
		if( OpenPrevPicture() != 0 ) {
			Widget_RemoveClass( this_view.tip_empty, "hide" );
			Widget_Show( this_view.tip_empty );
		}
	}
	LCFinder_DeleteFiles( &path, 1, NULL, NULL );
	LCFinder_TriggerEvent( EVENT_FILE_DEL, path );
	free( path );
}

/** 在”实际大小“按钮被点击的时候 */
static void OnBtnResetClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	double scale;
	if( this_view.picture->scale != this_view.picture->min_scale ) {
		scale = this_view.picture->min_scale;
	} else {
		scale = 1.0;
	}
	ResetOffsetPosition();
	SetPictureScale( this_view.picture, scale );
}

/** 文件扫描器 */
static void FileScanner_Thread( void *arg )
{
	size_t len;
	LCUI_Dir dir;
	LCUI_DirEntry *entry;
	FileIndex fidx;
	FileScanner scanner = &this_view.scanner;
	int i = 0, pos = -1;

	while( !scanner->is_ready ) {
		LCUICond_Wait( &scanner->cond, &scanner->mutex );
	}
	scanner->is_ready = FALSE;
	if( LCUI_OpenDirW( scanner->dirpath, &dir ) != 0 ) {
		LCUIThread_Exit( NULL );
		return;
	}
	while( scanner->is_running && (entry = LCUI_ReadDirW( &dir )) ) {
		wchar_t *name = LCUI_GetFileNameW( entry );
		/* 忽略 . 和 .. 文件夹 */
		if( name[0] == '.' ) {
			if( name[1] == 0 || (name[1] == '.' && name[2] == 0) ) {
				continue;
			}
		}
		if( !LCUI_FileIsArchive( entry ) || !IsImageFile( name ) ) {
			continue;
		}
		len = wcslen( name ) + 1;
		fidx = NEW( FileIndexRec, 1 );
		fidx->name = malloc( sizeof( wchar_t ) * len );
		wcscpy( fidx->name, name );
		fidx->node.data = fidx;
		LinkedList_AppendNode( &scanner->files, &fidx->node );
		if( wcscmp( name, scanner->file ) == 0 && !scanner->iterator ) {
			scanner->iterator = FileIterator_Create( scanner, fidx );
			pos = i;
		}
		if( pos >= 0 && i - pos > 2 ) {
			UI_SetPictureView( scanner->iterator );
			SetPicturePreloadList();
			pos = -1;
		}
		++i;
	}
	if( pos > 0 && scanner->iterator ) {
		UI_SetPictureView( scanner->iterator );
		SetPicturePreloadList();
	}
	scanner->is_running = FALSE;
	LCUIThread_Exit( NULL );
}

static void FileScanner_Init( FileScanner scanner )
{
	scanner->iterator = NULL;
	scanner->is_ready = FALSE;
	scanner->is_running = TRUE;
	LCUICond_Init( &scanner->cond );
	LCUIMutex_Init( &scanner->mutex );
	LCUIMutex_Lock( &scanner->mutex );
	LinkedList_Init( &scanner->files );
	LCUIThread_Create( &scanner->thread, FileScanner_Thread, NULL );
}

static void FileScanner_Start( FileScanner scanner, const wchar_t *filepath )
{
	size_t len;
	const wchar_t *name;
	if( scanner->dirpath ) {
		free( scanner->dirpath );
	}
	if( scanner->file ) {
		free( scanner->file );
	}
	name = wgetfilename( filepath );
	len = wcslen( name ) + 1;
	scanner->dirpath = wgetdirname( filepath );
	scanner->file = malloc( sizeof( wchar_t ) * len );
	wcscpy( scanner->file, name );
	scanner->is_ready = TRUE;
	LCUICond_Signal( &scanner->cond );
	LCUIMutex_Unlock( &scanner->mutex );
}

static void FileScanner_Exit( FileScanner scanner )
{
	scanner->is_running = FALSE;
	LCUIThread_Join( scanner->thread, NULL );
}

/** 图片加载器 */
static void PictureLoader( void *arg )
{
	LCUIMutex_Lock( &this_view.mutex );
	while( this_view.is_working ) {
		LCUICond_Wait( &this_view.cond, &this_view.mutex );
		this_view.picture_i = 0;
		for( ; this_view.picture_i < 3; ++this_view.picture_i ) {
			int i;
			switch( this_view.picture_i ) {
			case 0: i = 1; break;
			case 1: i = 2; break;
			case 2: i = 0; break;
			default: continue;
			}
			LoadPicture( this_view.pictures[i] );
		}
	}
	LCUIMutex_Unlock( &this_view.mutex );
	LCUIThread_Exit( NULL );
}

static void OnBtnShowInfoClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	UI_ShowPictureInfoView();
}

static void OnMouseWheel( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	if( this_view.is_zoom_mode ) {
		double scale;
		ResetOffsetPosition();
		if( e->wheel.delta < 0 ) {
			scale = this_view.picture->scale - 0.5;
		} else {
			scale = this_view.picture->scale + 0.5;
		}
		SetPictureScale( this_view.picture, scale );
	} else {
		if( e->wheel.delta < 0 ) {
			OpenNextPicture();
		} else {
			OpenPrevPicture();
		}
	}
}

static void OnKeyDown( LCUI_SysEvent e, void *data )
{
	if( this_view.is_zoom_mode ) {
		switch( e->key.code ) {
		case LCUIKEY_MINUS:
			SetPictureScale( this_view.picture, 
					 this_view.picture->scale - 0.5 );
			break;
		case LCUIKEY_EQUAL:
			SetPictureScale( this_view.picture, 
					 this_view.picture->scale + 0.5 );
			break;
		case LCUIKEY_LEFT:
		case LCUIKEY_RIGHT:
		case LCUIKEY_UP:
		case LCUIKEY_DOWN:
		default: break;
		}
	} else {
		switch( e->key.code ) {
		case LCUIKEY_MINUS:
			SetPictureScale( this_view.picture, 
					 this_view.picture->scale - 0.5 );
			break;
		case LCUIKEY_EQUAL:
			SetPictureScale( this_view.picture, 
					 this_view.picture->scale + 0.5 );
			break;
		case LCUIKEY_LEFT:
			OpenPrevPicture();
			break;
		case LCUIKEY_RIGHT:
			OpenNextPicture();
		default:
			break;
		}
	}
}

void UI_InitPictureView( int mode )
{
	LCUI_Widget box, btn_back, btn_back2, btn_info, btn_del;
	box = LCUIBuilder_LoadFile( XML_PATH );
	if( box ) {
		Widget_Top( box );
		Widget_Unwrap( box );
	}
	this_view.mode = mode;
	this_view.is_working = TRUE;
	this_view.is_opening = FALSE;
	this_view.zoom.point_ids[0] = -1;
	this_view.zoom.point_ids[1] = -1;
	this_view.zoom.is_running = FALSE;
	SelectWidget( btn_back, ID_BTN_BROWSE_ALL );
	SelectWidget( btn_del, ID_BTN_DELETE_PICTURE );
	SelectWidget( btn_info, ID_BTN_SHOW_PICTURE_INFO );
	SelectWidget( btn_back2, ID_BTN_HIDE_PICTURE_VIEWER );
	SelectWidget( this_view.btn_prev, ID_BTN_PCITURE_PREV );
	SelectWidget( this_view.btn_next, ID_BTN_PCITURE_NEXT );
	SelectWidget( this_view.btn_zoomin, ID_BTN_PICTURE_ZOOM_IN );
	SelectWidget( this_view.btn_zoomout, ID_BTN_PICTURE_ZOOM_OUT );
	SelectWidget( this_view.btn_reset, ID_BTN_PICTURE_RESET_SIZE );
	SelectWidget( this_view.window, ID_WINDOW_PCITURE_VIEWER );
	SelectWidget( this_view.tip_empty, ID_TIP_PICTURE_NOT_FOUND );
	SelectWidget( this_view.tip_loading, ID_TIP_PICTURE_LOADING );
	SelectWidget( this_view.tip_unsupport, ID_TIP_PICTURE_UNSUPPORT );
	SelectWidget( this_view.view_pictures, ID_VIEW_PICTURE_TARGET );
	BindEvent( btn_back, "click", OnBtnBackClick );
	BindEvent( btn_back2, "click", OnBtnBackClick );
	BindEvent( btn_del, "click", OnBtnDeleteClick );
	BindEvent( btn_info, "click", OnBtnShowInfoClick );
	BindEvent( this_view.btn_prev, "click", OnBtnPrevClick );
	BindEvent( this_view.btn_next, "click", OnBtnNextClick );
	BindEvent( this_view.btn_reset, "click", OnBtnResetClick );
	BindEvent( this_view.btn_zoomin, "click", OnBtnZoomInClick );
	BindEvent( this_view.btn_zoomout, "click", OnBtnZoomOutClick );
	BindEvent( this_view.view_pictures, "mousewheel", OnMouseWheel );
	BindEvent( this_view.view_pictures, "dblclick", OnMouseDblClick );
	LCUI_BindEvent( LCUI_KEYDOWN, OnKeyDown, NULL, NULL );
	InitSlideTransition();
	LCUICond_Init( &this_view.cond );
	LCUIMutex_Init( &this_view.mutex );
	this_view.pictures[0] = CreatePicture();
	this_view.pictures[1] = CreatePicture();
	this_view.pictures[2] = CreatePicture();
	this_view.picture = this_view.pictures[0];
	LCUIThread_Create( &this_view.th_picloader, PictureLoader, NULL );
	Widget_Hide( this_view.window );
	UI_InitPictureInfoView();
	UpdateSwitchButtons();
	if( this_view.mode == MODE_SINGLE_PICVIEW ) {
		FileScanner_Init( &this_view.scanner );
		Widget_SetStyle( btn_back2, key_display, SV_NONE, style );
		Widget_Hide( btn_back2 );
	} else {
		Widget_Destroy( btn_back );
	}
}

void UI_OpenPictureView( const char *filepath )
{
	int len;
	wchar_t *wpath, title[256] = {0};
	LCUI_Widget main_window, root;

	len = strlen( filepath );
	root = LCUIWidget_GetRoot();
	LCUIMutex_Lock( &this_view.mutex );
	wpath = calloc( len + 1, sizeof( wchar_t ) );
	this_view.picture_i = -1;
	this_view.is_opening = TRUE;
	this_view.is_zoom_mode = FALSE;
	this_view.picture = this_view.pictures[1];
	LCUI_DecodeString( wpath, filepath, len, ENCODING_UTF8 );
	SelectWidget( main_window, ID_WINDOW_MAIN );
	swprintf( title, 255, L"%ls - "LCFINDER_NAME, wgetfilename( wpath ) );
	Widget_SetTitleW( root, title );
	Widget_Hide( this_view.tip_unsupport );
	Widget_Hide( this_view.tip_empty );
	SetPicture( this_view.picture, wpath );
	SetPicturePreloadList();
	LCUIMutex_Unlock( &this_view.mutex );
	LCUICond_Signal( &this_view.cond );
	UI_SetPictureInfoView( filepath );
	Widget_Show( this_view.window );
	Widget_Hide( main_window );
	if( this_view.mode == MODE_SINGLE_PICVIEW && 
	    !this_view.scanner.is_ready ) {
		FileScanner_Start( &this_view.scanner, wpath );
	}
	free( wpath );
}

void UI_SetPictureView( FileIterator iter )
{
	if( this_view.iterator ) {
		this_view.iterator->destroy( this_view.iterator );
		this_view.iterator = NULL;
	}
	if( iter ) {
		this_view.iterator = iter;
	}
	UpdateSwitchButtons();
}

void UI_ClosePictureView( void )
{
	LCUI_Widget main_window;
	LCUI_Widget root = LCUIWidget_GetRoot();
	SelectWidget( main_window, ID_WINDOW_MAIN );
	Widget_SetTitleW( root, LCFINDER_NAME );
	ResetPictureSize( this_view.picture );
	Widget_Show( main_window );
	Widget_Hide( this_view.window );
	this_view.is_opening = FALSE;
	LCUICond_Signal( &this_view.cond );
	UI_SetPictureView( NULL );
}

void UI_ExitPictureView( void )
{
	this_view.is_working = FALSE;
	LCUIMutex_Lock( &this_view.mutex );
	LCUICond_Signal( &this_view.cond );
	LCUIMutex_Unlock( &this_view.mutex );
	LCUIThread_Join( this_view.th_picloader, NULL );
	LCUIMutex_Destroy( &this_view.mutex );
	LCUICond_Destroy( &this_view.cond );
	DeletePicture( this_view.pictures[0] );
	DeletePicture( this_view.pictures[1] );
	DeletePicture( this_view.pictures[2] );
	this_view.pictures[0] = NULL;
	this_view.pictures[1] = NULL;
	this_view.pictures[2] = NULL;
}
