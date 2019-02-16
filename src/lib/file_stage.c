/* ***************************************************************************
 * file_stage.c -- file stage
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
 * file_stage.c -- 文件暂存区，支持异步读写文件列表
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

#define HAVE_FILE_STAGE
#include <stdlib.h>
#include <LCUI_Build.h>
#include <LCUI/types.h>
#include <LCUI/thread.h>
#include <LCUI/util/linkedlist.h>
#include "file_stage.h"

typedef struct FileStageRec_ {
	LCUI_BOOL is_running;
	LCUI_Mutex mutex;

	LinkedList buffer;
	LinkedList files;
} FileStageRec;

FileStage FileStage_Create(void)
{
	FileStage stage;

	stage = malloc(sizeof(FileStageRec));
	stage->is_running = FALSE;
	LCUIMutex_Init(&stage->mutex);
	LinkedList_Init(&stage->buffer);
	LinkedList_Init(&stage->files);
	return stage;
}

void FileStage_Destroy(FileStage stage)
{
	LCUIMutex_Destroy(&stage->mutex);
	LinkedList_Clear(&stage->buffer, NULL);
	LinkedList_Clear(&stage->files, NULL);
	free(stage);
}

void FileStage_AddFile(FileStage stage, void *file)
{
	LinkedList_Append(&stage->buffer, file);
}

void FileStage_Commit(FileStage stage)
{
	LCUIMutex_Lock(&stage->mutex);
	LinkedList_Concat(&stage->files, &stage->buffer);
	LCUIMutex_Unlock(&stage->mutex);
}

size_t FileStage_GetFiles(FileStage stage, LinkedList *files, size_t max_files)
{
	size_t count = 0;
	LinkedListNode *node;

	LCUIMutex_Lock(&stage->mutex);
	if (max_files > 0) {
		node = LinkedList_GetNode(&stage->files, 0);
		while(node && count < max_files) {
			++count;
			LinkedList_Unlink(&stage->files, node);
			LinkedList_AppendNode(files, node);
			node = LinkedList_GetNode(&stage->files, 0);
		}
		LinkedList_Concat(files, &stage->files);
	} else {
		LinkedList_Concat(files, &stage->files);
		count = files->length;
	}
	LCUIMutex_Unlock(&stage->mutex);
	return count;
}
