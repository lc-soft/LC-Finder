/* ***************************************************************************
 * view_home.c -- folders view
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
 * view_home.c -- "文件夹" 视图
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "finder.h"
#include "file_storage.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "thumbview.h"
#include "textview_i18n.h"
#include "browser.h"
#include "i18n.h"

#define KEY_SORT_HEADER "sort.header"
#define KEY_TITLE "folders.title"
#define THUMB_CACHE_SIZE (20 * 1024 * 1024)

/** 视图同步功能的相关数据 */
typedef struct ViewSyncRec_ {
	LCUI_Thread tid;
	LCUI_BOOL is_running;
	LCUI_Mutex mutex;
	LCUI_Cond ready;
	int prev_item_type;
} ViewSyncRec, *ViewSync;

/** 文件扫描功能的相关数据 */
typedef struct FileScannerRec_ {
	size_t files_count;
	LCUI_Thread tid;
	LCUI_Cond cond;
	LCUI_Cond cond_scan;
	LCUI_Mutex mutex;
	LCUI_Mutex mutex_scan;
	LCUI_BOOL is_running;
	LCUI_BOOL is_async_scaning;
	LinkedList files;
	wchar_t *dirpath;
} FileScannerRec, *FileScanner;

typedef struct FileEntryRec_ {
	LCUI_BOOL is_dir;
	DB_File file;
	char *path;
} FileEntryRec, *FileEntry;

static struct FoldersViewData {
	DB_Dir dir;
	LCUI_BOOL is_activated;
	LCUI_Widget view;
	LCUI_Widget items;
	LCUI_Widget info;
	LCUI_Widget info_name;
	LCUI_Widget info_path;
	LCUI_Widget tip_empty;
	LCUI_Widget selected_sort;
	LCUI_BOOL show_private_folders;
	LCUI_BOOL folders_changed;
	ViewSyncRec viewsync;
	FileScannerRec scanner;
	FileBrowserRec browser;
	DB_QueryTermsRec terms;
	char *dirpath;
} view = { 0 };

#define SORT_METHODS_LEN 6

static FileSortMethodRec sort_methods[SORT_METHODS_LEN] = {
	{ "sort.ctime_desc", CREATE_TIME_DESC },
	{ "sort.ctime_asc", CREATE_TIME_ASC },
	{ "sort.mtime_desc", MODIFY_TIME_DESC },
	{ "sort.mtime_asc", MODIFY_TIME_ASC },
	{ "sort.score_desc", SCORE_DESC },
	{ "sort.score_asc", SCORE_ASC }
};

static void OpenFolder(const char *dirpath);

static void OnAddDir(void *privdata, void *data)
{
	DB_Dir dir = data;
	_DEBUG_MSG("add dir: %s\n", dir->path);
	if (!view.dir) {
		view.folders_changed = TRUE;
	}
}

static void OnFolderChange(void *privdata, void *data)
{
	view.folders_changed = TRUE;
}

static void OnViewShow(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	if (view.show_private_folders == finder.open_private_space &&
	    !view.folders_changed) {
		return;
	}
	view.show_private_folders = finder.open_private_space;
	view.folders_changed = FALSE;
	OpenFolder(NULL);
}

static void OnBtnSyncClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LCFinder_TriggerEvent(EVENT_SYNC, NULL);
}

static void OnBtnReturnClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	OpenFolder(NULL);
}

static void OnDeleteFileEntry(void *arg)
{
	FileEntry entry = arg;
	if (entry->is_dir) {
		free(entry->path);
	} else {
		free(entry->file->path);
		free(entry->file);
	}
	free(entry);
}

