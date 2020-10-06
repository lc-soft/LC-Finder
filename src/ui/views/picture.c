/* ***************************************************************************
 * view_picture.c -- picture view
 *
 * Copyright (C) 2016-2018 by Liu Chao <lc-soft@live.cn>
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
 * 版权所有 (C) 2016-2018 归属于 刘超 <lc-soft@live.cn>
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
#include <stdlib.h>
#include <string.h>

#include "finder.h"
#include "ui.h"
#include "file_storage.h"
#include "picture_loader.h"

#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/cursor.h>
#include <LCUI/graph.h>
#include <LCUI/input.h>
#include <LCUI/gui/builder.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/util/charset.h>
#include "progressbar.h"
#include "dialog.h"
#include "i18n.h"
#include "picture.h"

#define MAX_SCALE 5.0
#define SCALE_STEP 0.333
#define MOVE_STEP 50
#define KEY_DELETE "browser.dialog.title.confirm_delete"

#define ClearPositionStyle(W)                   \
	do {                                    \
		Widget_UnsetStyle(W, key_left); \
		Widget_UpdateStyle(W, FALSE);   \
	} while (0);

#define HideSiwtchButtons()                   \
	do {                                  \
		Widget_Hide(viewer.btn_prev); \
		Widget_Hide(viewer.btn_next); \
	} while (0);

#define OnMouseDblClick OnBtnResetClick

enum SliderDirection { LEFT, RIGHT };

enum SliderAction { SWITCH, RESTORE };

enum PcitureSlot { PICTURE_SLOT_PREV, PICTURE_SLOT_CURRENT, PICTURE_SLOT_NEXT };

typedef enum PictureSidePanel {
	SIDE_PANEL_NONE,
	SIDE_PANEL_INFO,
	SIDE_PANEL_LABELS
} PictureSidePanel;

/* clang-format off */

/** 图片实例 */
typedef struct PcitureRec_ {
	wchar_t *file;			/**< 当前已加载的图片的路径  */
	LCUI_Graph *data;		/**< 当前已经加载的图片数据 */
	LCUI_Widget view;		/**< 视图，用于呈现该图片 */
	double scale;			/**< 图片缩放比例 */
	double min_scale;		/**< 图片最小缩放比例 */
} PictureRec, *Picture;

/** 图片查看器相关数据 */
static struct PictureViewer {
	LCUI_Widget window;			/**< 图片查看器窗口 */
	LCUI_Widget tip_loading;		/**< 图片载入提示框，当图片正在载入时显示 */
	LCUI_Widget tip_progress;		/**< 图片载入进度条，当图片正在载入时显示 */
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
	int mode;				/**< 当前运行模式 */
	Picture picture;			/**< 当前焦点图片 */
	Picture pictures[3];			/**< 三组图片实例 */
	FileIterator iterator;			/**< 图片文件列表迭代器 */
	int focus_x, focus_y;			/**< 当前焦点位置，相对于按比例缩放后的图片 */
	int origin_focus_x, origin_focus_y;	/**< 当前焦点位置，相对于原始尺寸的图片 */
	int offset_x, offset_y;			/**< 浏览区域的位置偏移量，相对于焦点位置 */
	/** 图片拖拽功能的相关数据 */
	struct PictureDraging {
		int focus_x, focus_y;		/**< 拖动开始时的图片坐标，相对于按比例缩放后的图片 */
		int mouse_x, mouse_y;		/**< 拖动开始时的鼠标坐标 */
		LCUI_BOOL is_running;		/**< 是否正在执行拖动操作 */
		LCUI_BOOL with_x;		/**< 拖动时是否需要改变图片X坐标 */
		LCUI_BOOL with_y;		/**< 拖动时是否需要改变图片Y坐标 */
	} drag;
	/** 手势功能的相关数据 */
	struct Gesture {
		int x, y;			/**< 当前坐标 */
		int start_x, start_y;		/**< 开始坐标 */
		int64_t timestamp;		/**< 上次坐标更新时的时间戳 */
		LCUI_BOOL is_running;		/**< 是否正在运行 */
	} gesture;
	/** 图片触控缩放功能的相关数据 */
	struct PictureTouchZoom {
		int point_ids[2];		/**< 两个触点的ID */
		int distance;			/**< 触点距离 */
		double scale;			/**< 缩放开始前的缩放比例 */
		LCUI_BOOL is_running;		/**< 缩放是否正在进行 */
		int x, y;			/**< 缩放开始前的触点位置 */
	} zoom;
	/** 图片水平滑动功能的相关数据 */
	struct SlideTransition {
		int action;			/**< 在滑动完后的行为 */
		int direction;			/**< 滚动方向 */
		int src_x;			/**< 初始 X 坐标 */
		int dst_x;			/**< 目标 X 坐标 */
		int timer;			/**< 定时器 */
		unsigned int duration;		/**< 持续时间 */
		int64_t start_time;		/**< 开始时间 */
		LCUI_BOOL is_running;		/**< 是否正在运行 */
	} slide;
	void *scanner;
	PictureLoader loader;
	PictureSidePanel active_panel; /**< 当前活动的面板 */
} viewer = { 0 };

/* clang-format on */

static void TaskForSetWidgetBackground(void *arg1, void *arg2)
{
	LCUI_Widget w = arg1;
	Widget_SetStyle(w, key_background_image, arg2, image);
	Widget_UpdateStyle(w, FALSE);
}

static void TaskForHideTipEmpty(void *arg1, void *arg2)
{
	Widget_RemoveClass(viewer.tip_empty, "hide");
	Widget_Show(viewer.tip_empty);
}

static void GetViewerSize(size_t *width, size_t *height)
{
	if (viewer.picture->view->width > 200) {
		*width = (size_t)viewer.picture->view->width - 120;
	} else {
		*width = 200;
	}
	if (viewer.picture->view->height > 200) {
		*height = (size_t)viewer.picture->view->height - 120;
	} else {
		*height = 200;
	}
}

static void PictureView_SetLabels(void)
{
	PictureLabelsViewContextRec ctx;

	ctx.file = viewer.picture->file;
	ctx.view = viewer.picture->view;
	ctx.focus_x = viewer.focus_x;
	ctx.focus_y = viewer.focus_y;
	ctx.offset_x = viewer.offset_x;
	ctx.offset_y = viewer.offset_y;
	if (viewer.picture && viewer.picture->data) {
		ctx.scale = (float)viewer.picture->scale;
		ctx.width = viewer.picture->data->width;
		ctx.height = viewer.picture->data->height;
	} else {
		ctx.width = 0;
		ctx.height = 0;
		ctx.scale = 1.0f;
	}
	PictureView_SetLabelsContext(&ctx);
}

