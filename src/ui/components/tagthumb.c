/* ***************************************************************************
 * tagthumb.c -- tag thumbnail widget
 *
 * Copyright (C) 2018 by Liu Chao <lc-soft@live.cn>
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
 * tagthumb.c -- 标签缩略图部件
 *
 * 版权所有 (C) 2018 归属于 刘超 <lc-soft@live.cn>
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
#include "textview_i18n.h"
#include "thumbview.h"
#include "tagthumb.h"
#include "i18n.h"

#define KEY_ITEM "tagthumb.item"
#define KEY_ITEMS "tagthumb.items"

typedef struct TagThumbRec_ {
	size_t count;

	LCUI_Widget cover;
	LCUI_Widget txt_name;
	LCUI_Widget txt_count;
} TagThumbRec, *TagThumb;

static struct TagThumbModule {
	LCUI_WidgetPrototype proto;
} self;

static void TagThumb_RenderCount(wchar_t *out, const wchar_t *str, void *data)
{
	TagThumb that = data;
	const wchar_t *template;
	wchar_t numstr[32] = { 0 };

	get_human_number_wcs(numstr, 63, that->count);
	if (that->count > 1) {
		template = I18n_GetText(KEY_ITEMS);
	} else {
		template = I18n_GetText(KEY_ITEM);
	}
	swprintf(out, 48, template, numstr);
}

static void TagThumb_OnInit(LCUI_Widget w)
{
	TagThumb that = Widget_AddData(w, self.proto, sizeof(TagThumbRec));
	LCUI_Widget check = LCUIWidget_New("textview");
	LCUI_Widget dimmer = LCUIWidget_New(NULL);
	LCUI_Widget info = LCUIWidget_New(NULL);

	that->cover = LCUIWidget_New(NULL);
	that->txt_name = LCUIWidget_New("textview");
	that->txt_count = LCUIWidget_New("textview-i18n");

	Widget_AddClass(w, "tag-thumb");
	Widget_AddClass(info, "tag-thumb-info");
	Widget_AddClass(that->cover, "tag-thumb-cover");
	Widget_AddClass(dimmer, "tag-thumb-dimmer");
	Widget_AddClass(check, "icon icon-checkbox-marked-circle tag-thumb-checkbox");
	Widget_AddClass(that->txt_name, "text name");
	Widget_AddClass(that->txt_count, "text count");

	TextViewI18n_SetKey(that->txt_count, KEY_ITEMS);
	TextViewI18n_SetFormater(that->txt_count, TagThumb_RenderCount, that);

	Widget_Append(dimmer, check);
	Widget_Append(that->cover, dimmer);
	Widget_Append(info, that->txt_name);
	Widget_Append(info, that->txt_count);
	Widget_Append(w, that->cover);
	Widget_Append(w, info);

	self.proto->proto->init(w);
}

static void TagThumb_OnDestroy(LCUI_Widget w)
{
	ThumbViewItem_SetFunction(w, NULL, NULL, NULL);
	w->proto->proto->destroy(w);
}

void TagThumb_SetName(LCUI_Widget w, const char *name)
{
	TagThumb that = Widget_GetData(w, self.proto);
	TextView_SetText(that->txt_name, name);
}

void TagThumb_SetCount(LCUI_Widget w, size_t count)
{
	TagThumb that;

	that = Widget_GetData(w, self.proto);
	that->count = count;

	TextViewI18n_Refresh(that->txt_count);
}

void TagThumb_SetCover(LCUI_Widget w, LCUI_Graph *cover)
{
	TagThumb that = Widget_GetData(w, self.proto);
	Widget_SetStyle(that->cover, key_background_image, cover, image);
	Widget_UpdateStyle(that->cover, FALSE);
}

void TagThumb_RemoveCover(LCUI_Widget w)
{
	TagThumb that = Widget_GetData(w, self.proto);
	Graph_Init(&w->computed_style.background.image);
	Widget_UnsetStyle(that->cover, key_background_image);
	Widget_UpdateStyle(that->cover, FALSE);
}

void LCUIWidget_AddTagThumb(void)
{
	self.proto = LCUIWidget_NewPrototype("tagthumb", "thumbviewitem");
	self.proto->init = TagThumb_OnInit;
	self.proto->destroy = TagThumb_OnDestroy;
}