static void FileScanner_OnGetDirs(FileStatus *status, FileStream stream,
				  void *data)
{
	char *dirpath;
	size_t len, dirpath_len;
	FileScanner scanner = data;
	FileEntry file_entry;

	LCUIMutex_Lock(&scanner->mutex_scan);
	if (!status || !stream) {
		scanner->is_async_scaning = FALSE;
		LCUICond_Signal(&scanner->cond_scan);
		LCUIMutex_Unlock(&scanner->mutex_scan);
		return;
	}
	dirpath = EncodeUTF8(scanner->dirpath);
	dirpath_len = strlen(dirpath);
	while (1) {
		char *p, buf[PATH_LEN];
		p = FileStream_ReadLine(stream, buf, PATH_LEN);
		if (!p) {
			break;
		}
		if (buf[0] != 'd') {
			continue;
		}
		len = strlen(buf);
		if (len < 2) {
			continue;
		}
		buf[len - 1] = 0;
		file_entry = NEW(FileEntryRec, 1);
		file_entry->is_dir = TRUE;
		file_entry->file = NULL;
		len = len + dirpath_len;
		file_entry->path = malloc(sizeof(char) * len);
		pathjoin(file_entry->path, dirpath, buf + 1);
		LCUIMutex_Lock(&scanner->mutex);
		LinkedList_Append(&scanner->files, file_entry);
		scanner->files_count += 1;
		LCUICond_Signal(&scanner->cond);
		LCUIMutex_Unlock(&scanner->mutex);
	}
	scanner->is_async_scaning = FALSE;
	LCUICond_Signal(&scanner->cond_scan);
	LCUIMutex_Unlock(&scanner->mutex_scan);
}

static void FileScanner_ScanDirs(FileScanner scanner)
{
	LCUIMutex_Lock(&scanner->mutex_scan);
	scanner->is_async_scaning = TRUE;
	FileStorage_GetFolders(finder.storage_for_scan, scanner->dirpath,
			       FileScanner_OnGetDirs, scanner);
	while (scanner->is_async_scaning) {
		LCUICond_Wait(&scanner->cond_scan, &scanner->mutex_scan);
	}
	LCUIMutex_Unlock(&scanner->mutex_scan);
}

static void FileScanner_ScanFiles(FileScanner scanner)
{
	DB_File file;
	DB_Query query;
	FileEntry entry;
	int i, count;
	view.terms.limit = 50;
	view.terms.offset = 0;
	view.terms.dirpath = EncodeUTF8(scanner->dirpath);
	query = DB_NewQuery(&view.terms);
	count = DBQuery_GetTotalFiles(query);
	while (scanner->is_running && count > 0) {
		DB_DeleteQuery(query);
		query = DB_NewQuery(&view.terms);
		if (count < view.terms.limit) {
			i = count;
		} else {
			i = view.terms.limit;
		}
		for (; scanner->is_running && i > 0; --i) {
			file = DBQuery_FetchFile(query);
			if (!file) {
				break;
			}
			entry = NEW(FileEntryRec, 1);
			entry->is_dir = FALSE;
			entry->file = file;
			entry->path = file->path;
			DEBUG_MSG("file: %s\n", file->path);
			LCUIMutex_Lock(&scanner->mutex);
			LinkedList_Append(&scanner->files, entry);
			scanner->files_count += 1;
			LCUICond_Signal(&scanner->cond);
			LCUIMutex_Unlock(&scanner->mutex);
		}
		count -= view.terms.limit;
		view.terms.offset += view.terms.limit;
	}
	free(view.terms.dirpath);
}

static int FileScanner_LoadSourceDirs(FileScanner scanner)
{
	int i, n;
	DB_Dir *dirs;
	LCUIMutex_Lock(&scanner->mutex);
	n = LCFinder_GetSourceDirList(&dirs);
	for (i = 0; i < n; ++i) {
		FileEntry entry = NEW(FileEntryRec, 1);
		size_t len = strlen(dirs[i]->path) + 1;
		char *path = malloc(sizeof(char) * len);
		strcpy(path, dirs[i]->path);
		entry->path = path;
		entry->is_dir = TRUE;
		LinkedList_Append(&scanner->files, entry);
		scanner->files_count += 1;
	}
	LCUICond_Signal(&scanner->cond);
	LCUIMutex_Unlock(&scanner->mutex);
	free(dirs);
	return n;
}

