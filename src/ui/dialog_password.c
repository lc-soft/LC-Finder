/* ***************************************************************************
 * dialog_password.c -- password dialog, used for pop-up dialog to check
 * password or input new password.
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
 * dialog_confirm.c --  密码对话框，用于弹出提示框确认密码或设置新密码
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

#include <LCUI_Build.h>
#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/widget/textedit.h>
#include "i18n.h"
#include "textview_i18n.h"
#include "dialog.h"

#define KEY_OK		"button.ok"
#define KEY_CANCEL	"button.cancel"

typedef struct DialogContextRec_ {
	int result;
	const char *data;
	LCUI_BOOL (*check)(const char*, const char*);
	LCUI_Widget input;
	LCUI_Widget btn;
	LCUI_MainLoop loop;
} DialogContextRec, *DialogContext;

static void OnBtnOkClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	size_t len;
	char *text;
	wchar_t wtext[256] = { 0 };
	DialogContext ctx = e->data;
	if( w->disabled ) {
		return;
	}
	len = TextEdit_GetTextW( ctx->input, 0, 255, wtext );
	if( len == 0 ) {
		return;
	}
	text = EncodeUTF8( wtext );
	if( !ctx->check( text, ctx->data ) ) {
		Widget_AddClass( ctx->input, "error" );
		return;
	}
	Widget_RemoveClass( ctx->input, "error" );
	ctx->result = 0;
	LCUI_MainLoop_Quit( ctx->loop );
	free( text );
}

static void OnBtnCancelClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	DialogContext ctx = e->data;
	ctx->result = -1;
	LCUI_MainLoop_Quit( ctx->loop );
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
	LCUI_Widget btn_cancel = LCUIWidget_New( NULL );
	LCUI_Widget btn_ok = LCUIWidget_New( NULL );
	ctx.btn = btn_ok;
	ctx.check = check;
	ctx.loop = LCUI_MainLoop_New();
	ctx.input = LCUIWidget_New( "textedit" );
	Widget_AddClass( dialog, "dialog" );
	Widget_AddClass( dialog_body, "dialog-body" );
	Widget_AddClass( dialog_content, "dialog-content" );
	Widget_AddClass( dialog_header, "dialog-header" );
	Widget_AddClass( dialog_footer, "dialog-footer" );
	Widget_AddClass( ctx.input, "dialog-input full-width" );
	dialog_text = LCUIWidget_New( "textview" );
	TextView_SetTextW( dialog_text, title );
	Widget_Append( dialog_header, dialog_text );
	dialog_text = LCUIWidget_New( "textview" );
	Widget_AddClass( dialog_text, "text-block" );
	TextView_SetTextW( dialog_text, text );
	Widget_Append( dialog_body, dialog_text );
	Widget_Append( dialog_body, ctx.input );
	box = LCUIWidget_New( NULL );
	dialog_text = LCUIWidget_New( "textview" );
	Widget_AddClass( box, "dialog-btn-group" );
	Widget_AddClass( btn_ok, "dialog-btn" );
	TextView_SetTextW( dialog_text, I18n_GetText( KEY_OK ) );
	Widget_Append( btn_ok, dialog_text );
	Widget_Append( box, btn_ok );
	Widget_Append( dialog_footer, box );
	box = LCUIWidget_New( NULL );
	dialog_text = LCUIWidget_New( "textview" );
	Widget_AddClass( box, "dialog-btn-group" );
	Widget_AddClass( btn_cancel, "dialog-btn" );
	TextView_SetTextW( dialog_text, I18n_GetText( KEY_CANCEL ) );
	Widget_Append( btn_cancel, dialog_text );
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
	LCUIWidget_SetFocus( ctx.input );
	LCUI_MainLoop_Run( ctx.loop );
	Widget_Destroy( dialog );
	return ctx.result;
}
