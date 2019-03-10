/* ***************************************************************************
 * view_home.c -- home view
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
 * view_home.c -- 主页"集锦"视图
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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/widget/button.h>
#include "file_storage.h"
#include "file_stage.h"
#include "thumbview.h"
#include "progressbar.h"
#include "timeseparator.h"
#include "textview_i18n.h"
#include "i18n_datetime.h"
#include "browser.h"

#define KEY_TITLE "home.title"

/** 主页集锦视图的相关数据 */
static struct HomeCollectionView {
	LCUI_BOOL is_activated;
	LCUI_BOOL show_private_files;

	LCUI_Widget view;
	LCUI_Widget items;
	LCUI_Widget time_ranges;
	LCUI_Widget selected_time;
	LCUI_Widget info_path;
	LCUI_Widget tip_empty;
	LCUI_Widget progressbar;

	/** 文件列表，供视图渲染 */
	LinkedList files;
	/** 文件暂存区域，用于存放已扫描到的文件 */
	FileStage stage;
	/** 文件扫描器线程 */
	LCUI_Thread scanner_thread;
	/**< 文件扫描器是否在运行 */
	LCUI_BOOL scanner_running;
	/**< 定时器，用于定时处理暂存区域内的文件，将他们渲染到视图中 */
	int scanner_timer;

	/**< 时间分割器列表 */
	LinkedList separators;
	/**< 文件浏览器数据 */
	FileBrowserRec browser;
} view;

static void OnDeleteDBFile(void *arg)
{
	DBFile_Release(arg);
}

static void OnBtnSyncClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LCFinder_TriggerEvent(EVENT_SYNC, NULL);
}

static void DeleteTimeSeparator(LCUI_Widget sep)
{
	LinkedListNode *node;
	for (LinkedList_Each(node, &view.separators)) {
		if (node->data == sep) {
			LinkedList_Unlink(&view.separators, node);
			LinkedListNode_Delete(node);
			break;
		}
	}
	Widget_Destroy(sep);
}

static void OnAfterDeleted(LCUI_Widget first)
{
	int count;
	LCUI_Widget sep = NULL, w = first;
	while (w) {
		if (w->type && strcmp(w->type, "time-separator") == 0) {
			sep = w;
			break;
		}
		w = Widget_GetPrev(w);
	}
	if (!sep) {
		sep = LinkedList_Get(&view.separators, 0);
	}
	if (!sep) {
		Widget_RemoveClass(view.tip_empty, "hide");
		Widget_Show(view.tip_empty);
		return;
	}
	w = Widget_GetNext(sep);
	TimeSeparator_Reset(sep);
	for (count = 0; w; w = Widget_GetNext(w)) {
		if (!w->type) {
			continue;
		}
		if (strcmp(w->type, "thumbviewitem") == 0) {
			time_t time;
			struct tm *t;
			DB_File file;
			file = ThumbViewItem_GetFile(w);
			time = file->create_time;
			t = localtime(&time);
			TimeSeparator_AddTime(sep, t);
			count += 1;
		} else if (strcmp(w->type, "time-separator") == 0) {
			/* 如果该时间分割器下一个文件都没有 */
			if (count == 0) {
				DeleteTimeSeparator(sep);
			}
			count = 0, sep = w;
			TimeSeparator_Reset(sep);
		}
	}
	/* 处理最后一个没有文件的时间分割器 */
	if (sep && count == 0) {
		DeleteTimeSeparator(sep);
	}
	/* 如果时间分割器数量为0，则说明当前缩略图列表为空 */
	if (view.separators.length < 1) {
		Widget_RemoveClass(view.tip_empty, "hide");
		Widget_Show(view.tip_empty);
	}
}

static void OnTimeRangeClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LCUI_Widget title = e->data;
	FileBrowser_SetScroll(&view.browser, (int)title->box.canvas.y);
	FileBrowser_SetButtonsDisabled(&view.browser, FALSE);
	Widget_Hide(view.time_ranges->parent->parent);
}

static void OnTimeTitleClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	if (view.selected_time) {
		Widget_RemoveClass(view.selected_time, "selected");
	}
	view.selected_time = e->data;
	Widget_AddClass(e->data, "selected");
	FileBrowser_SetButtonsDisabled(&view.browser, TRUE);
	Widget_Show(view.time_ranges->parent->parent);
}

