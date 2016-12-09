/* ***************************************************************************
 * dialog.h -- dialog
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
 * dialog.h -- 对话框
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

#ifndef LCFINDER_DIALOG_H
#define LCFINDER_DIALOG_H

LCUI_BEGIN_HEADER

typedef struct LCUI_ProgressDialogRec_ {
	LCUI_Widget title;
	LCUI_Widget content;
	LCUI_Widget progress;
	LCUI_Widget btn_cancel;
	LCUI_Widget container;
	LCUI_MainLoop loop;
} LCUI_ProgressDialogRec, *LCUI_ProgressDialog;

/** 
 * 显示“确认”对话框
 * @param[in] parent 用于容纳对话框的父部件
 * @param[in] title 对话框标题
 * @param[in] text 对话框内容
 * @returns 在用户点击“确认”按钮后返回 TRUE，点击“取消”按钮则返回 FALSE
 */
LCUI_API LCUI_BOOL LCUIDialog_Confirm( LCUI_Widget parent, 
				       const wchar_t* title,
				       const wchar_t *text );

/**
 * 显示“文本输入”对话框
 * @param[in] parent 用于容纳对话框的父部件
 * @param[in] title 对话框标题
 * @param[in] placeholder 文本编辑框的占位符
 * @param[in] val 文本编辑框内预置的文本
 * @param[out] newval 文本缓存，用于存放编辑后的文本
 * @param[in] max_len 文本缓存的最大长度
 * @param[in] checker 验证函数，用于验证输入的文本是否符合要求，如果符合则返回
 * TRUE，否则返回 FALSE，置为 NULL 时将不验证文本。
 * @returns 点击“确认”按钮时返回 TRUE，点击“取消”按钮时返回 FALSE。
 */
LCUI_API int LCUIDialog_Prompt( LCUI_Widget parent, const wchar_t* title,
				const wchar_t *placeholder, const wchar_t *val,
				wchar_t *newval, size_t max_len,
				LCUI_BOOL( *checker )(const wchar_t*) );

/** 
 * 显示密码验证对话框
 * @param[in] parent 用于容纳对话框的父部件
 * @param[in] title 对话框标题
 * @param[in] text 对话框内的说明文本
 * @param[in] check 回调函数，验证密码
 * @param[in] data 传递给回调函数的附加数据
 */
LCUI_API int LCUIDialog_CheckPassword( LCUI_Widget parent, const wchar_t *title,
				       const wchar_t *text,
				       LCUI_BOOL( *check )(const char*, const char*),
				       const char *data );

/** 新建一个“进度”对话框，返回值为该对话框的数据 */
LCUI_API LCUI_ProgressDialog NewProgressDialog( void );

/**
 * 打开并显示“进度”对话框
 * @param[in] dialog 对话框数据
 * @param[in] parent 父级部件，指定该对话框将放在哪个容器中显示
 * @note 在该函数执行完后，会自动销毁对话框数据
 */
LCUI_API void OpenProgressDialog( LCUI_ProgressDialog dialog, LCUI_Widget parent );

/** 关闭”进度“对话框 */
LCUI_API void CloseProgressDialog( LCUI_ProgressDialog dialog );

LCUI_END_HEADER

#endif