/** 初始化文件扫描 */
static void FileScanner_Init(FileScanner scanner)
{
	LCUICond_Init(&scanner->cond);
	LCUICond_Init(&scanner->cond_scan);
	LCUIMutex_Init(&scanner->mutex);
	LCUIMutex_Init(&scanner->mutex_scan);
	LinkedList_Init(&scanner->files);
	scanner->files_count = 0;
	scanner->is_async_scaning = FALSE;
	scanner->is_running = FALSE;
	scanner->dirpath = NULL;
}

/** 重置文件扫描 */
static void FileScanner_Reset(FileScanner scanner)
{
	if (scanner->is_running) {
		scanner->is_running = FALSE;
		LCUIThread_Join(scanner->tid, NULL);
	}
	LCUIMutex_Lock(&scanner->mutex);
	LinkedList_Clear(&scanner->files, OnDeleteFileEntry);
	scanner->files_count = 0;
	LCUICond_Signal(&scanner->cond);
	LCUIMutex_Unlock(&scanner->mutex);
}

static void FileScanner_Destroy(FileScanner scanner)
{
	FileScanner_Reset(scanner);
	LCUICond_Destroy(&scanner->cond);
	LCUICond_Destroy(&scanner->cond_scan);
	LCUIMutex_Destroy(&scanner->mutex);
	LCUIMutex_Destroy(&scanner->mutex_scan);
}

static void FileScanner_Thread(void *arg)
{
	FileScanner scanner = arg;
	scanner->is_running = TRUE;
	if (scanner->dirpath) {
		FileScanner_ScanDirs(scanner);
		FileScanner_ScanFiles(scanner);
	} else {
		FileScanner_LoadSourceDirs(scanner);
	}
	_DEBUG_MSG("scan files: %d\n", scanner->files_count);
	if (scanner->files_count > 0) {
		Widget_AddClass(view.tip_empty, "hide");
		Widget_Hide(view.tip_empty);
	} else {
		Widget_RemoveClass(view.tip_empty, "hide");
		Widget_Show(view.tip_empty);
	}
	scanner->is_running = FALSE;
	LCUIThread_Exit(NULL);
}

/** 开始扫描 */
static void FileScanner_Start(FileScanner scanner, char *path)
{
	if (scanner->dirpath) {
		free(scanner->dirpath);
	}
	if (path) {
		scanner->dirpath = DecodeUTF8(path);
	} else {
		scanner->dirpath = NULL;
	}
	LCUIThread_Create(&scanner->tid, FileScanner_Thread, scanner);
}

static void OnItemClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	FileEntry entry = e->data;
	DEBUG_MSG("open file: %s\n", path);
	if (entry->is_dir) {
		OpenFolder(entry->path);
	}
}

/** 在缩略图列表准备完毕的时候 */
static void OnThumbViewReady(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	LCUICond_Signal(&view.viewsync.ready);
}

/** 视图同步线程 */
static void ViewSync_Thread(void *arg)
{
	ViewSync vs;
	LCUI_Widget item;
	FileEntry entry;
	FileScanner scanner;
	LinkedListNode *node;
	vs = &view.viewsync;
	scanner = &view.scanner;
	LCUIMutex_Lock(&vs->mutex);
	/* 等待缩略图列表部件准备完毕 */
	while (view.items->state < LCUI_WSTATE_READY) {
		LCUICond_TimedWait(&vs->ready, &vs->mutex, 100);
	}
	LCUIMutex_Unlock(&vs->mutex);
	vs->prev_item_type = -1;
	vs->is_running = TRUE;
	while (vs->is_running) {
		LCUIMutex_Lock(&scanner->mutex);
		if (scanner->files.length == 0) {
			LCUICond_Wait(&scanner->cond, &scanner->mutex);
			if (!vs->is_running) {
				LCUIMutex_Unlock(&scanner->mutex);
				break;
			}
		}
		LCUIMutex_Lock(&vs->mutex);
		node = LinkedList_GetNode(&scanner->files, 0);
		if (!node) {
			LCUIMutex_Unlock(&vs->mutex);
			LCUIMutex_Unlock(&scanner->mutex);
			continue;
		}
		entry = node->data;
		LinkedList_Unlink(&scanner->files, node);
		LCUIMutex_Unlock(&scanner->mutex);
		if (vs->prev_item_type != -1 &&
		    vs->prev_item_type != entry->is_dir) {
			LCUI_Widget separator = LCUIWidget_New(NULL);
			Widget_AddClass(separator, "divider");
			FileBrowser_Append(&view.browser, separator);
		}
		vs->prev_item_type = entry->is_dir;
		if (entry->is_dir) {
			item = FileBrowser_AppendFolder(&view.browser,
							entry->path,
							view.dir == NULL);
			Widget_BindEvent(item, "click", OnItemClick, entry,
					 NULL);
		} else {
			FileBrowser_AppendPicture(&view.browser,
						  entry->file);
			free(node->data);
		}
		LinkedListNode_Delete(node);
		LCUIMutex_Unlock(&vs->mutex);
	}
	LCUIThread_Exit(NULL);
}