/** 更新图片切换按钮的状态 */
static void UpdateSwitchButtons(void)
{
	FileIterator iter = viewer.iterator;
	HideSiwtchButtons();
	if (viewer.is_zoom_mode) {
		return;
	}
	if (iter) {
		if (iter->index > 0) {
			Widget_Show(viewer.btn_prev);
		}
		if (iter->length >= 1 && iter->index < iter->length - 1) {
			Widget_Show(viewer.btn_next);
		}
	}
}

/** 更新图片缩放按钮的状态 */
static void UpdateZoomButtons(void)
{
	if (!viewer.picture->data) {
		Widget_SetDisabled(viewer.btn_zoomin, TRUE);
		Widget_SetDisabled(viewer.btn_zoomout, TRUE);
		return;
	}
	if (viewer.picture->scale > viewer.picture->min_scale) {
		Widget_SetDisabled(viewer.btn_zoomout, FALSE);
	} else {
		Widget_SetDisabled(viewer.btn_zoomout, TRUE);
	}
	if (viewer.picture->scale < MAX_SCALE) {
		Widget_SetDisabled(viewer.btn_zoomin, FALSE);
	} else {
		Widget_SetDisabled(viewer.btn_zoomin, TRUE);
	}
}

static void UpdateResetSizeButton(void)
{
	LCUI_Widget txt = LinkedList_Get(&viewer.btn_reset->children, 0);
	if (viewer.picture->data) {
		Widget_SetDisabled(viewer.btn_reset, FALSE);
	} else {
		Widget_SetDisabled(viewer.btn_reset, TRUE);
	}
	if (viewer.picture->scale == viewer.picture->min_scale) {
		Widget_RemoveClass(txt, "icon-fullscreen-exit");
		Widget_AddClass(txt, "icon-fullscreen");
	} else {
		Widget_RemoveClass(txt, "icon-fullscreen");
		Widget_AddClass(txt, "icon-fullscreen-exit");
	}
}

static void SetPictureView(Picture pic)
{
	LCUI_PostSimpleTask(TaskForSetWidgetBackground, pic->view, pic->data);
}

static void ClearPictureView(Picture pic)
{
	Widget_UnsetStyle(pic->view, key_background_image);
	Widget_UpdateStyle(pic->view, FALSE);
	if (pic->data) {
		free(pic->data);
		pic->data = NULL;
	}
}

static int OpenPrevPicture(void)
{
	Picture pic;
	FileIterator iter = viewer.iterator;
	Picture *pictures = viewer.pictures;

	if (!iter || iter->index == 0) {
		return -1;
	}
	iter->prev(iter);
	UpdateSwitchButtons();
	pic = pictures[PICTURE_SLOT_NEXT];
	Widget_Prepend(viewer.view_pictures, pic->view);
	pictures[PICTURE_SLOT_NEXT] = pictures[PICTURE_SLOT_CURRENT];
	pictures[PICTURE_SLOT_CURRENT] = pictures[PICTURE_SLOT_PREV];
	pictures[PICTURE_SLOT_PREV] = pic;
	ClearPictureView(pic);
	UI_OpenPictureView(iter->filepath);
	return 0;
}

static int OpenNextPicture(void)
{
	Picture pic;
	Picture *pictures = viewer.pictures;
	FileIterator iter = viewer.iterator;

	if (!iter || iter->index >= iter->length - 1) {
		return -1;
	}
	iter->next(iter);
	UpdateSwitchButtons();
	pic = viewer.pictures[PICTURE_SLOT_PREV];
	Widget_Append(viewer.view_pictures, pic->view);
	pictures[PICTURE_SLOT_PREV] = pictures[PICTURE_SLOT_CURRENT];
	pictures[PICTURE_SLOT_CURRENT] = pictures[PICTURE_SLOT_NEXT];
	pictures[PICTURE_SLOT_NEXT] = pic;
	ClearPictureView(pic);
	UI_OpenPictureView(iter->filepath);
	return 0;
}

static void InitSlideTransition(void)
{
	struct SlideTransition *st;
	st = &viewer.slide;
	st->direction = 0;
	st->src_x = 0;
	st->dst_x = 0;
	st->timer = 0;
	st->duration = 200;
	st->start_time = 0;
	st->is_running = FALSE;
}

static void SetSliderPosition(int x)
{
	float fx = (float)x;
	float width = viewer.picture->view->width;
	Widget_Move(viewer.pictures[PICTURE_SLOT_PREV]->view, fx - width, 0);
	Widget_Move(viewer.pictures[PICTURE_SLOT_CURRENT]->view, fx, 0);
	Widget_Move(viewer.pictures[PICTURE_SLOT_NEXT]->view, fx + width, 0);
}

/** 更新滑动过渡效果 */
static void OnSlideTransition(void *arg)
{
	int delta, x;
	unsigned int delta_time;
	struct SlideTransition *st;

	st = &viewer.slide;
	delta = st->dst_x - st->src_x;
	delta_time = (unsigned int)LCUI_GetTimeDelta(st->start_time);
	if (delta_time < st->duration) {
		x = st->src_x + delta * (int)delta_time / (int)st->duration;
	} else {
		x = st->dst_x;
	}
	SetSliderPosition(x);
	if (x == st->dst_x) {
		st->timer = 0;
		st->is_running = FALSE;
		ClearPositionStyle(viewer.pictures[PICTURE_SLOT_PREV]->view);
		ClearPositionStyle(viewer.pictures[PICTURE_SLOT_CURRENT]->view);
		ClearPositionStyle(viewer.pictures[PICTURE_SLOT_NEXT]->view);
		if (st->action != SWITCH) {
			return;
		}
		if (st->direction == RIGHT) {
			OpenPrevPicture();
		} else {
			OpenNextPicture();
		}
		return;
	}
	st->timer = LCUITimer_Set(10, OnSlideTransition, NULL, FALSE);
}

