/* ***************************************************************************
* file_sync.c -- file sync and sync status view function.
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
* file_sync.c -- 文件同步和状态提示功能。
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
#include <LCUI/LCUI.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "finder.h"
#include "ui.h"

#define TEXT_SCANING	L"已扫描 %d 个文件"
#define TEXT_SAVING	L"正同步 %d/%d 个文件"
#define TEXT_FINISHED	L"资源同步完成！"

static struct SyncContextRec_ {
	LCUI_BOOL is_syncing;
	FileSyncStatusRec status;
	LCUI_Widget text;
	LCUI_Widget title;
	LCUI_Thread thread;
	int timer;
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
	default:return;
	}
	TextView_SetTextW( self.text, wstr );
}

static void OnHideTip( void *arg )
{
	LCUI_Widget alert = self.text->parent;
	Widget_AddClass( alert, "hide" );
}

static void FileSyncThread( void *arg )
{
	LCUI_Widget alert = self.text->parent;
	Widget_RemoveClass( alert, "hide" );
	LCFinder_SyncFiles( &self.status );
	OnUpdateStats( NULL );
	LCUITimer_Free( self.timer );
	TextView_SetTextW( self.title, TEXT_FINISHED );
	LCUITimer_Set( 3000, OnHideTip, NULL, FALSE );
	self.is_syncing = FALSE;
	self.timer = 0;
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
	self.text = LCUIWidget_GetById( "folders-view-sync-stats" );
	self.title = LCUIWidget_GetById( "folders-view-sync-title" );
	LCFinder_BindEvent( "sync", OnStartSyncFiles, NULL );
}
