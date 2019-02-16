#ifdef LCFINDER_THUMBVIEW_C

/** 获取文件夹缩略图文件路径 */
static int GetDirThumbFilePath(char *filepath, char *dirpath)
{
	int total;
	DB_File file;
	DB_Query query;
	DB_QueryTermsRec terms = { 0 };
	terms.dirpath = dirpath;
	terms.n_dirs = 1;
	terms.limit = 10;
	terms.for_tree = TRUE;
	terms.create_time = DESC;
	query = DB_NewQuery(&terms);
	total = DBQuery_GetTotalFiles(query);
	if (total > 0) {
		file = DBQuery_FetchFile(query);
		strcpy(filepath, file->path);
	}
	return total;
}

static void ThumbLoader_Destroy(ThumbLoader loader)
{
	loader->active = FALSE;
	if (loader->wfullpath) {
		free(loader->wfullpath);
	}
	LCUIMutex_Destroy(&loader->mutex);
	free(loader);
}

static void ThumbLoader_Callback(ThumbLoader loader)
{
	if (loader->callback) {
		loader->callback(loader);
	}
}

static void ThumbLoader_OnError(ThumbLoader loader)
{
	LCUI_Widget icon;
	ThumbViewItem item;

	LCUIMutex_Lock(&loader->mutex);
	if (!loader->active) {
		goto exit;
	}
	item = Widget_GetData(loader->target, self.item);
	item->loader = NULL;
	if (!item->is_valid || item->is_dir) {
		goto exit;
	}
	item->is_valid = FALSE;
	icon = LCUIWidget_New("textview");
	Widget_AddClass(icon, "tip icon icon-help");
	Widget_AddClass(icon, "floating center middle aligned");
	Widget_Append(item->cover, icon);
exit:
	LCUIMutex_Unlock(&loader->mutex);
	ThumbLoader_Callback(loader);
}

static void ThumbLoader_OnDone(ThumbLoader loader, ThumbData data,
			       FileStatus *status)
{
	LCUI_Graph *thumb;
	ThumbViewItem item;
	LCUIMutex_Lock(&loader->mutex);
	if (!loader->active || !loader->target) {
		LCUIMutex_Unlock(&loader->mutex);
		ThumbLoader_Callback(loader);
		return;
	}
	item = Widget_GetData(loader->target, self.item);
	if (!item->is_dir) {
		if (item->file->modify_time != (uint_t)status->mtime) {
			int ctime = (int)status->ctime;
			int mtime = (int)status->mtime;
			DBFile_SetTime(item->file, ctime, mtime);
		}
		if (data->origin_width > 0 && data->origin_height > 0 &&
		    (item->file->width != data->origin_width ||
		     item->file->height != data->origin_height)) {
			DBFile_SetSize(item->file, data->origin_width,
				       data->origin_height);
		}
	}
	item->loader = NULL;
	ThumbCache_Add(loader->view->cache, item->path, &data->graph);
	thumb =
	    ThumbLinker_Link(loader->view->linker, item->path, loader->target);
	if (thumb) {
		if (item->setthumb) {
			LCUI_PostSimpleTask(item->setthumb, loader->target,
					    thumb);
		}
		LCUIMutex_Unlock(&loader->mutex);
		ThumbLoader_Callback(loader);
		return;
	}
	LCUIMutex_Unlock(&loader->mutex);
	ThumbLoader_OnError(loader);
}

static void OnGetThumbnail(FileStatus *status, LCUI_Graph *thumb, void *data)
{
	ThumbDataRec tdata;
	ThumbLoader loader = data;
	LCUIMutex_Lock(&loader->mutex);
	if (!loader->active || !loader->target) {
		LCUIMutex_Unlock(&loader->mutex);
		ThumbLoader_Callback(loader);
		return;
	}
	LCUIMutex_Unlock(&loader->mutex);
	if (!status || !status->image || !thumb) {
		ThumbLoader_OnError(loader);
		return;
	}
	tdata.origin_width = status->image->width;
	tdata.origin_height = status->image->height;
	tdata.modify_time = (uint_t)status->mtime;
	tdata.graph = *thumb;
	ThumbDB_Save(loader->db, loader->path, &tdata);
	ThumbLoader_OnDone(loader, &tdata, status);
	/** 重置数据，避免被释放 */
	Graph_Init(thumb);
}