/** 还原滑动区块的位置，一般用于将拖动后的图片移动回原来的位置 */
static void RestoreSliderPosition(void)
{
	struct SlideTransition *st;
	st = &viewer.slide;
	if (st->is_running) {
		return;
	}
	st->action = RESTORE;
	st->src_x = (int)viewer.picture->view->x;
	if (st->src_x == 0) {
		return;
	} else if (st->src_x < 0) {
		st->direction = RIGHT;
	} else {
		st->direction = LEFT;
	}
	st->dst_x = 0;
	st->is_running = TRUE;
	st->start_time = LCUI_GetTime();
	if (st->timer > 0) {
		return;
	}
	st->timer = LCUITimer_Set(20, OnSlideTransition, NULL, FALSE);
}

static void StartSlideTransition(int direction)
{
	struct SlideTransition *st;
	st = &viewer.slide;
	if (st->is_running) {
		return;
	}
	st->action = SWITCH;
	st->direction = direction;
	st->src_x = (int)viewer.picture->view->x;
	if (st->direction == RIGHT) {
		st->dst_x = (int)viewer.picture->view->width;
	} else {
		st->dst_x = 0 - (int)viewer.picture->view->width;
	}
	st->is_running = TRUE;
	st->start_time = LCUI_GetTime();
	if (st->timer > 0) {
		return;
	}
	st->timer = LCUITimer_Set(20, OnSlideTransition, NULL, FALSE);
}

static int SwitchPrevPicture(void)
{
	FileIterator iter = viewer.iterator;
	if (!iter || iter->index == 0) {
		return -1;
	}
	StartSlideTransition(RIGHT);
	return 0;
}

static int SwitchNextPicture(void)
{
	FileIterator iter = viewer.iterator;
	if (!iter || iter->index >= iter->length - 1) {
		return -1;
	}
	StartSlideTransition(LEFT);
	return 0;
}

/** 更新图片位置 */
static void UpdatePicturePosition(Picture pic)
{
	float x = 0, y = 0, width, height;

	width = (float)(pic->data->width * pic->scale);
	height = (float)(pic->data->height * pic->scale);
	if (width <= pic->view->width) {
		Widget_SetStyle(pic->view, key_background_position_x, 0.5,
				scale);
	}
	if (height <= pic->view->height) {
		Widget_SetStyle(pic->view, key_background_position_y, 0.5,
				scale);
	}
	if (pic != viewer.picture) {
		Widget_UpdateStyle(pic->view, FALSE);
		return;
	}
	/* 若缩放后的图片宽度小于图片查看器的宽度 */
	if (width <= pic->view->width) {
		/* 设置拖动时不需要改变X坐标，且图片水平居中显示 */
		viewer.drag.with_x = FALSE;
		viewer.focus_x = iround(width / 2.0);
		viewer.origin_focus_x = iround(pic->data->width / 2.0);
		viewer.offset_x = iround(pic->view->width / 2);
		Widget_SetStyle(pic->view, key_background_position_x, 0.5,
				scale);
	} else {
		viewer.drag.with_x = TRUE;
		x = (float)viewer.origin_focus_x;
		viewer.focus_x = iround(x * pic->scale);
		x = (float)viewer.focus_x - viewer.offset_x;
		/* X坐标调整，确保查看器的图片浏览范围不超出图片 */
		if (x < 0) {
			x = 0;
			viewer.focus_x = viewer.offset_x;
		}
		if (x + pic->view->width > width) {
			x = width - pic->view->width;
			viewer.focus_x = (int)(x + viewer.offset_x);
		}
		_DEBUG_MSG("focus_x: %d\n", viewer.focus_x);
		Widget_SetStyle(pic->view, key_background_position_x, -x, px);
		/* 根据缩放后的焦点坐标，计算出相对于原始尺寸图片的焦点坐标 */
		x = (float)(viewer.focus_x / pic->scale + 0.5);
		viewer.origin_focus_x = (int)x;
	}
	/* 原理同上 */
	if (height <= pic->view->height) {
		viewer.drag.with_y = FALSE;
		viewer.focus_y = iround(height / 2);
		viewer.origin_focus_y = iround(pic->data->height / 2.0);
		viewer.offset_y = iround(pic->view->height / 2);
		Widget_SetStyle(pic->view, key_background_position_y, 0.5,
				scale);
	} else {
		viewer.drag.with_y = TRUE;
		y = (float)viewer.origin_focus_y;
		viewer.focus_y = (int)(y * pic->scale + 0.5);
		y = (float)(viewer.focus_y - viewer.offset_y);
		if (y < 0) {
			y = 0;
			viewer.focus_y = viewer.offset_y;
		}
		if (y + pic->view->height > height) {
			y = height - pic->view->height;
			viewer.focus_y = (int)(y + viewer.offset_y);
		}
		Widget_SetStyle(pic->view, key_background_position_y, -y, px);
		y = (float)(viewer.focus_y / pic->scale + 0.5);
		viewer.origin_focus_y = (int)y;
	}
	Widget_UpdateStyle(pic->view, FALSE);
	PictureView_SetLabels();
}

/** 重置浏览区域的位置偏移量 */
static void ResetOffsetPosition(void)
{
	viewer.offset_x = iround(viewer.picture->view->width / 2);
	viewer.offset_y = iround(viewer.picture->view->height / 2);
	PictureView_SetLabels();
}

/** 设置当前图片缩放比例 */
static void DirectSetPictureScale(Picture pic, double scale)
{
	float width, height;

	if (scale <= pic->min_scale) {
		scale = pic->min_scale;
	}
	if (scale > MAX_SCALE) {
		scale = MAX_SCALE;
	}
	pic->scale = scale;
	if (!pic->data) {
		return;
	}
	width = (float)(scale * pic->data->width);
	height = (float)(scale * pic->data->height);
	Widget_SetStyle(pic->view, key_background_size_width, width, px);
	Widget_SetStyle(pic->view, key_background_size_height, height, px);
	Widget_UpdateStyle(pic->view, FALSE);
	UpdatePicturePosition(pic);
	if (pic == viewer.picture) {
		UpdateZoomButtons();
		UpdateSwitchButtons();
		UpdateResetSizeButton();
		PictureView_SetLabels();
	}
}

static void SetPictureScale(Picture pic, double scale)
{
	if (scale <= pic->min_scale) {
		viewer.is_zoom_mode = FALSE;
	} else {
		viewer.is_zoom_mode = TRUE;
	}
	DirectSetPictureScale(pic, scale);
}