static void OpenFolder(const char *dirpath)
{
	DB_Dir dir = NULL, *dirs = NULL;
	char *path = NULL, *scan_path = NULL;

	if (dirpath) {
		size_t len = strlen(dirpath);
		int i, n = LCFinder_GetSourceDirList(&dirs);
		path = malloc(sizeof(char) * (len + 1));
		scan_path = malloc(sizeof(char) * (len + 2));
		strcpy(scan_path, dirpath);
		strcpy(path, dirpath);
		for (i = 0; i < n; ++i) {
			if (dirs[i] && strstr(dirs[i]->path, path)) {
				dir = dirs[i];
				break;
			}
		}
		if (scan_path[len - 1] != PATH_SEP) {
			scan_path[len++] = PATH_SEP;
			scan_path[len] = 0;
		} else {
			path[len] = 0;
		}
		TextView_SetText(view.info_name, getfilename(path));
		TextView_SetText(view.info_path, path);
		Widget_AddClass(view.view, "show-folder-info-box");
		free(dirs);
	} else {
		Widget_RemoveClass(view.view, "show-folder-info-box");
	}
	if (view.dirpath) {
		free(view.dirpath);
		view.dirpath = NULL;
	}
	FileScanner_Reset(&view.scanner);
	LCUIMutex_Lock(&view.viewsync.mutex);
	view.dir = dir;
	view.dirpath = path;
	view.viewsync.prev_item_type = -1;
	FileBrowser_Empty(&view.browser);
	FileScanner_Start(&view.scanner, path);
	LCUIMutex_Unlock(&view.viewsync.mutex);
	DEBUG_MSG("done\n");
}

static void OnSyncDone(void *privdata, void *arg)
{
	OpenFolder(NULL);
}

static void UpdateQueryTerms(void)
{
	view.terms.modify_time = NONE;
	view.terms.create_time = NONE;
	view.terms.score = NONE;
	switch (finder.config.files_sort) {
	case CREATE_TIME_DESC:
		view.terms.create_time = DESC;
		break;
	case CREATE_TIME_ASC:
		view.terms.create_time = ASC;
		break;
	case SCORE_DESC:
		view.terms.score = DESC;
		break;
	case SCORE_ASC:
		view.terms.score = ASC;
		break;
	case MODIFY_TIME_ASC:
		view.terms.modify_time = ASC;
		break;
	case MODIFY_TIME_DESC:
	default:
		view.terms.modify_time = DESC;
		break;
	}
}

static void OnSelectSortMethod(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	FileSortMethod sort;
	
	sort = (FileSortMethod)Widget_GetAttribute(e->target, "value");
	if (finder.config.files_sort == sort->value) {
		return;
	}
	finder.config.files_sort = sort->value;
	if (view.selected_sort) {
		Widget_RemoveClass(view.selected_sort, "selected");
	}
	view.selected_sort = e->target;
	Widget_AddClass(e->target, "selected");
	UpdateQueryTerms();
	OpenFolder(view.dirpath);
	LCFinder_SaveConfig();
}

