#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "finder.h"
#include "ui.h"

#define TEXT_SYNCING	L"已扫描 %d 个文件"
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
	int count = self.status.count;
	if( self.status.task ) {
		count += self.status.task->count;
	}
	wsprintf( wstr, TEXT_SYNCING, count );
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
	TextView_SetTextW( self.title, TEXT_FINISHED );
	LCUITimer_Set( 3000, OnHideTip, NULL, FALSE );
	self.is_syncing = FALSE;
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
	self.is_syncing = FALSE;
	self.status.count = 0;
	self.status.task = NULL;
	self.status.state = STATE_NONE;
	self.text = LCUIWidget_GetById( "folders-view-sync-stats" );
	self.title = LCUIWidget_GetById( "folders-view-sync-title" );
	LCFinder_BindEvent( "sync", OnStartSyncFiles, NULL );
}
