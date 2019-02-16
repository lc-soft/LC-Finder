#ifdef LCFINDER_THUMBVIEW_C

static void ThumbWorker_Run(void *arg);

static LCUI_BOOL ThumbWorker_RemoveTask(ThumbWorker worker, LCUI_Widget target)
{
	LinkedListNode *node;

	if (worker->tasks.length < 1) {
		return FALSE;
	}
	for (LinkedList_Each(node, &worker->tasks)) {
		if (node->data != target) {
			continue;
		}
		LinkedList_DeleteNode(&worker->tasks, node);
		return TRUE;
	}
	return FALSE;
}

static void ThumbWorker_OnThumbLoadDone(ThumbLoader loader)
{
	ThumbWorker worker = loader->data;

	ThumbLoader_Destroy(loader);
	worker->active = FALSE;
	worker->loader = NULL;
	ThumbWorker_Run(worker);
}

static LCUI_BOOL ThumbWorker_ProcessTask(ThumbWorker worker)
{
	LCUI_Graph *thumb;
	LCUI_Widget target;

	ThumbLoader loader;
	ThumbViewItem item;
	LinkedListNode *node;

	worker->active = FALSE;
	node = LinkedList_GetNode(&worker->tasks, 0);
	assert(node && node->data);
	target = node->data;
	item = Widget_GetData(target, self.item);
	LinkedList_Delete(&worker->tasks, 0);
	thumb = ThumbLinker_Link(item->view->linker, item->path, target);
	DEBUG_MSG("cache[%p]: load thumb: %s, cached: %d\n", view->cache,
		  item->path, thumb ? 1 : 0);
	if (thumb) {
		if (item->setthumb) {
			item->setthumb(target, thumb);
		}
		return FALSE;
	}
	loader = ThumbLoader_Create(item->view, target);
	if (!loader) {
		return FALSE;
	}
	worker->loader = loader;
	worker->active = TRUE;
	ThumbLoader_SetCallback(loader, ThumbWorker_OnThumbLoadDone, worker);
	ThumbLoader_Start(loader);
	return TRUE;
}

static void ThumbWorker_Run(void *arg)
{
	ThumbWorker worker = arg;
	if (worker->active) {
		return;
	}
	while (worker->tasks.length > 0 && !ThumbWorker_ProcessTask(worker))
		;
}

static void ThumbWorker_Activate(ThumbWorker worker)
{
	if (worker->timer) {
		LCUITimer_Free(worker->timer);
	}
	worker->timer = LCUI_SetTimeout(100, ThumbWorker_Run, worker);
}

static void ThumbWorker_Reset(ThumbWorker worker)
{
	if (worker->loader) {
		ThumbLoader_Stop(worker->loader);
		worker->loader = FALSE;
	}
	worker->active = FALSE;
	if (worker->timer) {
		LCUITimer_Free(worker->timer);
	}
	worker->timer = 0;
	LinkedList_Clear(&worker->tasks, NULL);
}

static void ThumbWorker_Init(ThumbWorker worker)
{
	worker->timer = 0;
	worker->loader = NULL;
	worker->active = FALSE;
	LinkedList_Init(&worker->tasks);
}

static void ThumbWorker_AddTask(ThumbWorker worker, LCUI_Widget target)
{
	LinkedList *tasks = &worker->tasks;
	/* 如果待处理的任务数量超过最大限制，则移除最后一个任务 */
	if (tasks->length >= THUMB_TASK_MAX) {
		LinkedListNode *node = LinkedList_GetNodeAtTail(tasks, 0);
		DEBUG_MSG("remove old task\n");
		LinkedList_Unlink(tasks, node);
	}
	LinkedList_Insert(tasks, 0, target);
	ThumbWorker_Activate(worker);
}

#endif
