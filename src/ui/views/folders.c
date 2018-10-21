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
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "file_storage.h"
#include "file_stage.h"
#include "thumbview.h"
#include "textview_i18n.h"
#include "browser.h"
#include "i18n.h"

#define KEY_SORT_HEADER		"sort.header"
#define KEY_TITLE		"folders.title"
#define THUMB_CACHE_SIZE	(20 * 1024 * 1024)

/** 文件扫描功能的相关数据 */
typedef struct FileScannerRec_ {
	int timer;
	FileStage stage;
	LCUI_Thread tid;
	LCUI_Cond cond_scan;
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
	int prev_item_type;
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
		DBFile_Release(entry->file);
	}
	free(entry);
}

static void FileScanner_OnGetDirs(FileStatus *status, FileStream stream,
				  void *data)
{
	char *dirpath;
	size_t len, dirpath_len;
	FileScanner scanner = data;
	FileEntry entry;

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
		entry = NEW(FileEntryRec, 1);
		entry->is_dir = TRUE;
		entry->file = NULL;
		len = len + dirpath_len;
		entry->path = malloc(sizeof(char) * len);
		pathjoin(entry->path, dirpath, buf + 1);
		FileStage_AddFile(scanner->stage, entry);
	}
	FileStage_Commit(scanner->stage);
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

static size_t FileScanner_ScanFiles(FileScanner scanner)
{
	DB_File file;
	DB_Query query;
	FileEntry entry;
	size_t i, count, total;

	view.terms.limit = 512;
	view.terms.offset = 0;
	view.terms.dirpath = EncodeUTF8(scanner->dirpath);

	query = DB_NewQuery(&view.terms);
	total = DBQuery_GetTotalFiles(query);
	DB_DeleteQuery(query);

	for (count = 0; scanner->is_running;) {
		query = DB_NewQuery(&view.terms);
		for (i = 0; scanner->is_running; ++count, ++i) {
			file = DBQuery_FetchFile(query);
			if (!file) {
				break;
			}
			entry = NEW(FileEntryRec, 1);
			entry->is_dir = FALSE;
			entry->file = file;
			entry->path = file->path;
			DEBUG_MSG("file: %s\n", file->path);
			FileStage_AddFile(scanner->stage, entry);
		}
		view.terms.offset += view.terms.limit;
		FileStage_Commit(scanner->stage);
		DB_DeleteQuery(query);
		if (i < view.terms.limit) {
			break;
		}
	}
	FileStage_Commit(scanner->stage);
	free(view.terms.dirpath);
	return count;
}

static int FileScanner_LoadSourceDirs(FileScanner scanner)
{
	size_t i, n;
	DB_Dir *dirs;
	n = LCFinder_GetSourceDirList(&dirs);
	for (i = 0; i < n; ++i) {
		FileEntry entry = NEW(FileEntryRec, 1);
		size_t len = strlen(dirs[i]->path) + 1;
		char *path = malloc(sizeof(char) * len);
		strcpy(path, dirs[i]->path);
		entry->path = path;
		entry->is_dir = TRUE;
		FileStage_AddFile(scanner->stage, entry);
	}
	FileStage_Commit(scanner->stage);
	free(dirs);
	return n;
}

static void OnItemClick(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	FileEntry entry = e->data;
	DEBUG_MSG("open file: %s\n", path);
	if (entry->is_dir) {
		OpenFolder(entry->path);
	}
}

static void FoldersView_AppendFile(FileEntry entry)
{
	LCUI_Widget item, separator;

	if (view.prev_item_type != -1 &&
	    view.prev_item_type != entry->is_dir) {
		separator = LCUIWidget_New(NULL);
		Widget_AddClass(separator, "divider");
		FileBrowser_Append(&view.browser, separator);
	}
	view.prev_item_type = entry->is_dir;
	if (entry->is_dir) {
		item = FileBrowser_AppendFolder(&view.browser,
						entry->path,
						view.dir == NULL);
		Widget_BindEvent(item, "click", OnItemClick, entry, NULL);
	} else {
		FileBrowser_AppendPicture(&view.browser,
					  entry->file);
	}
}

static void FoldersView_AppendFiles(void *unused)
{
	LinkedList files;
	LinkedListNode *node;
	FileScanner scanner = &view.scanner;

	view.prev_item_type = -1;
	LinkedList_Init(&files);
	FileStage_GetFiles(scanner->stage, &files);
	for (LinkedList_Each(node, &files)) {
		FoldersView_AppendFile(node->data);
	}
	LinkedList_Concat(&scanner->files, &files);
	if (!scanner->is_running) {
		scanner->timer = 0;
		return;
	}
	scanner->timer = LCUI_SetTimeout(200, FoldersView_AppendFiles, NULL);
}

