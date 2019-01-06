/* ***************************************************************************
 * settings_detector.c -- detector setting view
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
 * settings_detector.c -- “设置”视图中的检测器设置项
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

#include <stdio.h>
#include <string.h>
#include "finder.h"
#include <LCUI/timer.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "ui.h"
#include "i18n.h"
#include "taskitem.h"
#include "settings.h"
#include "detector.h"

#define KEY_DETECTOR_DETECTION_TITLE	"settings.detector.tasks.detection.title"
#define KEY_DETECTOR_DETECTION_TEXT	"settings.detector.tasks.detection.text"
#define KEY_DETECTOR_TRAINING_TITLE	"settings.detector.tasks.training.title"
#define KEY_DETECTOR_TRAINING_TEXT	"settings.detector.tasks.training.text"
#define KEY_DETECTOR_INIT_FAILED	"message.detector_init_failed"

typedef struct TaskControllerRec_ {
	int timer;
	LCUI_Widget view;
	DetectorTask task;
} TaskControllerRec, *TaskController;

static struct DetectorSettingView {
	LCUI_Widget view;
	TaskControllerRec detection;
	TaskControllerRec training;
} view;

static void RefreshDropdownText(void)
{
	LCUI_Widget txt;

	SelectWidget(txt, ID_TXT_CURRENT_DETECTOR_MODEL);
	TextView_SetTextW(txt, finder.config.detector_model_name);
}

static void TaskForSetModel(void *arg1, void *arg2)
{
	wchar_t *name = arg1;
	const wchar_t *message;

	if (Detector_SetModel(name) == 0) {
		wcscpy(finder.config.detector_model_name, name);
		LCFinder_SaveConfig();
	} else {
		message = I18n_GetText(KEY_DETECTOR_INIT_FAILED);
		TaskItem_SetError(view.detection.view, message);
	}
	free(name);
}

static void TaskForStartDetect(void *arg1, void *arg2)
{
	TaskController t = arg1;

	Detector_RunTaskAync(t->task);
}

static int SetDetectorModelAsync(const wchar_t *name)
{
	LCUI_TaskRec task = { 0 };

	task.arg[0] = wcsdup2(name);
	task.func = TaskForSetModel;
	return LCUI_PostAsyncTask(&task);
}

static void OnDetectionProgress(void *arg)
{
	TaskController t = &view.detection;

	TaskItem_SetProgress(t->view, t->task->current, t->task->total);
}

static OnStartDetect(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	int worker;
	LCUI_TaskRec task = { 0 };
	TaskController t = &view.detection;
	wchar_t *model = finder.config.detector_model_name;

	t->task = Detector_CreateTask(DETECTOR_TASK_DETECT);
	t->timer = LCUI_SetInterval(500, OnDetectionProgress, NULL);
	task.arg[0] = t;
	task.func = TaskForStartDetect;
	worker = SetDetectorModelAsync(model);
	LCUI_PostAsyncTaskTo(&task, worker);
}

static OnStopDetect(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	//Detector_CancelTask(view.detection.task);
}

static void OnChangeModel(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	const wchar_t *value;
	
	value = (const wchar_t*)Widget_GetAttribute(e->target, "value");
	if (!value || wcslen(value) < 1) {
		return;
	}
	wcscpy(finder.config.detector_model_name, value);
	LCFinder_SaveConfig();
	RefreshDropdownText();
}

static void InitModelsDropdown(void)
{
	size_t i;
	wchar_t **names;
	wchar_t *selected_model;
	LCUI_Widget menu;
	LCUI_Widget item;
	LCUI_BOOL found_model = FALSE;

	SelectWidget(menu, ID_DROPDOWN_DETECTOR_MODELS);
	BindEvent(menu, "change.dropdown", OnChangeModel);

	names = Detector_GetModels();
	selected_model = finder.config.detector_model_name;
	for (i = 0; names[i]; ++i) {
		item = LCUIWidget_New("textview");
		Widget_AddClass(item, "dropdown-item");
		Widget_SetAttributeEx(item, "value", names[i], 0, NULL);
		Widget_Append(menu, item);
		TextView_SetTextW(item, names[i]);
		if (wcscmp(selected_model, names[i]) == 0) {
			found_model = TRUE;
		}
	}
	if (i > 0) {
		if (!found_model) {
			wcscpy(finder.config.detector_model_name, names[0]);
			TaskItem_SetActionDisabled(view.detection.view, TRUE);
			LCFinder_SaveConfig();
		}
		Widget_AddClass(view.view, "models-available");
	} else {
		Widget_AddClass(view.view, "models-unavailable");
	}
	RefreshDropdownText();
}

void SettingsView_InitDetector(void)
{
	LCUI_Widget list;
	LCUI_Widget task_training = LCUIWidget_New("taskitem");
	LCUI_Widget task_detection = LCUIWidget_New("taskitem");

	SelectWidget(list, ID_VIEW_DETECTOR_TASKS);
	SelectWidget(view.view, ID_VIEW_DETECTOR_SETTING);
	TaskItem_SetIcon(task_detection, "image-search-outline");
	TaskItem_SetNameKey(task_detection, KEY_DETECTOR_DETECTION_TITLE);
	TaskItem_SetTextKey(task_detection, KEY_DETECTOR_DETECTION_TEXT);
	TaskItem_SetIcon(task_training, "brain");
	TaskItem_SetNameKey(task_training, KEY_DETECTOR_TRAINING_TITLE);
	TaskItem_SetTextKey(task_training, KEY_DETECTOR_TRAINING_TEXT);
	Widget_BindEvent(task_detection, "start", OnStartDetect, NULL, NULL);
	Widget_BindEvent(task_detection, "stop", OnStopDetect, NULL, NULL);
	Widget_Append(list, task_detection);
	Widget_Append(list, task_training);
	view.detection.view = task_detection;
	view.training.view = task_training;
	InitModelsDropdown();
}