static void InitFolderFilesSort(void)
{
	int i;
	LCUI_Widget menu, item, icon, text;

	SelectWidget(menu, ID_DROPDOWN_FOLDER_FILES_SORT);
	Widget_Empty(menu);

	item = LCUIWidget_New("textview-i18n");
	TextViewI18n_SetKey(item, KEY_SORT_HEADER);
	Widget_AddClass(item, "dropdown-header");
	Widget_Append(menu, item);

	for (i = 0; i < SORT_METHODS_LEN; ++i) {
		FileSortMethod sort = &sort_methods[i];
		item = LCUIWidget_New(NULL);
		icon = LCUIWidget_New("textview");
		text = LCUIWidget_New("textview-i18n");
		TextViewI18n_SetKey(text, sort->name_key);
		Widget_SetAttributeEx(item, "value", sort, 0, NULL);
		Widget_AddClass(item, "dropdown-item");
		Widget_AddClass(icon, "icon icon-check");
		Widget_AddClass(text, "text");
		Widget_Append(item, icon);
		Widget_Append(item, text);
		Widget_Append(menu, item);
		if (sort->value == finder.config.files_sort) {
			Widget_AddClass(item, "selected");
			view.selected_sort = item;
		}
	}

	BindEvent(menu, "change.dropdown", OnSelectSortMethod);
	UpdateQueryTerms();
}

void UI_InitFoldersView(void)
{
	LCUI_Widget title;
	LCUI_Widget btn[5], btn_return;

	FileScanner_Init(&view.scanner);
	LCUICond_Init(&view.viewsync.ready);
	LCUIMutex_Init(&view.viewsync.mutex);
	SelectWidget(view.items, ID_VIEW_FILE_LIST);
	SelectWidget(title, ID_TXT_FOLDERS_SELECTION_STATS);
	SelectWidget(btn[0], ID_BTN_SYNC_FOLDER_FILES);
	SelectWidget(btn[1], ID_BTN_SELECT_FOLDER_FILES);
	SelectWidget(btn[2], ID_BTN_CANCEL_FOLDER_SELECT);
	SelectWidget(btn[3], ID_BTN_TAG_FOLDER_FILES);
	SelectWidget(btn[4], ID_BTN_DELETE_FOLDER_FILES);
	SelectWidget(btn_return, ID_BTN_RETURN_ROOT_FOLDER);
	SelectWidget(view.view, ID_VIEW_FOLDERS);
	SelectWidget(view.info, ID_VIEW_FOLDER_INFO);
	SelectWidget(view.info_name, ID_VIEW_FOLDER_INFO_NAME);
	SelectWidget(view.info_path, ID_VIEW_FOLDER_INFO_PATH);
	SelectWidget(view.tip_empty, ID_TIP_FOLDERS_EMPTY);
	view.browser.btn_tag = btn[3];
	view.browser.btn_select = btn[1];
	view.browser.btn_cancel = btn[2];
	view.browser.btn_delete = btn[4];
	view.browser.txt_selection_stats = title;
	view.browser.view = view.view;
	view.browser.items = view.items;
	BindEvent(btn[0], "click", OnBtnSyncClick);
	BindEvent(btn_return, "click", OnBtnReturnClick);
	BindEvent(view.items, "ready", OnThumbViewReady);
	BindEvent(view.view, "show.view", OnViewShow);
	ThumbView_SetCache(view.items, finder.thumb_cache);
	ThumbView_SetStorage(view.items, finder.storage_for_thumb);
	LCUIThread_Create(&view.viewsync.tid, ViewSync_Thread, NULL);
	LCFinder_BindEvent(EVENT_SYNC_DONE, OnSyncDone, NULL);
	LCFinder_BindEvent(EVENT_DIR_ADD, OnAddDir, NULL);
	LCFinder_BindEvent(EVENT_DIR_DEL, OnFolderChange, NULL);
	FileBrowser_Create(&view.browser);
	view.is_activated = TRUE;
	InitFolderFilesSort();
	OpenFolder(NULL);
}

void UI_FreeFoldersView(void)
{
	if (!view.is_activated) {
		return;
	}
	view.viewsync.is_running = FALSE;
	FileScanner_Reset(&view.scanner);
	LCUIThread_Join(view.viewsync.tid, NULL);
	FileScanner_Destroy(&view.scanner);
	LCUIMutex_Unlock(&view.viewsync.mutex);
}