/** 初始化文件扫描 */
static void FileScanner_Init(FileScanner scanner)
{
	scanner->timer = 0;
	scanner->stage = FileStage_Create();
	scanner->is_async_scaning = FALSE;
	scanner->is_running = FALSE;
	scanner->dirpath = NULL;
	LCUICond_Init(&scanner->cond_scan);
	LCUIMutex_Init(&scanner->mutex_scan);
	LinkedList_Init(&scanner->files);
}

/** 重置文件扫描 */
static void FileScanner_Reset(FileScanner scanner)
{
	if (scanner->is_running) {
		scanner->is_running = FALSE;
		LCUIThread_Join(scanner->tid, NULL);
	}
	if (scanner->timer) {
		LCUITimer_Free(scanner->timer);
		scanner->timer = 0;
	}
	FileStage_GetFiles(scanner->stage, &scanner->files);
	LinkedList_Clear(&scanner->files, OnDeleteFileEntry);
}

static void FileScanner_Destroy(FileScanner scanner)
{
	FileScanner_Reset(scanner);
	FileStage_Destroy(scanner->stage);
	LCUICond_Destroy(&scanner->cond_scan);
	LCUIMutex_Destroy(&scanner->mutex_scan);
}

static void FileScanner_Thread(void *arg)
{
	size_t count;
	FileScanner scanner = arg;
	if (scanner->dirpath) {
		FileScanner_ScanDirs(scanner);
		count = FileScanner_ScanFiles(scanner);
	} else {
		count = FileScanner_LoadSourceDirs(scanner);
	}
	_DEBUG_MSG("scan files: %lu\n", scanner->files.length);
	if (count > 0) {
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
	FileScanner_Reset(scanner);
	if (scanner->dirpath) {
		free(scanner->dirpath);
	}
	if (path) {
		scanner->dirpath = DecodeUTF8(path);
	} else {
		scanner->dirpath = NULL;
	}
	scanner->is_running = TRUE;
	LCUIThread_Create(&scanner->tid, FileScanner_Thread, scanner);
	scanner->timer = LCUI_SetTimeout(200, FoldersView_AppendFiles, NULL);
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
	view.dir = dir;
	view.dirpath = path;
	view.prev_item_type = -1;
	FileBrowser_Empty(&view.browser);
	FileScanner_Start(&view.scanner, path);
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

static void FoldersView_InitBase(void)
{
	SelectWidget(view.items, ID_VIEW_FILE_LIST);
	SelectWidget(view.view, ID_VIEW_FOLDERS);
	SelectWidget(view.info, ID_VIEW_FOLDER_INFO);
	SelectWidget(view.info_name, ID_VIEW_FOLDER_INFO_NAME);
	SelectWidget(view.info_path, ID_VIEW_FOLDER_INFO_PATH);
	SelectWidget(view.tip_empty, ID_TIP_FOLDERS_EMPTY);
	LCFinder_BindEvent(EVENT_SYNC_DONE, OnSyncDone, NULL);
	LCFinder_BindEvent(EVENT_DIR_ADD, OnAddDir, NULL);
	LCFinder_BindEvent(EVENT_DIR_DEL, OnFolderChange, NULL);
}

static void FoldersView_InitBrowser(void)
{
	LCUI_Widget title;
	LCUI_Widget btn[5], btn_return;

	SelectWidget(title, ID_TXT_FOLDERS_SELECTION_STATS);
	SelectWidget(btn[0], ID_BTN_SYNC_FOLDER_FILES);
	SelectWidget(btn[1], ID_BTN_SELECT_FOLDER_FILES);
	SelectWidget(btn[2], ID_BTN_CANCEL_FOLDER_SELECT);
	SelectWidget(btn[3], ID_BTN_TAG_FOLDER_FILES);
	SelectWidget(btn[4], ID_BTN_DELETE_FOLDER_FILES);
	SelectWidget(btn_return, ID_BTN_RETURN_ROOT_FOLDER);
	BindEvent(btn[0], "click", OnBtnSyncClick);
	BindEvent(btn_return, "click", OnBtnReturnClick);

	view.browser.btn_tag = btn[3];
	view.browser.btn_select = btn[1];
	view.browser.btn_cancel = btn[2];
	view.browser.btn_delete = btn[4];
	view.browser.txt_selection_stats = title;
	view.browser.view = view.view;
	view.browser.items = view.items;
	ThumbView_SetCache(view.items, finder.thumb_cache);
	ThumbView_SetStorage(view.items, finder.storage_for_thumb);
}

void UI_InitFoldersView(void)
{
	FoldersView_InitBase();
	FoldersView_InitBrowser();
	FileScanner_Init(&view.scanner);
	BindEvent(view.view, "show.view", OnViewShow);
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
	FileScanner_Reset(&view.scanner);
	FileScanner_Destroy(&view.scanner);
}