static void SetPictureZoomIn(Picture pic)
{
	SetPictureScale(pic, pic->scale * (1.0 + SCALE_STEP));
}

static void SetPictureZoomOut(Picture pic)
{
	SetPictureScale(pic, pic->scale * (1.0 - SCALE_STEP));
}

/** 重置当前显示的图片的尺寸 */
static void ResetPictureSize(Picture pic)
{
	size_t width, height;
	double scale_x, scale_y;
	if (!pic->data || !Graph_IsValid(pic->data)) {
		return;
	}
	GetViewerSize(&width, &height);
	/* 如果尺寸小于图片查看器尺寸 */
	if (pic->data->width < width && pic->data->height < height) {
		pic->scale = 1.0;
	} else {
		scale_x = 1.0 * width / pic->data->width;
		scale_y = 1.0 * height / pic->data->height;
		if (scale_y < scale_x) {
			pic->scale = scale_y;
		} else {
			pic->scale = scale_x;
		}
		if (pic->scale < 0.05) {
			pic->scale = 0.05;
		}
	}
	pic->min_scale = pic->scale;
	ResetOffsetPosition();
	DirectSetPictureScale(pic, pic->scale);
}

/** 在返回按钮被点击的时候 */
static void OnBtnBackClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	if (viewer.mode == MODE_SINGLE_PICVIEW) {
		LCUI_Widget btn_back, btn_back2;
		SelectWidget(btn_back, ID_BTN_BROWSE_ALL);
		SelectWidget(btn_back2, ID_BTN_HIDE_PICTURE_VIEWER);
		Widget_UnsetStyle(btn_back2, key_display);
		Widget_Destroy(btn_back);
		Widget_Show(btn_back2);
		viewer.mode = MODE_FULL;
		UI_InitMainView();
	}
	UI_ClosePictureView();
}

/** 在图片视图尺寸发生变化的时候 */
static void OnPictureResize(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	if (viewer.is_zoom_mode) {
		UpdatePicturePosition(viewer.pictures[PICTURE_SLOT_PREV]);
		UpdatePicturePosition(viewer.pictures[PICTURE_SLOT_CURRENT]);
		UpdatePicturePosition(viewer.pictures[PICTURE_SLOT_NEXT]);
	} else {
		ResetPictureSize(viewer.pictures[PICTURE_SLOT_PREV]);
		ResetPictureSize(viewer.pictures[PICTURE_SLOT_CURRENT]);
		ResetPictureSize(viewer.pictures[PICTURE_SLOT_NEXT]);
	}
}

static void StartGesture(int mouse_x, int mouse_y)
{
	struct Gesture *g;
	g = &viewer.gesture;
	g->x = mouse_x;
	g->y = mouse_y;
	g->start_x = mouse_x;
	g->start_y = mouse_y;
	g->timestamp = LCUI_GetTime();
	g->is_running = TRUE;
}

static void StopGesture(void)
{
	viewer.gesture.is_running = FALSE;
}

static void UpdateGesture(int mouse_x, int mouse_y)
{
	struct Gesture *g = &viewer.gesture;
	/* 只处理左右滑动 */
	if (g->x != mouse_x) {
		/* 如果滑动方向不同 */
		if ((g->x > g->start_x && mouse_x < g->x) ||
		    (g->x < g->start_x && mouse_x > g->x)) {
			g->start_x = mouse_x;
		}
		g->x = mouse_x;
		g->timestamp = LCUI_GetTime();
	}
	g->y = mouse_y;
}

static int HandleGesture(void)
{
	struct Gesture *g = &viewer.gesture;
	int delta_time = (int)LCUI_GetTimeDelta(g->timestamp);
	/* 如果坐标停止变化已经超过 100ms，或滑动距离太短，则视为无效手势 */
	if (delta_time > 100 || abs(g->x - g->start_x) < 80) {
		return -1;
	}
	if (g->x > g->start_x) {
		return SwitchPrevPicture();
		return 0;
	} else if (g->x < g->start_x) {
		return SwitchNextPicture();
	}
	return -1;
}

static void DragPicture(int mouse_x, int mouse_y)
{
	Picture pic = viewer.picture;

	if (!viewer.drag.is_running) {
		return;
	}

	if (viewer.drag.with_x) {
		float x, width;

		width = (float)(pic->data->width * pic->scale);
		viewer.focus_x = viewer.drag.focus_x;
		viewer.focus_x -= mouse_x - viewer.drag.mouse_x;
		x = (float)(viewer.focus_x - viewer.offset_x);
		if (x < 0) {
			x = 0;
			viewer.focus_x = viewer.offset_x;
		}
		if (x + pic->view->width > width) {
			x = width - pic->view->width;
			viewer.focus_x = iround(x + viewer.offset_x);
		}
		Widget_SetStyle(pic->view, key_background_position_x, -x, px);
		x = (float)(viewer.focus_x / pic->scale);
		viewer.origin_focus_x = iround(x);
	} else {
		Widget_SetStyle(pic->view, key_background_position_x, 0.5,
				scale);
	}
	if (viewer.drag.with_y) {
		float y, height;

		height = (float)(pic->data->height * pic->scale);
		viewer.focus_y = viewer.drag.focus_y;
		viewer.focus_y -= mouse_y - viewer.drag.mouse_y;
		y = (float)(viewer.focus_y - viewer.offset_y);
		if (y < 0) {
			y = 0;
			viewer.focus_y = viewer.offset_y;
		}
		if (y + pic->view->height > height) {
			y = height - pic->view->height;
			viewer.focus_y = iround(y + viewer.offset_y);
		}
		Widget_SetStyle(pic->view, key_background_position_y, -y, px);
		y = (float)(viewer.focus_y / pic->scale);
		viewer.origin_focus_y = iround(y);
	} else {
		Widget_SetStyle(pic->view, key_background_position_y, 0.5,
				scale);
	}
	Widget_UpdateStyle(pic->view, FALSE);
	PictureView_SetLabels();
}

/** 当鼠标在图片容器上移动的时候 */
static void OnPictureMouseMove(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	/* 如果是触控模式就不再处理鼠标事件了，因为触控事件中已经处理了一次图片拖动
	 */
	if (viewer.is_touch_mode) {
		return;
	}
	DragPicture(e->motion.x, e->motion.y);
}

