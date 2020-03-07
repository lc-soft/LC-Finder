#ifdef LCFINDER_THUMBVIEW_C

static int LCUI_NextFrame(void (*callback)(void *), void *arg)
{
	return LCUI_SetTimeout(LCUI_MAX_FRAME_MSEC, callback, arg);
}

static float ComputeLayoutMaxWidth(LCUI_Widget parent)
{
	float width = max(THUMBVIEW_MIN_WIDTH, parent->box.content.width);
	return width + FILE_ITEM_MARGIN * 2;
}

/** 更新当前行内的各个缩略图尺寸 */
static void UpdateThumbRow(ThumbView view)
{
	LCUI_Widget item;
	ThumbViewItem data;
	LinkedListNode *node;
	float overflow_width, width, thumb_width, rest_width;
	if (view->layout.max_width < THUMBVIEW_MIN_WIDTH) {
		return;
	}
	/* 计算溢出的宽度 */
	overflow_width = view->layout.x - view->layout.max_width;
	DEBUG_MSG("overflow_width: %g, x: %g, max_width: %g\n", overflow_width,
		  view->layout.x, view->layout.max_width);
	/**
	 * 如果这一行缩略图的总宽度有溢出（超出最大宽度），则根据缩略图宽度占总宽度
	 * 的比例，分别缩减相应的宽度。
	 */
	if (overflow_width <= 0) {
		LinkedList_Clear(&view->layout.row, NULL);
		view->layout.x = 0;
		return;
	}
	rest_width = overflow_width;
	LCUIMutex_Lock(&view->layout.row_mutex);
	for (LinkedList_Each(node, &view->layout.row)) {
		int w, h;
		item = node->data;
		data = Widget_GetData(item, self.item);
		/* 确定合适的尺寸 */
		w = data->file->width > 0 ? data->file->width : 226;
		h = data->file->height > 0 ? data->file->height : 226;
		/* 按高的比例，计算合适的缩略图宽度 */
		thumb_width = 1.0f * PICTURE_FIXED_HEIGHT * w / h;
		/* 加上外间距占用的宽度 */
		thumb_width += FILE_ITEM_MARGIN * 2;
		/* 按照溢出后的宽度，计算当前图片需要分摊的溢出宽度 */
		width = overflow_width * thumb_width / view->layout.x;
		thumb_width -= FILE_ITEM_MARGIN * 2;
		/**
		 * 以上按比例分配的扣除宽度有误差，通常会少扣除几个像素的
		 * 宽度，这里用 rest_width 变量记录剩余待扣除的宽度，最
		 * 后一个缩略图的宽度直接减去 rest_width，以补全少扣除的
		 * 宽度。
		 */
		if (node->next) {
			rest_width -= width;
			width = thumb_width - width;
		} else {
			width = thumb_width - rest_width;
		}
		DEBUG_MSG("[%d] %g\n", item->index, width);
		Widget_SetStyle(item, key_width, width, px);
		Widget_UpdateStyle(item, FALSE);
	}
	LinkedList_Clear(&view->layout.row, NULL);
	LCUIMutex_Unlock(&view->layout.row_mutex);
	view->layout.x = 0;
}

/* 根据当前布局更新图片尺寸 */
static void UpdatePictureSize(LCUI_Widget item)
{
	float width, w = 226, h = 226;
	ThumbViewItem data = Widget_GetData(item, self.item);
	ThumbView view = data->view;

	if (data->file->width > 0) {
		w = (float)data->file->width;
		h = (float)data->file->height;
	}
	width = PICTURE_FIXED_HEIGHT * w / h;
	Widget_SetStyle(item, key_width, width, px);
	Widget_UpdateStyle(item, FALSE);
	view->layout.x += width + FILE_ITEM_MARGIN * 2;
	LinkedList_Append(&view->layout.row, item);
	/* 如果当前行图片总宽度超出最大宽度，则更新各个图片的宽度 */
	if (view->layout.x >= view->layout.max_width) {
		UpdateThumbRow(view);
	}
}

/* 根据当前布局更新文件夹尺寸 */
static void UpdateFolderSize(LCUI_Widget item)
{
	float width;
	ThumbViewItem data = Widget_GetData(item, self.item);
	ThumbView view = data->view;

	++view->layout.folder_count;
	if (view->layout.max_width < THUMBVIEW_MIN_WIDTH) {
		Widget_UnsetStyle(item, key_width);
		Widget_AddClass(item, "full-width");
		return;
	} else {
		Widget_RemoveClass(item, "full-width");
	}

	width = view->layout.max_width / view->layout.folders_per_row;
	width -= FILE_ITEM_MARGIN * 2;
	Widget_SetStyle(item, key_width, width, px);
	Widget_UpdateStyle(item, FALSE);
}

