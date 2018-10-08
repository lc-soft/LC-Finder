/* ***************************************************************************
 * picture_scanner.c -- the picture scanner for picture view
 *
 * Copyright (C) 2018 by Liu Chao <lc-soft@live.cn>
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
 * picture_scanner.c -- 图片视图的图片扫描器
 *
 * 版权所有 (C) 2018 归属于 刘超 <lc-soft@live.cn>
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

#include <stdlib.h>
#include <string.h>

#include <LCUI_Build.h>
#include <LCUI/types.h>
#include <LCUI/util/dict.h>
#include <LCUI/util/linkedlist.h>
#include <LCUI/font/charset.h>

#include "build.h"
#include "types.h"
#include "common.h"
#include "file_storage.h"
#include "picture.h"

/** 文件索引记录 */
typedef struct FileIndexRec_ {
	wchar_t *name;		/**< 文件名 */
	LinkedListNode node;	/**< 在列表中的节点 */
} FileIndexRec, *FileIndex;

/** 文件扫描器数据结构 */
typedef struct FileScannerRec_ {
	int storage;
	LCUI_BOOL is_ready;		/**< 是否已经准备好 */
	LCUI_BOOL is_running;		/**< 是否正在运行 */
	wchar_t *dirpath;		/**< 扫描目录 */
	wchar_t *file;			/**< 目标文件，当扫描到该文件后会创建文件迭代器 */
	LinkedList files;		/**< 已扫描到的文件列表 */
	FileIterator iterator;		/**< 文件迭代器 */

	/**< 回调函数，当定位到目标文件的迭代器时调用 */
	FileIteratorFunc on_found;

	/**< 回调函数，当目标文件前后的文件已扫描完时调用 */
	void(*on_active)(void);
} FileScannerRec, *FileScanner;

typedef struct FileDataPackRTec_ {
	FileIndex fidx;
	FileScanner scanner;
} FileDataPackRec, *FileDataPack;

static void FileIndex_Destroy(FileIndex fidx)
{
	free(fidx->name);
	free(fidx);
}

static void FileIndex_OnDestroy(void *data)
{
	FileIndex_Destroy(data);
}

static void FileIterator_Destroy(FileIterator iter)
{
	free(iter->filepath);
	iter->privdata = NULL;
	iter->filepath = NULL;
	iter->next = NULL;
	iter->prev = NULL;
	free(iter);
}

static void FileIterator_Update(FileIterator iter)
{
	wchar_t wpath[PATH_LEN];
	FileDataPack data = iter->privdata;
	wpathjoin(wpath, data->scanner->dirpath, data->fidx->name);
	iter->length = data->scanner->files.length;
	LCUI_EncodeUTF8String(iter->filepath, wpath, PATH_LEN);
}

static void FileIterator_Next(FileIterator iter)
{
	FileDataPack data = iter->privdata;
	if (data->fidx->node.next) {
		iter->index += 1;
		data->fidx = data->fidx->node.next->data;
		FileIterator_Update(iter);
	}
}

static void FileIterator_Prev(FileIterator iter)
{
	FileDataPack data = iter->privdata;
	if (data->fidx->node.prev &&
	    &data->fidx->node != data->scanner->files.head.next) {
		iter->index -= 1;
		data->fidx = data->fidx->node.prev->data;
		FileIterator_Update(iter);
	}
}

static FileIterator FileIterator_Create(FileScanner scanner, FileIndex fidx)
{
	LinkedListNode *node = &fidx->node;
	FileDataPack data = NEW(FileDataPackRec, 1);
	FileIterator iter = NEW(FileIteratorRec, 1);

	data->fidx = fidx;
	data->scanner = scanner;
	iter->index = 0;
	iter->privdata = data;
	iter->next = FileIterator_Next;
	iter->prev = FileIterator_Prev;
	iter->destroy = FileIterator_Destroy;
	iter->filepath = NEW(char, PATH_LEN);
	while (node != scanner->files.head.next) {
		node = node->prev;
		iter->index += 1;
	}
	FileIterator_Update(iter);
	return iter;
}

static void OnOpenDir(FileStatus *status, FileStream stream, void *data)
{
	size_t len;
	int i = 0, pos = -1;
	char buf[PATH_LEN + 2];
	FileScanner fs = data;

	if (!status || status->type != FILE_TYPE_DIRECTORY || !stream) {
		return;
	}
	while (fs->is_running) {
		char *p;
		FileIndex fidx;

		buf[0] = 0;
		p = FileStream_ReadLine(stream, buf, PATH_LEN + 2);
		if (p == NULL) {
			break;
		}
		/* 忽略文件夹 */
		if (buf[0] == 'd') {
			continue;
		}
		len = strlen(buf);
		if (len < 2) {
			continue;
		}
		buf[len - 1] = 0;
		fidx = NEW(FileIndexRec, 1);
		fidx->name = DecodeUTF8(buf + 1);
		fidx->node.data = fidx;
		LinkedList_AppendNode(&fs->files, &fidx->node);
		if (wcscmp(fidx->name, fs->file) == 0 && !fs->iterator) {
			fs->iterator = FileIterator_Create(fs, fidx);
			pos = i;
		}
		if (pos < 0 && fs->iterator) {
			FileIterator_Update(fs->iterator);
			fs->on_found(fs->iterator);
		}
		if (pos >= 0 && i - pos > 2) {
			fs->on_active();
			pos = -1;
		}
		++i;
	}
	if (fs->iterator && pos >= 0) {
		FileIterator_Update(fs->iterator);
		fs->on_found(fs->iterator);
		fs->on_active();
	}
	fs->is_running = FALSE;
}

static FileScanner FileScanner_Create(int storage)
{
	FileScanner scanner = malloc(sizeof(FileScannerRec));

	scanner->iterator = NULL;
	scanner->is_running = FALSE;
	scanner->is_ready = TRUE;
	LinkedList_Init(&scanner->files);
	return scanner;
}

static int FileScanner_Start(FileScanner scanner, const wchar_t *filepath,
			     void(*on_found)(FileIterator),
			     void(*on_active)(void))
{
	size_t len;
	const wchar_t *name;

	if (scanner->is_ready) {
		return -1;
	}
	scanner->is_ready = FALSE;
	scanner->is_running = TRUE;
	scanner->on_found = on_found;
	if (scanner->dirpath) {
		free(scanner->dirpath);
	}
	if (scanner->file) {
		free(scanner->file);
	}
	name = wgetfilename(filepath);
	len = wcslen(name) + 1;
	scanner->dirpath = wgetdirname(filepath);
	scanner->file = malloc(sizeof(wchar_t) * len);
	wcscpy(scanner->file, name);
	return FileStorage_GetFile(scanner->storage, scanner->dirpath,
				   OnOpenDir, scanner);
}

static void FileScanner_Exit(FileScanner scanner)
{
	scanner->is_running = FALSE;
	LinkedList_ClearData(&scanner->files, FileIndex_OnDestroy);
	free(scanner->file);
	scanner->file = NULL;
}

void *PictureView_CreateScanner(int storage)
{
	return FileScanner_Create(storage);
}

int PictureView_OpenScanner(void *scanner, const wchar_t *filepath,
			    void(*on_found)(FileIterator),
			    void (*on_active)(void))
{
	return FileScanner_Start(scanner, filepath, on_found, on_active);
}

void PictureView_CloseScanner(void *scanner)
{
	FileScanner_Exit(scanner);
}

void PictureView_FreeScanner(void *scanner)
{
	free(scanner);
}
