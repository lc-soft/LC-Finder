#ifdef LCFINDER_THUMBVIEW_C

static int AutoLoader_OnUpdate(AutoLoader ctx)
{
	LCUI_Widget w;
	LinkedList list;
	LinkedListNode *node;
	LCUI_WidgetEventRec e = { 0 };
	int count = 0;
	float top, bottom;

	/* 若未启用滚动加载，或滚动层的高度过低，则本次不处理滚动加载 */
	if (!ctx->enabled || ctx->scrolllayer->height < 64) {
		return 0;
	}
	e.type = ctx->event_id;
	e.cancel_bubble = TRUE;
	bottom = top = (float)ctx->top;
	bottom += ctx->scrolllayer->parent->box.padding.height;
	if (!ctx->top_child) {
		node = ctx->scrolllayer->children.head.next;
		if (node) {
			ctx->top_child = node->data;
		}
	}
	if (!ctx->top_child) {
		return 0;
	}
	if (ctx->top_child->box.border.y > bottom) {
		node = &ctx->top_child->node;
		ctx->top_child = NULL;
		while (node) {
			w = node->data;
			if (w && w->state == LCUI_WSTATE_NORMAL) {
				if (w->box.border.y < bottom) {
					ctx->top_child = w;
					break;
				}
			}
			node = node->prev;
		}
		if (!ctx->top_child) {
			return 0;
		}
	}
	LinkedList_Init(&list);
	node = &ctx->top_child->node;
	while (node) {
		w = node->data;
		if (w->box.border.y > bottom) {
			break;
		}
		if (w->box.border.y + w->box.border.height >= top) {
			LinkedList_Append(&list, w);
			++count;
		}
		node = node->next;
	}
	for (LinkedList_Each(node, &list)) {
		w = node->data;
		Widget_TriggerEvent(w, &e, &count);
	}
	LinkedList_Clear(&list, NULL);
	return count;
}

static void AutoLoader_OnDelayUpdate(void *arg)
{
	AutoLoader ctx = arg;
	AutoLoader_OnUpdate(ctx);
	if (ctx->need_update) {
		ctx->timer =
		    LCUITimer_Set(SCROLLLOADING_DELAY,
				  AutoLoader_OnDelayUpdate, ctx, FALSE);
		ctx->need_update = FALSE;
	} else {
		ctx->timer = -1;
		ctx->is_delaying = FALSE;
	}
	DEBUG_MSG("ctx->top: %d\n", ctx->top);
}

static void AutoLoader_Update(AutoLoader ctx)
{
	assert(ctx != NULL);
	ctx->need_update = TRUE;
	if (ctx->is_delaying && ctx->timer > 0) {
		return;
	}
	ctx->timer = LCUITimer_Set(SCROLLLOADING_DELAY,
				   AutoLoader_OnDelayUpdate, ctx, FALSE);
	ctx->is_delaying = TRUE;
}

static void AutoLoader_OnScroll(LCUI_Widget w, LCUI_WidgetEvent e, void *arg)
{
	float *scroll_pos = arg;
	AutoLoader ctx = e->data;
	ctx->top = *scroll_pos;
	AutoLoader_Update(ctx);
}

/** 新建一个滚动加载功能实例 */
static AutoLoader AutoLoader_New(LCUI_Widget scrolllayer)
{
	AutoLoader ctx = NEW(AutoLoaderRec, 1);
	ctx->top = 0;
	ctx->timer = -1;
	ctx->enabled = TRUE;
	ctx->top_child = NULL;
	ctx->need_update = FALSE;
	ctx->is_delaying = FALSE;
	ctx->scrolllayer = scrolllayer;
	ctx->event_id = self.event_scrollload;
	Widget_BindEvent(scrolllayer, "scroll", AutoLoader_OnScroll, ctx,
			 NULL);
	return ctx;
}

/** 删除滚动加载功能实例 */
static void AutoLoader_Delete(AutoLoader ctx)
{
	ctx->top = 0;
	ctx->top_child = NULL;
	Widget_UnbindEvent(ctx->scrolllayer, "scroll", AutoLoader_OnScroll);
	free(ctx);
}

/** 重置滚动加载 */
static void AutoLoader_Reset(AutoLoader ctx)
{
	ctx->top = 0;
	ctx->top_child = NULL;
}

/** 启用/禁用滚动加载 */
static void AutoLoader_Enable(AutoLoader ctx, LCUI_BOOL enable)
{
	ctx->enabled = enable;
}

#endif
