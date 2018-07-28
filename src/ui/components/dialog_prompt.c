/* ***************************************************************************
 * dialog_input.c -- input dialog, used for pop-up dialog to get user input
 * string.
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
 * dialog_input.c -- 输入对话框，用于弹出提示框获取用户输入的字符串。
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
#include <LCUI/gui/widget/button.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/widget/textedit.h>
#include "i18n.h"
#include "dialog.h"

#define KEY_OK		"button.ok"
#define KEY_CANCEL	"button.cancel"

typedef struct DialogContextRec_ {
	int result;
	size_t len;
	size_t max_len;
	wchar_t *text;
	const wchar_t *old_text;
	LCUI_BOOL (*checker)(const wchar_t*);
	LCUI_Widget input;
	LCUI_Widget btn;
	LCUI_MainLoop loop;
} DialogContextRec, *DialogContext;

static void OnInputChange( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	DialogContext ctx = e->data;
	ctx->len = TextEdit_GetTextW( ctx->input, 0, ctx->max_len, ctx->text );
	if( ctx->len > 0 ) {
		if( ctx->checker && !ctx->checker(ctx->text) ) {
			Widget_AddClass( w, "has-error" );
			Widget_SetDisabled( ctx->btn, TRUE );
		} else {
			Widget_RemoveClass( w, "has-error" );
			Widget_SetDisabled( ctx->btn, FALSE );
		}
	} else {
		Widget_SetDisabled( ctx->btn, TRUE );
	}
}

static void OnBtnOkClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	DialogContext ctx = e->data;
	if( w->disabled ) {
		return;
	}
	ctx->len = TextEdit_GetTextW( ctx->input, 0, ctx->max_len, ctx->text );
	if( ctx->len == 0 ) {
		return;
	}
	ctx->result = 0;
	LCUIMainLoop_Quit( ctx->loop );
}

static void OnBtnCancelClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	DialogContext ctx = e->data;
	ctx->result = -1;
	LCUIMainLoop_Quit( ctx->loop );
}

int LCUIDialog_Prompt( LCUI_Widget parent, const wchar_t* title,
		       const wchar_t *placeholder, const wchar_t *val,
		       wchar_t *newval, size_t max_len,
		       LCUI_BOOL( *checker )(const wchar_t*) )
{
	DialogContextRec ctx;
	LCUI_Widget dialog_text, box;
	LCUI_Widget dialog = LCUIWidget_New( NULL );
	LCUI_Widget dialog_body = LCUIWidget_New( NULL );
	LCUI_Widget dialog_content = LCUIWidget_New( NULL );
	LCUI_Widget dialog_header = LCUIWidget_New( NULL );
	LCUI_Widget dialog_footer = LCUIWidget_New( NULL );
	LCUI_Widget btn_cancel = LCUIWidget_New( "button" );
	LCUI_Widget btn_ok = LCUIWidget_New( "button" );
	ctx.btn = btn_ok;
	ctx.text = newval;
	ctx.old_text = val;
	ctx.max_len = max_len;
	ctx.checker = checker;
	ctx.loop = LCUIMainLoop_New();
	ctx.input = LCUIWidget_New( "textedit" );
	Widget_AddClass( dialog, "dialog" );
	Widget_AddClass( dialog_body, "dialog-body" );
	Widget_AddClass( dialog_content, "dialog-content" );
	Widget_AddClass( dialog_header, "dialog-header" );
	Widget_AddClass( dialog_footer, "dialog-footer" );
	Widget_AddClass( ctx.input, "dialog-input full-width" );
	dialog_text = LCUIWidget_New( "textview" );
	TextEdit_SetTextW( ctx.input, val );
	TextEdit_SetPlaceHolderW( ctx.input, placeholder );
	TextView_SetTextW( dialog_text, title );
	Widget_Append( dialog_header, dialog_text );
	Widget_Append( dialog_body, ctx.input );
	box = LCUIWidget_New( NULL );
	Widget_AddClass( box, "dialog-btn-group" );
	Widget_AddClass( btn_ok, "dialog-btn" );
	Button_SetTextW( btn_ok, I18n_GetText( KEY_OK ) );
	Widget_Append( box, btn_ok );
	Widget_Append( dialog_footer, box );
	box = LCUIWidget_New( NULL );
	Widget_AddClass( box, "dialog-btn-group" );
	Widget_AddClass( btn_cancel, "dialog-btn" );
	Button_SetTextW( btn_cancel, I18n_GetText( KEY_CANCEL ) );
	Widget_Append( box, btn_cancel );
	Widget_Append( dialog_footer, box );
	Widget_Append( dialog_content, dialog_header );
	Widget_Append( dialog_content, dialog_body );
	Widget_Append( dialog_content, dialog_footer );
	Widget_Append( dialog, dialog_content );
	Widget_Append( parent, dialog );
	Widget_SetDisabled( btn_ok, TRUE );
	Widget_BindEvent( btn_ok, "click", OnBtnOkClick, &ctx, NULL );
	Widget_BindEvent( btn_cancel, "click", OnBtnCancelClick, &ctx, NULL );
	Widget_BindEvent( ctx.input, "change", OnInputChange, &ctx, NULL );
	LCUIWidget_SetFocus( ctx.input );
	LCUIMainLoop_Run( ctx.loop );
	Widget_Destroy( dialog );
	return ctx.result;
}
