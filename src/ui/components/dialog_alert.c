/* ***************************************************************************
 * dialog_confirm.c -- confirm dialog, used for pop-up dialog to confirm 
 * whether or not to continue operation.
 *
 * Copyright (C) 2017 by Liu Chao <lc-soft@live.cn>
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
 * dialog_confirm.c -- “警告”对话框
 *
 * 版权所有 (C) 2017 归属于 刘超 <lc-soft@live.cn>
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
#include "dialog.h"
#include "i18n.h"

#define KEY_OK		"button.ok"

typedef struct DialogContextRec_ {
	LCUI_BOOL result;
	LCUI_MainLoop loop;
} DialogContextRec, *DialogContext;

static void OnBtnOkClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	DialogContext ctx = e->data;
	ctx->result = TRUE;
	LCUIMainLoop_Quit( ctx->loop );
}

void LCUIDialog_Alert( LCUI_Widget parent, const wchar_t* title,
		       const wchar_t *text )
{
	DialogContextRec ctx = { 0 };
	LCUI_Widget dialog_text, box;
	LCUI_Widget dialog = LCUIWidget_New( NULL );
	LCUI_Widget dialog_body = LCUIWidget_New( NULL );
	LCUI_Widget dialog_content = LCUIWidget_New( NULL );
	LCUI_Widget dialog_header = LCUIWidget_New( NULL );
	LCUI_Widget dialog_footer = LCUIWidget_New( NULL );
	LCUI_Widget btn_ok = LCUIWidget_New( "button" );
	ctx.loop = LCUIMainLoop_New();
	Widget_AddClass( dialog, "dialog" );
	Widget_AddClass( dialog_body, "dialog-body" );
	Widget_AddClass( dialog_content, "dialog-content" );
	Widget_AddClass( dialog_header, "dialog-header" );
	Widget_AddClass( dialog_footer, "dialog-footer" );
	dialog_text = LCUIWidget_New( "textview" );
	TextView_SetTextW( dialog_text, title );
	Widget_Append( dialog_header, dialog_text );
	dialog_text = LCUIWidget_New( "textview" );
	TextView_SetTextW( dialog_text, text );
	Widget_Append( dialog_body, dialog_text );
	box = LCUIWidget_New( NULL );
	Widget_AddClass( box, "dialog-btn-group" );
	Widget_Append( dialog_footer, box );
	box = LCUIWidget_New( NULL );
	Widget_AddClass( box, "dialog-btn-group" );
	Widget_AddClass( btn_ok, "dialog-btn" );
	Button_SetTextW( btn_ok, I18n_GetText( KEY_OK ) );
	Widget_Append( box, btn_ok );
	Widget_Append( dialog_footer, box );
	Widget_Append( dialog_content, dialog_header );
	Widget_Append( dialog_content, dialog_body );
	Widget_Append( dialog_content, dialog_footer );
	Widget_Append( dialog, dialog_content );
	Widget_Append( parent, dialog );
	Widget_BindEvent( btn_ok, "click", OnBtnOkClick, &ctx, NULL );
	LCUIMainLoop_Run( ctx.loop );
	Widget_Destroy( dialog );
}