/** 更新用于布局的游标，以确保能够从正确的位置开始排列部件 */
static void ThumbView_UpdateLayoutCursor(LCUI_Widget w)
{
	LCUI_Widget child, prev;
	ThumbView view = Widget_GetData(w, self.main);
	if (view->layout.current) {
		return;
	}
	if (!view->layout.start) {
		if (w->children.length > 0) {
			view->layout.current = w->children.head.next->data;
		}
		return;
	}
	child = view->layout.start;
	prev = Widget_GetPrev(child);
	while (prev) {
		if (prev->computed_style.position == SV_ABSOLUTE ||
		    prev->computed_style.display == SV_NONE) {
			prev = Widget_GetPrev(prev);
			continue;
		}
		if ((child->x == 0 && child->y > prev->y) ||
		    prev->computed_style.display == SV_BLOCK) {
			break;
		}
		child = prev;
		prev = Widget_GetPrev(prev);
	}
	view->layout.start = child;
	view->layout.current = child;
}

/** 直接执行布局更新操作 */
static int ThumbView_OnUpdateLayout(LCUI_Widget w, int limit)
{
	int count = 0;
	LCUI_Widget child;
	ThumbView view = Widget_GetData(w, self.main);
	ThumbView_UpdateLayoutCursor(w);
	child = view->layout.current;
	for (; child && --limit >= 0; child = Widget_GetNext(child)) {
		ThumbViewItem item;
		view->layout.count += 1;
		item = Widget_GetData(child, self.item);
		if (!child->computed_style.visible) {
			++limit;
			continue;
		}
		if (Widget_CheckType(child, "thumbviewitem")) {
			++count;
			view->layout.current = child;
			item->updatesize(child);
			continue;
		}
		UpdateThumbRow(view);
		++limit;
	}
	if (view->layout.current) {
		view->layout.current = Widget_GetNext(view->layout.current);
	}
	return count;
}

/** 在布局完成后 */
static void ThumbView_OnAfterLayout(LCUI_Widget w, LCUI_WidgetEvent e,
				    void *arg)
{
	ThumbView view = Widget_GetData(w, self.main);
	AutoLoader_Update(view->loader);
	Widget_UnbindEvent(w, "afterlayout", ThumbView_OnAfterLayout);
}

static void ThumbView_OnLayoutDone(void *arg)
{
	LCUI_Widget w = arg;
	ThumbView view = Widget_GetData(w, self.main);

	view->layout.timer = 0;
	view->layout.current = NULL;
	view->layout.is_running = FALSE;
	Widget_AddTask(w, LCUI_WTASK_REFLOW);
	Widget_BindEvent(w, "afterlayout", ThumbView_OnAfterLayout, NULL, NULL);
}

/** 执行缩略图列表的布局任务 */
static void ThumbView_UpdateLayout(LCUI_Widget w)
{
	int n;
	ThumbView view = Widget_GetData(w, self.main);
	LayoutContext ctx = &view->layout;

	if (!ctx->is_running) {
		return;
	}
	n = ThumbView_OnUpdateLayout(w, MAX_LAYOUT_TARGETS);
	if (n < MAX_LAYOUT_TARGETS) {
		UpdateThumbRow(view);
		ctx->timer = LCUI_NextFrame(ThumbView_OnLayoutDone, w);
		return;
	}
	ctx->timer = LCUI_NextFrame((FuncPtr)ThumbView_UpdateLayout, w);
}

/** 更新布局上下文，为接下来的子部件布局处理做准备 */
static void ThumbView_UpdateLayoutContext(LCUI_Widget w)
{
	ThumbView view = Widget_GetData(w, self.main);
	float max_width = ComputeLayoutMaxWidth(w->parent);

	view->layout.folders_per_row = (int)ceil(max_width / FOLDER_MAX_WIDTH);
	view->layout.max_width = max_width;
	Widget_SetStyle(w, key_width, max_width, px);
}

/** 延迟执行缩略图列表的布局操作 */
static void ThumbView_OnDelayLayout(void *arg)
{
	LCUI_Widget w = arg;
}

#endif