static void RenderTime(wchar_t *buf, const wchar_t *text, void *privdata)
{
	struct tm *t = privdata;
	if (!text) {
		wcscpy(buf, L"<translation missing>");
		return;
	}
	FormatYearString(buf, TXTFMT_BUF_MAX_LEN - 1, t);
}

static LCUI_Widget LCUIWidget_NewTimeRange(const struct tm *t)
{
	LCUI_Widget range;
	struct tm *data;

	range = LCUIWidget_New("textview-i18n");
	data = malloc(sizeof(struct tm));
	memcpy(data, t, sizeof(struct tm));
	Widget_SetAttributeEx(range, "time", data, 0, free);
	TextViewI18n_SetFormater(range, RenderTime, data);
	TextViewI18n_SetKey(range, KEY_YEAR_FORMAT);
	return range;
}

/** 向视图追加文件 */
static void HomeView_AppendFile(DB_File file)
{
	time_t time;
	struct tm *t;
	LCUI_Widget sep, range;

	time = file->modify_time;
	t = localtime(&time);
	sep = LinkedList_Get(&view.separators, view.separators.length - 1);
	/* 如果当前文件的创建时间超出当前时间段，则新建分割线 */
	if (!sep || !TimeSeparator_CheckTime(sep, t)) {
		sep = LCUIWidget_New("time-separator");
		range = LCUIWidget_NewTimeRange(t);
		TimeSeparator_SetTime(sep, t);
		FileBrowser_Append(&view.browser, sep);
		LinkedList_Append(&view.separators, sep);
		Widget_AddClass(range, "time-range link");
		Widget_Append(view.time_ranges, range);
		Widget_BindEvent(TimeSeparator_GetTitle(sep), "click",
				 OnTimeTitleClick, range, NULL);
		Widget_BindEvent(range, "click", OnTimeRangeClick, sep, NULL);
	}
	TimeSeparator_AddTime(sep, t);
	FileBrowser_AppendPicture(&view.browser, file);
}

static void HomeView_AppendFiles(void *unused)
{
	LinkedList files;
	LinkedListNode *node;

	LinkedList_Init(&files);
	FileStage_GetFiles(view.stage, &files, 512);
	for (LinkedList_Each(node, &files)) {
		HomeView_AppendFile(node->data);
	}
	LinkedList_Concat(&view.files, &files);
	view.scanner_timer = LCUI_SetTimeout(200, HomeView_AppendFiles, NULL);
}

static size_t HomeView_ScanFiles(void)
{
	DB_File file;
	DB_Query query;
	size_t i, total, count;
	DB_QueryTermsRec terms = { 0 };

	terms.limit = 512;
	terms.modify_time = DESC;
	terms.n_dirs = LCFinder_GetSourceDirList(&terms.dirs);
	if (terms.n_dirs == finder.n_dirs) {
		free(terms.dirs);
		terms.dirs = NULL;
		terms.n_dirs = 0;
	}
	query = DB_NewQuery(&terms);
	total = DBQuery_GetTotalFiles(query);
	DB_DeleteQuery(query);

	for (count = 0; view.scanner_running && count < total;) {
		query = DB_NewQuery(&terms);
		for (i = 0; view.scanner_running; ++count, ++i) {
			file = DBQuery_FetchFile(query);
			if (!file) {
				break;
			}
			FileStage_AddFile(view.stage, file);
		}
		terms.offset += terms.limit;
		FileStage_Commit(view.stage);
		DB_DeleteQuery(query);
		if (i < terms.limit) {
			break;
		}
	}
	if (terms.dirs) {
		free(terms.dirs);
		terms.dirs = NULL;
	}
	FileStage_Commit(view.stage);
	return total;
}

static void HomeView_ScannerThread(void *unused)
{
	size_t count = HomeView_ScanFiles();

	if (view.is_activated) {
		if (count > 0) {
		Widget_AddClass(view.tip_empty, "hide");
		Widget_Hide(view.tip_empty);
	} else {
		Widget_RemoveClass(view.tip_empty, "hide");
		Widget_Show(view.tip_empty);
	}
	}
	view.scanner_running = FALSE;
	LCUIThread_Exit(NULL);
}

static void HomeView_InitScanner(void)
{
	view.stage = FileStage_Create();
	view.scanner_running = FALSE;
	view.scanner_timer = 0;
	LinkedList_Init(&view.files);
}