/** 当鼠标按钮在图片容器上释放的时候 */
static void OnPictureMouseUp(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	if (viewer.is_touch_mode) {
		return;
	}
	viewer.drag.is_running = FALSE;
	Widget_UnbindEvent(w, "mouseup", OnPictureMouseUp);
	Widget_UnbindEvent(w, "mousemove", OnPictureMouseMove);
	Widget_ReleaseMouseCapture(w);
}

/** 当鼠标按钮在图片容器上点住的时候 */
static void OnPictureMouseDown(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	if (viewer.is_touch_mode) {
		return;
	}
	viewer.drag.is_running = TRUE;
	viewer.drag.mouse_x = e->motion.x;
	viewer.drag.mouse_y = e->motion.y;
	viewer.drag.focus_x = viewer.focus_x;
	viewer.drag.focus_y = viewer.focus_y;
	Widget_BindEvent(w, "mousemove", OnPictureMouseMove, NULL, NULL);
	Widget_BindEvent(w, "mouseup", OnPictureMouseUp, NULL, NULL);
	Widget_SetMouseCapture(w);
}

static void OnPictureTouchDown(LCUI_TouchPoint point)
{
	viewer.is_touch_mode = TRUE;
	viewer.drag.is_running = TRUE;
	if (!viewer.is_zoom_mode) {
		StartGesture(point->x, point->y);
	}
	viewer.drag.mouse_x = point->x;
	viewer.drag.mouse_y = point->y;
	viewer.drag.focus_x = viewer.focus_x;
	viewer.drag.focus_y = viewer.focus_y;
	Widget_SetTouchCapture(viewer.picture->view, point->id);
}

static void OnPictureTouchUp(LCUI_TouchPoint point)
{
	viewer.is_touch_mode = FALSE;
	viewer.drag.is_running = FALSE;
	if (viewer.gesture.is_running) {
		if (!viewer.zoom.is_running && !viewer.is_zoom_mode) {
			if (HandleGesture() != 0) {
				RestoreSliderPosition();
			}
		}
		StopGesture();
	}
	Widget_ReleaseTouchCapture(viewer.picture->view, -1);
}

static void OnPictureTouchMove(LCUI_TouchPoint point)
{
	DragPicture(point->x, point->y);
	if (viewer.gesture.is_running) {
		UpdateGesture(point->x, point->y);
		SetSliderPosition(point->x - viewer.drag.mouse_x);
	}
}

static void OnPictureTouch(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	double distance, scale;
	int i, x, y, *point_ids;
	LCUI_TouchPoint points[2] = { NULL, NULL };
	if (e->touch.n_points == 0) {
		return;
	}
	point_ids = viewer.zoom.point_ids;
	if (e->touch.n_points == 1) {
		LCUI_TouchPoint point;
		point = &e->touch.points[0];
		switch (point->state) {
		case LCUI_WEVENT_TOUCHDOWN:
			OnPictureTouchDown(point);
			point_ids[0] = point->id;
			break;
		case LCUI_WEVENT_TOUCHMOVE:
			OnPictureTouchMove(point);
			break;
		case LCUI_WEVENT_TOUCHUP:
			OnPictureTouchUp(point);
			point_ids[0] = -1;
			break;
		default:
			break;
		}
		return;
	}
	StopGesture();
	viewer.drag.is_running = FALSE;
	/* 遍历各个触点，确定两个用于缩放功能的触点 */
	for (i = 0; i < e->touch.n_points; ++i) {
		if (point_ids[0] == -1) {
			points[0] = &e->touch.points[i];
			point_ids[0] = points[0]->id;
			continue;
		} else if (point_ids[0] == e->touch.points[i].id) {
			points[0] = &e->touch.points[i];
			continue;
		}
		if (viewer.zoom.point_ids[1] == -1) {
			points[1] = &e->touch.points[i];
			point_ids[1] = points[1]->id;
		} else if (point_ids[1] == e->touch.points[i].id) {
			points[1] = &e->touch.points[i];
		}
	}
	/* 计算两触点的距离 */
	distance = pow(points[0]->x - points[1]->x, 2);
	distance += pow(points[0]->y - points[1]->y, 2);
	distance = sqrt(distance);
	if (!viewer.zoom.is_running) {
		x = points[0]->x - (points[0]->x - points[1]->x) / 2;
		y = points[0]->y - (points[0]->y - points[1]->y) / 2;
		viewer.zoom.distance = (int)distance;
		viewer.zoom.scale = viewer.picture->scale;
		viewer.zoom.is_running = TRUE;
		viewer.zoom.x = x;
		viewer.zoom.y = y;
	} else if (points[0]->state == LCUI_WEVENT_TOUCHMOVE ||
		   points[1]->state == LCUI_WEVENT_TOUCHMOVE) {
		/* 重新计算焦点位置 */
		x = viewer.focus_x - viewer.offset_x;
		y = viewer.focus_y - viewer.offset_y;
		viewer.offset_x = viewer.zoom.x;
		viewer.offset_y = viewer.zoom.y;
		viewer.focus_x = x + viewer.offset_x;
		viewer.focus_y = y + viewer.offset_y;
		x = iround(viewer.focus_x / viewer.picture->scale);
		y = iround(viewer.focus_y / viewer.picture->scale);
		viewer.origin_focus_x = x;
		viewer.origin_focus_y = y;
		scale = viewer.zoom.scale;
		scale *= distance / viewer.zoom.distance;
		SetPictureScale(viewer.picture, scale);
	}
	for (i = 0; i < 2; ++i) {
		int j, id;
		/* 如果当前用于控制缩放的触点已经释放，则找其它触点来代替 */
		if (points[i]->state != LCUI_WEVENT_TOUCHUP) {
			continue;
		}
		point_ids[i] = -1;
		viewer.zoom.is_running = FALSE;
		id = point_ids[i == 0 ? 1 : 0];
		for (j = 0; j < e->touch.n_points; ++j) {
			if (e->touch.points[j].state == LCUI_WEVENT_TOUCHUP ||
			    e->touch.points[j].id == id) {
				continue;
			}
			points[i] = &e->touch.points[j];
			point_ids[i] = points[i]->id;
			i -= 1;
			break;
		}
	}
	if (point_ids[0] == -1) {
		point_ids[0] = point_ids[1];
		point_ids[1] = -1;
		viewer.zoom.is_running = FALSE;
	} else if (point_ids[1] == -1) {
		viewer.zoom.is_running = FALSE;
	}
}

