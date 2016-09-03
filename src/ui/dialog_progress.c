/* ***************************************************************************
 * dialog_progress.c -- progress dialog, used for pop-up dialog to show
 * current operation progress.
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
 * dialog_progress.c -- “进度”对话框，用于弹出提示框提示当前的操作进度。
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
#include "ui.h"
#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include "dialog.h"

LCUI_ProgressDialog NewProgressDialog( void )
{
	LCUI_ProgressDialog dialog;
	LCUI_Widget box = LCUIWidget_New( NULL );
	LCUI_Widget dialog_body = LCUIWidget_New( NULL );
	LCUI_Widget dialog_content = LCUIWidget_New( NULL );
	LCUI_Widget dialog_header = LCUIWidget_New( NULL );
	LCUI_Widget dialog_footer = LCUIWidget_New( NULL );
	dialog = NEW(LCUI_ProgressDialogRec, 1);
	dialog->title = LCUIWidget_New( "textview" );
	dialog->content = LCUIWidget_New( "textview" );
	dialog->progress = LCUIWidget_New( "progress" );
	dialog->btn_cancel = LCUIWidget_New( "button" );
	dialog->container = LCUIWidget_New( NULL );
	dialog->loop = LCUI_MainLoop_New();
	Widget_AddClass( box, "dialog-btn-group one-button" );
	Widget_AddClass( dialog->container, "dialog" );
	Widget_AddClass( dialog_body, "dialog-body" );
	Widget_AddClass( dialog_content, "dialog-content" );
	Widget_AddClass( dialog_header, "dialog-header" );
	Widget_AddClass( dialog_footer, "dialog-footer" );
	Widget_AddClass( dialog->btn_cancel, "dialog-btn" );
	Widget_Append( box, dialog->btn_cancel );
	Widget_Append( dialog_header, dialog->title );
	Widget_Append( dialog_body, dialog->content );
	Widget_Append( dialog_body, dialog->progress );
	Widget_Append( dialog_footer, box );
	Widget_Append( dialog_content, dialog_header );
	Widget_Append( dialog_content, dialog_body );
	Widget_Append( dialog_content, dialog_footer );
	Widget_Append( dialog->container, dialog_content );
	return dialog;
}

void OpenProgressDialog( LCUI_ProgressDialog dialog, LCUI_Widget parent )
{
	Widget_Append( parent, dialog->container );
	LCUI_MainLoop_Run( dialog->loop );
	Widget_Destroy( dialog->container );
	free( dialog );
}

void CloseProgressDialog( LCUI_ProgressDialog dialog )
{
	LCUI_MainLoop_Quit( dialog->loop );
}
