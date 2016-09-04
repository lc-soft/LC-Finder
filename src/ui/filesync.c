/* ***************************************************************************
 * filesync.c -- file sync and sync status view function.
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
 * filesync.c -- 文件同步和状态提示功能。
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
#include "ui.h"

#define TEXT_STARED	L"正在同步你的资源"
#define TEXT_SCANING	L"已扫描 %d 个文件"
#define TEXT_SAVING	L"正同步 %d/%d 个文件"
#define TEXT_SAVED	L"已同步 %d 个文件"
#define TEXT_FINISHED	L"资源同步完成！"

/** 当前文件同步功能所需的数据 */
static struct SyncContextRec_ {
	LCUI_BOOL is_syncing;		/**< 是否正在同步 */
	FileSyncStatusRec status;	/**< 当前状态数据 */
	LCUI_Widget text;		/**< 提示框中显示的内容 */
	LCUI_Widget title;		/**< 提示框中显示的标题 */
	LCUI_Thread thread;		/**< 用于进行文件同步的线程 */
	int timer;			/**< 用于动态更新提示框内容的定时器 */
} self;

static void OnUpdateStats( void *arg )
{
	wchar_t wstr[256];
	int count, total;
	switch( self.status.state ) {
	case STATE_STARTED:
		count = self.status.scaned_files;
		if( self.status.task ) {
			count += self.status.task->total_files;
		}
		wsprintf( wstr, TEXT_SCANING, count );
		break;
	case STATE_SAVING:
		count = self.status.synced_files;
		total = self.status.added_files + self.status.deleted_files;
		wsprintf( wstr, TEXT_SAVING, count, total );
		break;
	case STATE_FINISHED:
		count = self.status.synced_files;
		wsprintf( wstr, TEXT_SAVED, count );
		break;
	default:return;
	}
	TextView_SetTextW( self.text, wstr );
}

static void OnHideTip( void *arg )
{
	LCUI_Widget alert = self.text->parent;
	Widget_AddClass( alert, "hide" );
}

/** 文件同步线程 */
static void FileSyncThread( void *arg )
{
	LCUI_Widget alert = self.text->parent;
	TextView_SetTextW( self.title, TEXT_STARED );
	Widget_RemoveClass( alert, "hide" );
	LCFinder_SyncFiles( &self.status );
	OnUpdateStats( NULL );
	LCUITimer_Free( self.timer );
	TextView_SetTextW( self.title, TEXT_FINISHED );
	LCUITimer_Set( 3000, OnHideTip, NULL, FALSE );
	self.is_syncing = FALSE;
	self.timer = 0;
	LCFinder_TriggerEvent( EVENT_SYNC_DONE, NULL );
}

static void OnStartSyncFiles( void *privdata, void *data )
{
	if( self.is_syncing ) {
		return;
	}
	self.is_syncing = TRUE;
	self.timer = LCUITimer_Set( 200, OnUpdateStats, NULL, TRUE );
	LCUIThread_Create( &self.thread, FileSyncThread, NULL );
}

void UI_InitFileSyncTip( void )
{
	memset( &self, 0, sizeof( self ) );
	self.text = LCUIWidget_GetById( ID_TXT_FILE_SYNC_STATS );
	self.title = LCUIWidget_GetById( ID_TXT_FILE_SYNC_TITLE );
	LCFinder_BindEvent( EVENT_SYNC, OnStartSyncFiles, NULL );
}