/** 初始化图片实例 */
static Picture CreatePicture(void)
{
	ASSIGN(pic, Picture);
	pic->file = NULL;
	pic->data = malloc(sizeof(LCUI_Graph));
	pic->view = LCUIWidget_New("picture");
	pic->min_scale = pic->scale = 1.0;

	Graph_Init(pic->data);
	Widget_Append(viewer.view_pictures, pic->view);
	BindEvent(pic->view, "resize", OnPictureResize);
	BindEvent(pic->view, "touch", OnPictureTouch);
	BindEvent(pic->view, "mousedown", OnPictureMouseDown);
	return pic;
}

static void DeletePicture(Picture pic)
{
	if (!pic) {
		return;
	}
	if (pic->file) {
		free(pic->file);
		pic->file = NULL;
	}
	if (pic->data) {
		free(pic->data);
		pic->data = NULL;
	}
	pic->view = NULL;
	free(pic);
}

static void DirectSetPictureView(FileIterator iter)
{
	viewer.iterator = iter;
	UpdateSwitchButtons();
}

static void SetPicture(Picture pic, const wchar_t *filepath)
{
	if (pic->file) {
		free(pic->file);
	}
	pic->file = wcsdup2(filepath);
	ClearPictureView(pic);
	PictureLoader_Load(viewer.loader, filepath);
}

static void SetPicturePreloadList(void)
{
	wchar_t *filepath;
	FileIterator iter;

	if (!viewer.iterator) {
		return;
	}
	iter = viewer.iterator;
	if (iter->index < iter->length - 1) {
		iter->next(iter);
		if (iter->filepath) {
			filepath = DecodeUTF8(iter->filepath);
			SetPicture(viewer.pictures[PICTURE_SLOT_NEXT],
				   filepath);
			free(filepath);
		}
		iter->prev(iter);
	}
	if (iter->index > 0) {
		iter->prev(iter);
		if (iter->filepath) {
			filepath = DecodeUTF8(iter->filepath);
			SetPicture(viewer.pictures[PICTURE_SLOT_PREV],
				   filepath);
			free(filepath);
		}
		iter->next(iter);
	}
}

static Picture PictureView_FindPicture(const wchar_t *filepath)
{
	int i;

	if (!viewer.is_working) {
		return NULL;
	}
	for (i = 0; i < 3; ++i) {
		if (viewer.pictures[i]->file &&
		    wcscmp(viewer.pictures[i]->file, filepath) == 0) {
			return viewer.pictures[i];
		}
	}
	return NULL;
}

/** 在”放大“按钮被点击的时候 */
static void OnBtnZoomInClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	double scale;
	Picture pic = viewer.picture;
	ResetOffsetPosition();
	if (!pic->data) {
		return;
	}
	scale = pic->scale * (1.0 + SCALE_STEP);
	SetPictureScale(viewer.picture, scale);
}

/** 在”缩小“按钮被点击的时候 */
static void OnBtnZoomOutClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	double scale;
	Picture pic = viewer.picture;
	if (!pic->data) {
		return;
	}
	ResetOffsetPosition();
	scale = pic->scale * (1.0 - SCALE_STEP);
	SetPictureScale(viewer.picture, scale);
}

static void OnBtnPrevClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	OpenPrevPicture();
}

static void OnBtnNextClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	OpenNextPicture();
}

static void OnBtnDeleteClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	char *path;
	wchar_t *wpath = viewer.picture->file;
	const wchar_t *title = I18n_GetText(KEY_DELETE);

	if (!LCUIDialog_Confirm(viewer.window, title, NULL)) {
		return;
	}
	MoveFileToTrashW(wpath);
	ClearPictureView(viewer.picture);
	path = EncodeUTF8(wpath);
	viewer.iterator->unlink(viewer.iterator);
	LCFinder_DeleteFiles(&path, 1, NULL, NULL);
	LCFinder_TriggerEvent(EVENT_FILE_DEL, path);
	free(path);

	if (viewer.iterator->length == 0) {
		LCUI_PostSimpleTask(TaskForHideTipEmpty, NULL, NULL);
		if (viewer.mode == MODE_SINGLE_PICVIEW) {
			LCUI_Quit();
		} else {
			UI_ClosePictureView();
		}
	} else {
		UI_OpenPictureView(viewer.iterator->filepath);
	}
}

/** 在”实际大小“按钮被点击的时候 */
static void OnBtnResetClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	double scale;
	if (viewer.picture->scale != viewer.picture->min_scale) {
		scale = viewer.picture->min_scale;
	} else {
		scale = 1.0;
	}
	ResetOffsetPosition();
	SetPictureScale(viewer.picture, scale);
}

static void ToggleSidePanel(PictureSidePanel panel)
{
	LCUI_BOOL visible = FALSE;

	switch (viewer.active_panel) {
	case SIDE_PANEL_INFO:
		visible = PictureView_VisibleInfo();
		PictureView_HideInfo();
		break;
	case SIDE_PANEL_LABELS:
		visible = PictureView_VisibleLabels();
		PictureView_HideLabels();
		break;
	default:
		break;
	}
	if (viewer.active_panel == panel && visible) {
		viewer.active_panel = SIDE_PANEL_NONE;
		return;
	}
	switch (panel) {
	case SIDE_PANEL_INFO:
		PictureView_ShowInfo();
		break;
	case SIDE_PANEL_LABELS:
		PictureView_ShowLabels();
		break;
	default:
		break;
	}
	viewer.active_panel = panel;
}

static void OnBtnShowInfoClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	ToggleSidePanel(SIDE_PANEL_INFO);
}

static void OnBtnShowLabelsClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	ToggleSidePanel(SIDE_PANEL_LABELS);
}

