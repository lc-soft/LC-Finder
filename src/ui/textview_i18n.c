/* ***************************************************************************
 * textview_i18n.c -- textview internationalization version
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
 * textview_i18n.c -- 文本显示部件的国际化版本
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
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "textview_i18n.h"
#include "i18n.h"

#define WIDGET_NAME "textview-i18n"

typedef struct TextViewI18nRec_ {
	char *key;
	char *text;
	LCUI_BOOL key_valid;
	LinkedListNode node;
} TextViewI18nRec, *TextViewI18n;

static struct TextViewI18nModule {
	LinkedList textviews;
	LCUI_WidgetPrototype prototype;
} self;

static void TextViewI18n_Init( LCUI_Widget w )
{
	const size_t data_size = sizeof( TextViewI18nRec );
	TextViewI18n txt = Widget_AddData( w, self.prototype, data_size );

	txt->node.data = w;
	txt->key_valid = FALSE;
	txt->key = txt->text = NULL;
	LinkedList_Append( &self.textviews, &txt->node );
	self.prototype->proto->init( w );
}

static void TextViewI18n_Destroy( LCUI_Widget w )
{
	TextViewI18n txt = Widget_GetData( w, self.prototype );
	LinkedList_Unlink( &self.textviews, &txt->node );
}

void TextViewI18n_Refresh( LCUI_Widget w )
{
	const char *text;
	TextViewI18n textview;

	textview = Widget_GetData( w, self.prototype );
	if( textview->key ) {
		text = I18n_GetText( textview->key );
		if( text ) {
			textview->key_valid = TRUE;
			TextView_SetText( w, text );
			return;
		}
		textview->key_valid = FALSE;
	}
	if( textview->text ) {
		TextView_SetText( w, textview->text );
	}
}

void TextViewI18n_SetKey( LCUI_Widget w, const char *key )
{
	TextViewI18n txt = Widget_GetData( w, self.prototype );
	if( txt->key ) {
		free( txt->key );
	}
	txt->key = strdup( key );
	TextViewI18n_Refresh( w );
}

static void TextViewI18n_SetAttr( LCUI_Widget w, const char *name, 
				  const char *value  )
{
	if( strcasecmp( name, "data-i18n-key" ) == 0 ) {
		TextViewI18n_SetKey( w, value );
	}
	if( w->proto->proto->setattr ) {
		w->proto->proto->setattr( w, name, value );
	}
}

static void TextViewI18n_SetText( LCUI_Widget w, const char *text  )
{
	size_t len = strlen( text ) + 1;
	TextViewI18n txt = Widget_GetData( w, self.prototype );

	if( txt->text ) {
		free( txt->text );
	}
	txt->text = malloc( sizeof( char ) * len );
	strncpy( txt->text, text, len );
	if( !txt->key_valid ) {
		TextView_SetText( w, text );
	}
}

void LCUIWidget_RefreshTextViewI18n( void )
{
	LinkedListNode *node;
	for( LinkedList_Each( node, &self.textviews ) ) {
		TextViewI18n_Refresh( node->data );
	}
}

void LCUIWidget_AddTextViewI18n( void )
{
	/* 继承自 textview */
	self.prototype = LCUIWidget_NewPrototype( WIDGET_NAME, "textview" );
	self.prototype->init = TextViewI18n_Init;
	self.prototype->destroy = TextViewI18n_Destroy;
	self.prototype->setattr = TextViewI18n_SetAttr;
	self.prototype->settext = TextViewI18n_SetText;
}
