/* ***************************************************************************
 * detector.h -- detector
 *
 * Copyright (C) 2019 by Liu Chao <lc-soft@live.cn>
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
 * detector.h -- 检测器，基于 darknet 实现
 *
 * 版权所有 (C) 2019 归属于 刘超 <lc-soft@live.cn>
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

#ifndef LCFINDER_DETECTOR_H
#define LCFINDER_DETECTOR_H

#include "file_search.h"

typedef enum DetectorTaskType {
	DETECTOR_TASK_DETECT,
	DETECTOR_TASK_TRAIN,
	DETECTOR_TASK_TOTAL_NUM
} DetectorTaskType;

typedef enum DetetctorTaskState {
	DETECTOR_TASK_STATE_READY,
	DETECTOR_TASK_STATE_PREPARING,
	DETECTOR_TASK_STATE_RUNNING,
	DETECTOR_TASK_STATE_FINISHED
} DetetctorTaskState;

typedef struct DetectorTaskRec_ {
	DetectorTaskType type;
	DetetctorTaskState state;

	LCUI_BOOL cancel;

	size_t current;
	size_t total;
} DetectorTaskRec, *DetectorTask;

int Detector_Init(const wchar_t *workdir);

void Detector_Free(void);

wchar_t **Detector_GetModels(void);

int Detector_SetModel(const wchar_t *name);

DetectorTask Detector_CreateTask(DetectorTaskType type);

void Detector_CancelTask(DetectorTask task);

void Detector_FreeTask(DetectorTask task);

int Detector_RunTask(DetectorTask task);

void Detector_RunTaskAync(DetectorTask task);

#endif