static void SetPictureFocusPoint(Picture pic, int focus_x, int focus_y)
{
	int x, y;
	float width, height;
	/* 获取浏览区域的位置 */
	x = focus_x - viewer.offset_x;
	y = focus_y - viewer.offset_y;
	width = (float)(pic->scale * pic->data->width);
	height = (float)(pic->scale * pic->data->height);
	/* 如果缩放后的尺寸小于浏览区域，则调整偏移量 */
	if (width < pic->view->width) {
		viewer.offset_x += (int)((pic->view->width - width) / 2);
	}
	if (height < pic->view->height) {
		viewer.offset_y += (int)((pic->view->height - height) / 2);
	}
	/* 调整浏览区域坐标 */
	if (x < 0) {
		x = 0;
	}
	if (x + pic->view->width > width) {
		x = iround(width - pic->view->width);
	}
	if (y < 0) {
		y = 0;
	}
	if (y + pic->view->height > height) {
		y = iround(height - pic->view->height);
	}
	/* 更新焦点位置 */
	viewer.focus_x = x + viewer.offset_x;
	viewer.focus_y = y + viewer.offset_y;
	viewer.origin_focus_x = iround(viewer.focus_x / pic->scale);
	viewer.origin_focus_y = iround(viewer.focus_y / pic->scale);
	UpdatePicturePosition(pic);
}

static void OnMouseWheel(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	int x, y;
	float width, height;
	double scale = viewer.picture->scale;
	Picture pic = viewer.picture;

	/* 获取当前浏览区域的位置 */
	x = viewer.focus_x - viewer.offset_x;
	y = viewer.focus_y - viewer.offset_y;
	/* 更新区域偏移量 */
	viewer.offset_x = e->wheel.x;
	viewer.offset_y = e->wheel.y;
	width = (float)(scale * pic->data->width);
	height = (float)(scale * pic->data->height);
	/* 如果缩放后的尺寸小于浏览区域，则调整偏移量 */
	if (width < pic->view->width) {
		viewer.offset_x += (int)((pic->view->width - width) / 2);
	}
	if (height < pic->view->height) {
		viewer.offset_y += (int)((pic->view->height - height) / 2);
	}
	/* 调整浏览区域坐标 */
	if (x < 0) {
		x = 0;
	}
	if (x + pic->view->width > width) {
		x = iround(width - pic->view->width);
	}
	if (y < 0) {
		y = 0;
	}
	if (y + pic->view->height > height) {
		y = iround(height - pic->view->height);
	}
	/* 更新焦点位置 */
	viewer.focus_x = x + viewer.offset_x;
	viewer.focus_y = y + viewer.offset_y;
	viewer.origin_focus_x = iround(viewer.focus_x / pic->scale);
	viewer.origin_focus_y = iround(viewer.focus_y / pic->scale);
	if (e->wheel.delta < 0) {
		SetPictureZoomOut(pic);
	} else {
		SetPictureZoomIn(pic);
	}
}

static void OnKeyDown(LCUI_SysEvent e, void *data)
{
	int focus_x = viewer.focus_x;
	int focus_y = viewer.focus_y;
	LCUI_Widget w;

	for (w = LCUIWidget_GetFocus(); w; w = w->parent) {
		if (Widget_HasClass(w, "labelbox-box")) {
			return;
		}
	}
	switch (e->key.code) {
	case LCUI_KEY_MINUS:
		SetPictureZoomOut(viewer.picture);
		break;
	case LCUI_KEY_EQUAL:
		SetPictureZoomIn(viewer.picture);
		break;
	case LCUI_KEY_LEFT:
		if (!viewer.is_zoom_mode) {
			OpenPrevPicture();
			break;
		}
		focus_x -= MOVE_STEP;
		SetPictureFocusPoint(viewer.picture, focus_x, focus_y);
		break;
	case LCUI_KEY_RIGHT:
		if (!viewer.is_zoom_mode) {
			OpenNextPicture();
			break;
		}
		focus_x += MOVE_STEP;
		SetPictureFocusPoint(viewer.picture, focus_x, focus_y);
		break;
	case LCUI_KEY_UP:
		if (!viewer.is_zoom_mode) {
			break;
		}
		focus_y -= MOVE_STEP;
		SetPictureFocusPoint(viewer.picture, focus_x, focus_y);
		break;
	case LCUI_KEY_DOWN:
		if (!viewer.is_zoom_mode) {
			break;
		}
		focus_y += MOVE_STEP;
		SetPictureFocusPoint(viewer.picture, focus_x, focus_y);
	default:
		break;
	}
}

static void PictureView_OnProgress(const wchar_t *filepath, float progress)
{
	if (PictureView_FindPicture(filepath) == viewer.picture) {
		ProgressBar_SetValue(viewer.tip_progress, iround(0.5 * 100));
		Widget_Show(viewer.tip_loading);
	}
}

static void PictureView_OnSuccess(const wchar_t *filepath, LCUI_Graph *img)
{
	Picture pic = PictureView_FindPicture(filepath);
	if (!pic) {
		return;
	}
	pic->data = malloc(sizeof(LCUI_Graph));
	*pic->data = *img;
	SetPictureView(pic);
	ResetPictureSize(pic);
	if (viewer.picture->view == pic->view) {
		Widget_Hide(viewer.tip_loading);
		Widget_Hide(viewer.tip_unsupport);
		UpdateResetSizeButton();
		UpdateZoomButtons();
	}
}

static void PictureView_OnError(const wchar_t *filepath)
{
	Picture pic = PictureView_FindPicture(filepath);
	if (pic) {
		ClearPictureView(pic);
	}
	if (viewer.picture->view == pic->view) {
		Widget_Hide(viewer.tip_loading);
		Widget_Show(viewer.tip_unsupport);
		UpdateResetSizeButton();
		UpdateZoomButtons();
	}
}

static void PictureView_OnFree(const wchar_t *filepath)
{
	Picture pic = PictureView_FindPicture(filepath);
	if (pic) {
		ClearPictureView(pic);
	}
}