static void HomeView_StopScanner(void)
{
	if (view.scanner_running) {
		view.scanner_running = FALSE;
		LCUIThread_Join(view.scanner_thread, NULL);
	}
	if (view.scanner_timer) {
		LCUITimer_Free(view.scanner_timer);
		view.scanner_timer = 0;
	}
	FileStage_GetFiles(view.stage, &view.files, 0);
	LinkedList_Clear(&view.files, OnDeleteDBFile);
}

static void HomeView_StartScanner(void)
{
	HomeView_StopScanner();
	view.scanner_running = TRUE;
	view.scanner_timer = LCUI_SetTimeout(200, HomeView_AppendFiles, NULL);
	LCUIThread_Create(&view.scanner_thread, HomeView_ScannerThread, NULL);
	ProgressBar_SetValue(view.progressbar, 0);
	ProgressBar_SetMaxValue(view.progressbar, 100);
	Widget_Show(view.progressbar);
}

static void HomeView_FreeScanner(void)
{
	HomeView_StopScanner();
	FileStage_Destroy(view.stage);
}

/** 载入集锦中的文件列表 */
static void HomeView_LoadFiles(void)
{
	LinkedList_Clear(&view.separators, NULL);
	Widget_Empty(view.time_ranges);
	FileBrowser_Empty(&view.browser);
	HomeView_StartScanner();
}

static void HomeView_OnShow(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	if (view.show_private_files == finder.open_private_space) {
		return;
	}
	view.show_private_files = finder.open_private_space;
	HomeView_LoadFiles();
}

static void OnSyncDone(void *privdata, void *arg)
{
	LCUI_PostSimpleTask(HomeView_LoadFiles, NULL, NULL);
}

static void HomeView_InitBase(void)
{
	SelectWidget(view.view, ID_VIEW_HOME);
	SelectWidget(view.items, ID_VIEW_HOME_COLLECTIONS);
	SelectWidget(view.time_ranges, ID_VIEW_TIME_RANGE_LIST);
	SelectWidget(view.progressbar, ID_VIEW_HOME_PROGRESS);
	SelectWidget(view.tip_empty, ID_TIP_HOME_EMPTY);
	BindEvent(view.view, "show.view", HomeView_OnShow);
	Widget_Hide(view.time_ranges->parent->parent);
	Widget_AddClass(view.time_ranges, "time-range-list");
	LCFinder_BindEvent(EVENT_SYNC_DONE, OnSyncDone, NULL);
}

static void HomeView_OnProgress(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	size_t *current = arg;

	ProgressBar_SetValue(view.progressbar, *current);
	ProgressBar_SetMaxValue(view.progressbar, w->children.length);
	if (*current == w->children.length) {
		Widget_Hide(view.progressbar);
	} else {
		Widget_Show(view.progressbar);
	}
}

static void HomeView_InitBrowser(void)
{
	LCUI_Widget btn[5], title;

	SelectWidget(title, ID_TXT_HOME_SELECTION_STATS);
	SelectWidget(btn[0], ID_BTN_SYNC_HOME_FILES);
	SelectWidget(btn[1], ID_BTN_SELECT_HOME_FILES);
	SelectWidget(btn[2], ID_BTN_CANCEL_HOME_SELECT);
	SelectWidget(btn[3], ID_BTN_TAG_HOME_FILES);
	SelectWidget(btn[4], ID_BTN_DELETE_HOME_FILES);
	BindEvent(btn[0], "click", OnBtnSyncClick);

	view.browser.btn_select = btn[1];
	view.browser.btn_cancel = btn[2];
	view.browser.btn_delete = btn[4];
	view.browser.btn_tag = btn[3];
	view.browser.view = view.view;
	view.browser.items = view.items;
	view.browser.txt_selection_stats = title;
	view.browser.after_deleted = OnAfterDeleted;
	ThumbView_SetCache(view.items, finder.thumb_cache);
	ThumbView_SetStorage(view.items, finder.storage_for_thumb);
	Widget_BindEvent(view.items, "progress", HomeView_OnProgress, NULL, NULL);
	FileBrowser_Init(&view.browser);
}

void UI_InitHomeView(void)
{
	HomeView_InitBase();
	HomeView_InitBrowser();
	HomeView_InitScanner();
	HomeView_LoadFiles();
	view.is_activated = TRUE;
}

void UI_FreeHomeView(void)
{
	if (!view.is_activated) {
		return;
	}
	view.is_activated = FALSE;
	HomeView_FreeScanner();
}
