/* ***************************************************************************
 * dialog_password.c -- password dialog, used for pop-up dialog to check
 * password or input new password.
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
 * dialog_confirm.c --  密码对话框，用于弹出提示框确认密码或设置新密码
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

#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/button.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/widget/textedit.h>
#include "i18n.h"
#include "dialog.h"

#define TYPE_NEW_PASSWORD	0
#define TYPE_VERIFY_PASSWORD	1

#define KEY_OK			"button.ok"
#define KEY_CANCEL		"button.cancel"
#define KEY_MSG_PWD_ERR		"message.password_error"
#define KEY_MSG_PWD_EMPTY	"message.password_empty"
#define KEY_MSG_PWD_TOO_LONG	"message.password_too_long"
#define KEY_MSG_PWD_NOT_MATCH	"message.passwords_not_match"
#define KEY_TXT_PWD		"dialog.placeholder.password"
#define KEY_TXT_PWD2		"dialog.placeholder.confirm_password"
#define PWD_MAX_LEN		48

typedef struct DialogContextRec_ {
	int type;
	int result;
	const char *data;
	LCUI_BOOL (*check)(const char*, const char*);
	LCUI_Widget input;
	LCUI_Widget input2;
	LCUI_Widget msg;
	LCUI_Widget msg2;
	LCUI_Widget btn;
	LCUI_MainLoop loop;
} DialogContextRec, *DialogContext;

static void OnInputChange( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	size_t len;
	wchar_t pwd[256] = { 0 };
	DialogContext ctx = e->data;
	len = TextEdit_GetTextW( ctx->input, 0, 255, pwd );
	if( len == 0 ) {
		Widget_SetDisabled( ctx->btn, TRUE );
	} else {
		Widget_SetDisabled( ctx->btn, FALSE );
	}
}

static void OnBtnOkClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	size_t len;
	const wchar_t *text;
	wchar_t pwd[256] = { 0 };
	wchar_t pwd2[256] = { 0 };
	DialogContext ctx = e->data;
	if( w->disabled ) {
		return;
	}
	if( ctx->type == TYPE_VERIFY_PASSWORD ) {
		char *str;
		len = TextEdit_GetTextW( ctx->input, 0, 255, pwd );
		if( len == 0 ) {
			return;
		}
		str = EncodeUTF8( pwd );
		if( !ctx->check( str, ctx->data ) ) {
			Widget_AddClass( ctx->input->parent, "error" );
			return;
		}
		Widget_RemoveClass( ctx->input->parent, "error" );
		ctx->result = 0;
		LCUIMainLoop_Quit( ctx->loop );
		free( str );
		return;
	}
	len = TextEdit_GetTextW( ctx->input, 0, 255, pwd );
	if( len == 0 ) {
		text = I18n_GetText( KEY_MSG_PWD_EMPTY );
		TextView_SetTextW( ctx->msg, text );
		Widget_SetDisabled( ctx->btn, TRUE );
		Widget_AddClass( ctx->input->parent, "error" );
		return;
	} else if( len > PWD_MAX_LEN ) {
		text = I18n_GetText( KEY_MSG_PWD_TOO_LONG );
		TextView_SetTextW( ctx->msg, text );
		Widget_SetDisabled( ctx->btn, TRUE );
		Widget_AddClass( ctx->input->parent, "error" );
	} else {
		Widget_RemoveClass( ctx->input->parent, "error" );
	}
	len = TextEdit_GetTextW( ctx->input2, 0, 255, pwd2 );
	if( wcscmp( pwd, pwd2 ) != 0 ) {
		text = I18n_GetText( KEY_MSG_PWD_NOT_MATCH );
		TextView_SetTextW( ctx->msg2, text );
		Widget_SetDisabled( ctx->btn, TRUE );
		Widget_AddClass( ctx->input2->parent, "error" );
		return;
	} else {
		Widget_RemoveClass( ctx->input2->parent, "error" );
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

int LCUIDialog_CheckPassword( LCUI_Widget parent, const wchar_t *title, 
			      const wchar_t *text,
			      LCUI_BOOL( *check )(const char*, const char*),
			      const char *data )
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
	ctx.data = data;
	ctx.check = check;
	ctx.type = TYPE_VERIFY_PASSWORD;
	ctx.loop = LCUIMainLoop_New();
	ctx.input = LCUIWidget_New( "textedit" );
	Widget_AddClass( dialog, "dialog" );
	Widget_AddClass( dialog_body, "dialog-body" );
	Widget_AddClass( dialog_content, "dialog-content" );
	Widget_AddClass( dialog_header, "dialog-header" );
	Widget_AddClass( dialog_footer, "dialog-footer" );
	Widget_AddClass( ctx.input, "dialog-input full-width" );
	/* title */
	dialog_text = LCUIWidget_New( "textview" );
	TextView_SetTextW( dialog_text, title );
	Widget_Append( dialog_header, dialog_text );
	/* text */
	dialog_text = LCUIWidget_New( "textview" );
	Widget_AddClass( dialog_text, "text-block" );
	TextView_SetTextW( dialog_text, text );
	Widget_Append( dialog_body, dialog_text );
	/* password input */
	box = LCUIWidget_New( NULL );
	dialog_text = LCUIWidget_New( "textview" );
	TextEdit_SetPasswordChar( ctx.input, L'●' );
	TextEdit_SetPlaceHolderW( ctx.input, I18n_GetText( KEY_TXT_PWD ) );
	TextView_SetTextW( dialog_text, I18n_GetText( KEY_MSG_PWD_ERR ) );
	Widget_AddClass( dialog_text, "error message" );
	Widget_AddClass( box, "input-group" );
	Widget_Append( box, ctx.input );
	Widget_Append( box, dialog_text );
	Widget_Append( dialog_body, box );
	/* footer buttons */
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
	/* build dialog */
	Widget_Append( dialog_content, dialog_header );
	Widget_Append( dialog_content, dialog_body );
	Widget_Append( dialog_content, dialog_footer );
	Widget_Append( dialog, dialog_content );
	Widget_Append( parent, dialog );
	/* bind events */
	Widget_SetDisabled( btn_ok, TRUE );
	Widget_BindEvent( btn_ok, "click", OnBtnOkClick, &ctx, NULL );
	Widget_BindEvent( btn_cancel, "click", OnBtnCancelClick, &ctx, NULL );
	Widget_BindEvent( ctx.input, "change", OnInputChange, &ctx, NULL );
	LCUIWidget_SetFocus( ctx.input );
	LCUIMainLoop_Run( ctx.loop );
	Widget_Destroy( dialog );
	return ctx.result;
}

