/* ***************************************************************************
 * LCUIRenderer.cpp -- UWP input support for LCUI
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
 * LCUIRenderer.cpp -- LCUI 的 UWP 版输入支持，包括鼠标、键盘、触屏等的输入处理
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

#include "pch.h"
#include "LCUIInput.h"
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/cursor.h>

using namespace UWP;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI;
using namespace Windows::Foundation;

LCUIInput::LCUIInput()
{
	m_actived = false;
}

void LCUIInput::OnPointerPressed( CoreWindow^ sender, PointerEventArgs^ args )
{

}

void LCUIInput::OnPointerMoved( CoreWindow^ sender, PointerEventArgs^ args )
{
	Point position;
	LCUI_Pos pos;
	LCUI_SysEventRec sys_ev;

	position = args->CurrentPoint->Position;
	if( !m_actived ) {
		m_actived = true;
		m_position = position;
	}
	sys_ev.type = LCUI_MOUSEMOVE;
	sys_ev.motion.x = (int)(position.X);
	sys_ev.motion.y = (int)(position.Y);
	sys_ev.motion.xrel = (int)(position.X - m_position.Y);
	sys_ev.motion.yrel = (int)(position.Y - m_position.Y);
	m_position = position;
	pos.x = sys_ev.motion.x;
	pos.y = sys_ev.motion.y;
	LCUI_TriggerEvent( &sys_ev, NULL );
	LCUICursor_SetPos( pos );
}

void LCUIInput::OnPointerReleased( CoreWindow^ sender, PointerEventArgs^ args )
{

}

void LCUIInput::OnPointerExited( CoreWindow^ sender, PointerEventArgs^ args )
{

}

void LCUIInput::OnKeyDown( CoreWindow^ sender, KeyEventArgs^ args )
{

}

void LCUIInput::OnKeyUp( CoreWindow^ sender, KeyEventArgs^ args )
{

}