static void ThumbLoader_Load(ThumbLoader loader, FileStatus *status)
{
	int ret;
	ThumbDataRec tdata;
	ThumbViewItem item;
	LCUIMutex_Lock(&loader->mutex);
	DEBUG_MSG("start\n");
	if (!loader->active || !loader->target) {
		LCUIMutex_Unlock(&loader->mutex);
		ThumbLoader_Callback(loader);
		DEBUG_MSG("end\n");
		return;
	}
	ret = ThumbDB_Load(loader->db, loader->path, &tdata);
	item = Widget_GetData(loader->target, self.item);
	LCUIMutex_Unlock(&loader->mutex);
	DEBUG_MSG("load path: %s, ret: %d, is_dir: %d\n", loader->path, ret,
		  item->is_dir);
	if (ret == 0) {
		if (status && tdata.modify_time == status->mtime) {
			ThumbLoader_OnDone(loader, &tdata, status);
			return;
		}
	}
	if (item->is_dir) {
		FileStorage_GetThumbnail(loader->view->storage,
					 loader->wfullpath, FOLDER_MAX_WIDTH, 0,
					 OnGetThumbnail, loader);
		return;
	}
	FileStorage_GetThumbnail(loader->view->storage, loader->wfullpath, 0,
				 THUMB_MAX_WIDTH, OnGetThumbnail, loader);
}

static void OnGetFileStatus(FileStatus *status, void *data)
{
	if (!status) {
		ThumbLoader_OnError(data);
		return;
	}
	ThumbLoader_Load(data, status);
}

static ThumbLoader ThumbLoader_Create(ThumbView view, LCUI_Widget target)
{
	ThumbViewItem item;
	ThumbLoader loader;

	item = Widget_GetData(target, self.item);
	if (!item || item->loader) {
		return NULL;
	}
	loader = NEW(ThumbLoaderRec, 1);
	loader->view = view;
	loader->data = NULL;
	loader->active = TRUE;
	loader->target = target;
	loader->callback = NULL;
	LCUICond_Init(&loader->cond);
	LCUIMutex_Init(&loader->mutex);
	item->loader = loader;
	return loader;
}

static void ThumbLoader_SetCallback(ThumbLoader loader,
				    ThumbLoaderCallback callback, void *data)
{
	loader->callback = callback;
	loader->data = data;
}

/** 载入缩略图 */
static void ThumbLoader_Start(ThumbLoader loader)
{
	size_t len;
	DB_Dir dir;
	ThumbViewItem item;
	ThumbView view = loader->view;
	item = Widget_GetData(loader->target, self.item);
	dir = LCFinder_GetSourceDir(item->path);
	if (!dir) {
		ThumbLoader_OnError(loader);
		return;
	}
	loader->db = Dict_FetchValue(*view->dbs, dir->path);
	if (!loader->db) {
		ThumbLoader_OnError(loader);
		return;
	}
	len = strlen(dir->path);
	if (item->is_dir) {
		pathjoin(loader->fullpath, item->path, "");
		if (GetDirThumbFilePath(loader->fullpath, loader->fullpath) ==
		    0) {
			ThumbLoader_OnError(loader);
			return;
		}
		if (item->path[len] == PATH_SEP) {
			len += 1;
		}
		pathjoin(loader->path, item->path + len, DIR_COVER_THUMB);
	} else {
		pathjoin(loader->fullpath, item->path, "");
		if (item->path[len] == PATH_SEP) {
			len += 1;
		}
		pathjoin(loader->path, item->path + len, "");
	}
	loader->wfullpath = DecodeUTF8(loader->fullpath);
	FileStorage_GetStatus(loader->view->storage, loader->wfullpath, FALSE,
			      OnGetFileStatus, loader);
}

static void ThumbLoader_Stop(ThumbLoader loader)
{
	LCUIMutex_Lock(&loader->mutex);
	loader->active = FALSE;
	loader->target = NULL;
	LCUIMutex_Unlock(&loader->mutex);
}

#endif