int LCUIDialog_NewPassword( LCUI_Widget parent, const wchar_t *title,
			    const wchar_t *text, wchar_t *password )
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
	ctx.type = TYPE_NEW_PASSWORD;
	ctx.loop = LCUIMainLoop_New();
	ctx.input = LCUIWidget_New( "textedit" );
	ctx.input2 = LCUIWidget_New( "textedit" );
	Widget_AddClass( dialog, "dialog" );
	Widget_AddClass( dialog_body, "dialog-body" );
	Widget_AddClass( dialog_content, "dialog-content" );
	Widget_AddClass( dialog_header, "dialog-header" );
	Widget_AddClass( dialog_footer, "dialog-footer" );
	Widget_AddClass( ctx.input, "dialog-input full-width" );
	Widget_AddClass( ctx.input2, "dialog-input full-width" );
	/* title */
	dialog_text = LCUIWidget_New( "textview" );
	TextView_SetTextW( dialog_text, title );
	Widget_Append( dialog_header, dialog_text );
	/* text */
	dialog_text = LCUIWidget_New( "textview" );
	Widget_AddClass( dialog_text, "text-block" );
	TextView_SetTextW( dialog_text, text );
	Widget_Append( dialog_body, dialog_text );
	/* password input */
	box = LCUIWidget_New( NULL );
	ctx.msg = LCUIWidget_New( "textview" );
	ctx.msg2 = LCUIWidget_New( "textview" );
	TextEdit_SetPasswordChar( ctx.input, L'●' );
	TextEdit_SetPasswordChar( ctx.input2, L'●' );
	TextEdit_SetPlaceHolderW( ctx.input, I18n_GetText( KEY_TXT_PWD ) );
	TextEdit_SetPlaceHolderW( ctx.input2, I18n_GetText( KEY_TXT_PWD2 ) );
	Widget_AddClass( ctx.msg, "error message" );
	Widget_AddClass( ctx.msg2, "error message" );
	Widget_AddClass( box, "input-group" );
	Widget_Append( box, ctx.input );
	Widget_Append( box, ctx.msg );
	Widget_Append( dialog_body, box );
	box = LCUIWidget_New( NULL );
	Widget_AddClass( box, "input-group" );
	Widget_Append( box, ctx.input2 );
	Widget_Append( box, ctx.msg2 );
	Widget_Append( dialog_body, box );
	/* footer buttons */
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
	/* build dialog */
	Widget_Append( dialog_content, dialog_header );
	Widget_Append( dialog_content, dialog_body );
	Widget_Append( dialog_content, dialog_footer );
	Widget_Append( dialog, dialog_content );
	Widget_Append( parent, dialog );
	/* bind events */
	Widget_SetDisabled( btn_ok, TRUE );
	Widget_BindEvent( btn_ok, "click", OnBtnOkClick, &ctx, NULL );
	Widget_BindEvent( btn_cancel, "click", OnBtnCancelClick, &ctx, NULL );
	Widget_BindEvent( ctx.input, "change", OnInputChange, &ctx, NULL );
	Widget_BindEvent( ctx.input2, "change", OnInputChange, &ctx, NULL );
	LCUIWidget_SetFocus( ctx.input );
	LCUIMainLoop_Run( ctx.loop );
	TextEdit_GetTextW( ctx.input, 0, PWD_MAX_LEN, password );
	Widget_Destroy( dialog );
	return ctx.result;
}
