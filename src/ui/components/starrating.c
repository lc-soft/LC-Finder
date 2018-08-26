/* ***************************************************************************
 * starrating.c -- star rating widget
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
 * starrating.c -- 星级评分部件
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

#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/css_parser.h>

/* clang-format off */

static LCUI_WidgetPrototype prototype = NULL;

static const char *starrating_css = CodeToString(

starrating .star {
	width: 24px;
	height: 24px;
	font-size: 18px;
	line-height: 24px;
	display: inline-block;
}

);

typedef struct StarRatingRec_ {
	int rating;
	int max_rating;
} StarRatingRec, *StarRating;

/* clang-format on */

static void UpdateRating(LCUI_Widget w, int rating)
{
	LCUI_Widget child;
	LinkedListNode *node;
	for (LinkedList_Each(node, &w->children)) {
		child = node->data;
		if (child->index >= rating) {
			break;
		}
		Widget_AddClass(child, "icon-star");
		Widget_RemoveClass(child, "icon-star-outline");
	}
	for (; node; node = node->next) {
		child = node->data;
		Widget_RemoveClass(child, "icon-star");
		Widget_AddClass(child, "icon-star-outline");
	}
}

static void OnMouseMove(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	int ix, iy;
	float x, y;
	LCUI_Widget target;
	Widget_GetOffset(w, NULL, &x, &y);
	ix = e->motion.x - iround(x);
	iy = e->motion.y - iround(y);
	target = Widget_At(w, ix, iy);
	if (!target) {
		return;
	}
	UpdateRating(w, target->index + 1);
}

static void OnMouseDown(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	int ix, iy;
	float x, y;
	LCUI_Widget target;
	StarRating data = Widget_GetData(w, prototype);
	Widget_GetOffset(w, NULL, &x, &y);
	ix = e->motion.x - iround(x);
	iy = e->motion.y - iround(y);
	target = Widget_At(w, ix, iy);
	if (!target) {
		return;
	}
	data->rating = target->index + 1;
	if (data->rating > data->max_rating) {
		data->rating = data->max_rating;
	}
	UpdateRating(w, data->rating);
}

static void OnMouseOut(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	StarRating data = Widget_GetData(w, prototype);
	UpdateRating(w, data->rating);
}

static void OnInit(LCUI_Widget w)
{
	int i;
	const size_t data_size = sizeof(StarRatingRec);
	StarRating data = Widget_AddData(w, prototype, data_size);

	data->rating = 0;
	data->max_rating = 5;
	for (i = 0; i < data->max_rating; ++i) {
		LCUI_Widget child = LCUIWidget_New("textview");
		Widget_AddClass(child, "star icon icon-star-outline");
		Widget_Append(w, child);
	}
	Widget_BindEvent(w, "mousemove", OnMouseMove, NULL, NULL);
	Widget_BindEvent(w, "mousedown", OnMouseDown, NULL, NULL);
	Widget_BindEvent(w, "mouseout", OnMouseOut, NULL, NULL);
}

void StarRating_SetRating(LCUI_Widget w, int rating)
{
	StarRating data = Widget_GetData(w, prototype);
	if (rating > data->max_rating) {
		rating = data->max_rating;
	}
	data->rating = rating;
	UpdateRating(w, rating);
}

int StarRating_GetRating(LCUI_Widget w)
{
	StarRating data = Widget_GetData(w, prototype);
	return data->rating;
}

void LCUIWidget_AddStarRating(void)
{
	prototype = LCUIWidget_NewPrototype("starrating", NULL);
	prototype->init = OnInit;
	LCUI_LoadCSSString(starrating_css, NULL);
}