void UI_InitPictureView(int mode)
{
	LCUI_Widget box, btn_back, btn_back2, btn_info, btn_label, btn_del;

	box = LCUIBuilder_LoadFile(FILE_PICTURE_VIEW);
	if (box) {
		Widget_Top(box);
		Widget_Unwrap(box);
	}
	viewer.mode = mode;
	viewer.is_working = TRUE;
	viewer.is_opening = FALSE;
	viewer.zoom.point_ids[0] = -1;
	viewer.zoom.point_ids[1] = -1;
	viewer.zoom.is_running = FALSE;
	viewer.active_panel = SIDE_PANEL_NONE;
	SelectWidget(btn_back, ID_BTN_BROWSE_ALL);
	SelectWidget(btn_del, ID_BTN_DELETE_PICTURE);
	SelectWidget(btn_info, ID_BTN_SHOW_PICTURE_INFO);
	SelectWidget(btn_label, ID_BTN_SHOW_PICTURE_LABEL);
	SelectWidget(btn_back2, ID_BTN_HIDE_PICTURE_VIEWER);
	SelectWidget(viewer.btn_prev, ID_BTN_PCITURE_PREV);
	SelectWidget(viewer.btn_next, ID_BTN_PCITURE_NEXT);
	SelectWidget(viewer.btn_zoomin, ID_BTN_PICTURE_ZOOM_IN);
	SelectWidget(viewer.btn_zoomout, ID_BTN_PICTURE_ZOOM_OUT);
	SelectWidget(viewer.btn_reset, ID_BTN_PICTURE_RESET_SIZE);
	SelectWidget(viewer.window, ID_WINDOW_PCITURE_VIEWER);
	SelectWidget(viewer.tip_empty, ID_TIP_PICTURE_NOT_FOUND);
	SelectWidget(viewer.tip_loading, ID_TIP_PICTURE_LOADING);
	SelectWidget(viewer.tip_unsupport, ID_TIP_PICTURE_UNSUPPORT);
	SelectWidget(viewer.view_pictures, ID_VIEW_PICTURE_TARGET);
	BindEvent(btn_back, "click", OnBtnBackClick);
	BindEvent(btn_back2, "click", OnBtnBackClick);
	BindEvent(btn_del, "click", OnBtnDeleteClick);
	BindEvent(btn_info, "click", OnBtnShowInfoClick);
	BindEvent(btn_label, "click", OnBtnShowLabelsClick);
	BindEvent(viewer.btn_prev, "click", OnBtnPrevClick);
	BindEvent(viewer.btn_next, "click", OnBtnNextClick);
	BindEvent(viewer.btn_reset, "click", OnBtnResetClick);
	BindEvent(viewer.btn_zoomin, "click", OnBtnZoomInClick);
	BindEvent(viewer.btn_zoomout, "click", OnBtnZoomOutClick);
	BindEvent(viewer.view_pictures, "mousewheel", OnMouseWheel);
	BindEvent(viewer.view_pictures, "dblclick", OnMouseDblClick);
	LCUI_BindEvent(LCUI_KEYDOWN, OnKeyDown, NULL, NULL);
	InitSlideTransition();
	viewer.pictures[PICTURE_SLOT_PREV] = CreatePicture();
	viewer.pictures[PICTURE_SLOT_CURRENT] = CreatePicture();
	viewer.pictures[PICTURE_SLOT_NEXT] = CreatePicture();
	viewer.picture = viewer.pictures[PICTURE_SLOT_PREV];
	viewer.tip_progress = LCUIWidget_New("progress");
	viewer.loader = PictureLoader_Create(
	    finder.storage_for_image, PictureView_OnProgress,
	    PictureView_OnSuccess, PictureView_OnError, PictureView_OnFree);
	ProgressBar_SetMaxValue(viewer.tip_progress, 100);
	Widget_Append(viewer.tip_loading, viewer.tip_progress);
	Widget_Hide(viewer.window);
	PictureView_InitInfo();
	PictureView_InitLabels();
	UpdateSwitchButtons();
	if (viewer.mode == MODE_SINGLE_PICVIEW) {
		viewer.scanner =
		    PictureView_CreateScanner(finder.storage_for_scan);
		Widget_SetStyle(btn_back2, key_display, SV_NONE, style);
		Widget_Hide(btn_back2);
	} else {
		Widget_Destroy(btn_back);
	}
}

static void PictureView_Load(const wchar_t *filepath)
{
	SetPicture(viewer.picture, filepath);
	SetPicturePreloadList();
}

void UI_OpenPictureView(const char *filepath)
{
	wchar_t *wpath, title[256] = { 0 };
	LCUI_Widget main_window, root;

	root = LCUIWidget_GetRoot();
	wpath = DecodeUTF8(filepath);
	viewer.is_opening = TRUE;
	viewer.is_zoom_mode = FALSE;
	viewer.picture = viewer.pictures[PICTURE_SLOT_CURRENT];
	SelectWidget(main_window, ID_WINDOW_MAIN);
	swprintf(title, 255, L"%ls - " LCFINDER_NAME, wgetfilename(wpath));
	Widget_SetTitleW(root, title);
	Widget_Hide(viewer.tip_unsupport);
	Widget_Hide(viewer.tip_empty);
	PictureView_Load(wpath);
	PictureView_SetInfo(filepath);
	PictureView_SetLabels();
	Widget_Show(viewer.window);
	Widget_Hide(main_window);
	if (viewer.mode == MODE_SINGLE_PICVIEW) {
		PictureView_OpenScanner(viewer.scanner, wpath,
					DirectSetPictureView,
					SetPicturePreloadList);
	}
	free(wpath);
}

void UI_SetPictureView(FileIterator iter)
{
	if (viewer.iterator) {
		viewer.iterator->destroy(viewer.iterator);
		viewer.iterator = NULL;
	}
	DirectSetPictureView(iter);
}

void UI_ClosePictureView(void)
{
	LCUI_Widget main_window;
	LCUI_Widget root = LCUIWidget_GetRoot();

	SelectWidget(main_window, ID_WINDOW_MAIN);
	Widget_SetTitleW(root, LCFINDER_NAME);
	ResetPictureSize(viewer.picture);
	Widget_Show(main_window);
	Widget_Hide(viewer.window);
	viewer.is_opening = FALSE;
	UI_SetPictureView(NULL);
}

void UI_FreePictureView(void)
{
	viewer.is_working = FALSE;
	if (viewer.mode == MODE_SINGLE_PICVIEW) {
		PictureView_CloseScanner(viewer.scanner);
	}
	PictureLoader_Destroy(viewer.loader);
	DeletePicture(viewer.pictures[PICTURE_SLOT_PREV]);
	DeletePicture(viewer.pictures[PICTURE_SLOT_CURRENT]);
	DeletePicture(viewer.pictures[PICTURE_SLOT_NEXT]);
	viewer.pictures[PICTURE_SLOT_PREV] = NULL;
	viewer.pictures[PICTURE_SLOT_CURRENT] = NULL;
	viewer.pictures[PICTURE_SLOT_NEXT] = NULL;
}
