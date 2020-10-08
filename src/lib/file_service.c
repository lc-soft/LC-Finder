﻿/* ***************************************************************************
 * file_service.c -- file service
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
 * file_service.c -- 文件服务
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

#define LCFINDER_FILE_SERVICE_C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/util/charset.h>
#include <LCUI/thread.h>
#include <LCUI/graph.h>
#include <LCUI/image.h>
#include "build.h"
#include "bridge.h"
#include "common.h"
#include "file_service.h"

#ifdef _WIN32
#define _S_ISTYPE(mode, mask) (((mode)&_S_IFMT) == (mask))
#define S_ISDIR(mode) _S_ISTYPE((mode), _S_IFDIR)
#define S_ISREG(mode) _S_ISTYPE((mode), _S_IFREG)
#endif

#undef LOG
#define LOG DEBUG_MSG

typedef struct FileStreamRec_ {
	LCUI_BOOL closed;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	LinkedList data;        /**< 数据块列表 */
	FileStreamChunk *chunk; /**< 当前操作的数据块 */
} FileStreamRec;

typedef struct ConnectionHubRec_ {
	unsigned int id;
	FileStream streams[2];
	Connection client;
	Connection service;
	LinkedListNode node;
} ConnectionHubRec, *ConnectionHub;

typedef struct ConnectionRec_ {
	unsigned int id;
	LCUI_BOOL closed;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	LCUI_Thread thread;
	FileStream input;
	FileStream output;
} ConnectionRec;

typedef struct FileClientTask_ {
	FileRequest request;
	FileRequestHandler handler;
} FileClientTask;

typedef struct FileClientRec_ {
	LCUI_BOOL active;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	LCUI_Thread thread;
	Connection connection;
	LinkedList tasks;
} FileClientRec, *FileClient;

static struct FileService {
	LCUI_BOOL active;
	LCUI_Thread thread;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	size_t backlog;
	LinkedList requests;
	LinkedList connections;
} service;

void FileStreamChunk_Destroy(FileStreamChunk *chunk)
{
	switch (chunk->type) {
	case DATA_CHUNK_RESPONSE:
		if (chunk->response.file.image) {
			free(chunk->response.file.image);
		}
		break;
	case DATA_CHUNK_IMAGE:
		Graph_Free(&chunk->image);
		break;
	case DATA_CHUNK_THUMB:
		Graph_Free(&chunk->thumb);
		break;
	case DATA_CHUNK_BUFFER:
		free(chunk->data);
		break;
	case DATA_CHUNK_FILE:
		fclose(chunk->file);
		break;
	default:
		break;
	}
}

static void FileStreamChunk_Release(void *data)
{
	FileStreamChunk *chunk = data;
	FileStreamChunk_Destroy(chunk);
	free(chunk);
}

static void FileClientTask_Destroy(FileClientTask *task)
{
	free(task);
}

static void OnDestroyFileClientTask(void *data)
{
	FileClientTask_Destroy(data);
}

FileStream FileStream_Create(void)
{
	FileStream stream;
	stream = NEW(FileStreamRec, 1);
	stream->closed = FALSE;
	LinkedList_Init(&stream->data);
	LCUICond_Init(&stream->cond);
	LCUIMutex_Init(&stream->mutex);
	return stream;
}

void FileStream_Close(FileStream stream)
{
	LCUIMutex_Lock(&stream->mutex);
	stream->closed = TRUE;
	LCUICond_Signal(&stream->cond);
	LCUIMutex_Unlock(&stream->mutex);
}

void FileStream_Destroy(FileStream stream)
{
	LOG("destroy stream %p start\n", stream);
	FileStream_Close(stream);
	LinkedList_Clear(&stream->data, FileStreamChunk_Release);
	LCUIMutex_Destroy(&stream->mutex);
	LCUICond_Destroy(&stream->cond);
	LOG("destroy stream %p end\n", stream);
}

int FileStream_ReadChunk(FileStream stream, FileStreamChunk *chunk)
{
	LinkedListNode *node;
	FileStreamChunk *raw_chunk;

	if (stream->closed) {
		return -1;
	}
	LCUIMutex_Lock(&stream->mutex);
	LOG("waitting stream %p\n", stream);
	while (stream->data.length < 1 && !stream->closed) {
		LCUICond_Wait(&stream->cond, &stream->mutex);
	}
	LOG("stream %p, closed: %d, len: %lu\n", stream, stream->closed, stream->data.length);
	if (stream->data.length > 0) {
		node = LinkedList_GetNode(&stream->data, 0);
		LinkedList_Unlink(&stream->data, node);
		raw_chunk = node->data;
		*chunk = *raw_chunk;
		free(raw_chunk);
		LinkedListNode_Delete(node);
		LOG("stream %p, unlocking\n", stream);
		LCUIMutex_Unlock(&stream->mutex);
		LOG("stream %p, unlocked\n", stream);
		return 1;
	}
	LCUIMutex_Unlock(&stream->mutex);
	return 0;
}

int FileStream_WriteChunk(FileStream stream, FileStreamChunk *chunk)
{
	FileStreamChunk *buf;

	if (stream->closed) {
		return -1;
	}
	LCUIMutex_Lock(&stream->mutex);
	buf = NEW(FileStreamChunk, 1);
	*buf = *chunk;
	buf->cur = 0;
	if (chunk->type == DATA_CHUNK_REQUEST) {
		buf->size = sizeof(buf->request);
	} else {
		buf->size = sizeof(buf->response);
	}
	LinkedList_Append(&stream->data, buf);
	LCUICond_Signal(&stream->cond);
	LCUIMutex_Unlock(&stream->mutex);
	return 1;
}

size_t FileStream_Read(FileStream stream, char *buf, size_t size, size_t count)
{
	LinkedListNode *node;
	FileStreamChunk *chunk;
	size_t read_count = 0, cur = 0;

	if (stream->closed) {
		return 0;
	}
	while (1) {
		size_t n, read_size;
		if (stream->chunk) {
			chunk = stream->chunk;
		} else {
			LCUIMutex_Lock(&stream->mutex);
			while (stream->data.length < 1 && !stream->closed) {
				LCUICond_Wait(&stream->cond, &stream->mutex);
			}
			if (stream->data.length < 1) {
				LCUIMutex_Unlock(&stream->mutex);
				return read_count;
			}
			node = LinkedList_GetNode(&stream->data, 0);
			chunk = node->data;
			LinkedList_Unlink(&stream->data, node);
			LinkedListNode_Delete(node);
			LCUIMutex_Unlock(&stream->mutex);
			if (chunk->type != DATA_CHUNK_BUFFER ||
			    chunk->type != DATA_CHUNK_FILE) {
				FileStreamChunk_Release(stream->chunk);
				break;
			}
			stream->chunk = chunk;
		}
		if (chunk->type == DATA_CHUNK_FILE) {
			read_count = fread(buf, size, count, chunk->file);
			if (feof(chunk->file)) {
				FileStreamChunk_Release(stream->chunk);
				stream->chunk = NULL;
			}
			return read_count;
		}
		n = (chunk->size - chunk->cur) / size;
		if (n < count) {
			read_count += n;
			count -= n;
		} else {
			read_count += count;
			count = 0;
		}
		read_size = size * n;
		memcpy(buf + cur, chunk->data + chunk->cur, read_size);
		chunk->cur += read_size;
		cur += read_size;
		if (chunk->cur >= chunk->size - 1) {
			FileStreamChunk_Release(stream->chunk);
			stream->chunk = NULL;
		}
	}
	return read_count;
}

size_t FileStream_Write(FileStream stream, char *buf, size_t size, size_t count)
{
	FileStreamChunk *chunk;
	if (stream->closed) {
		return 0;
	}
	LCUIMutex_Lock(&stream->mutex);
	if (stream->closed) {
		return 0;
	}
	chunk = NEW(FileStreamChunk, 1);
	chunk->cur = 0;
	chunk->size = count * size;
	chunk->type = DATA_CHUNK_BUFFER;
	chunk->data = malloc(chunk->size);
	memcpy(chunk->data, buf, chunk->size);
	LinkedList_Append(&stream->data, chunk);
	LCUICond_Signal(&stream->cond);
	LCUIMutex_Unlock(&stream->mutex);
	return count;
}

char *FileStream_ReadLine(FileStream stream, char *buf, size_t size)
{
	char *p = buf;
	char *end = buf + size - 1;
	size_t count = 0;
	LinkedListNode *node;
	FileStreamChunk *chunk;

	if (stream->closed) {
		return NULL;
	}
	do {
		if (stream->chunk) {
			chunk = stream->chunk;
		} else {
			LCUIMutex_Lock(&stream->mutex);
			while (stream->data.length < 1 && !stream->closed) {
				LCUICond_Wait(&stream->cond, &stream->mutex);
			}
			if (stream->data.length < 1) {
				LCUIMutex_Unlock(&stream->mutex);
				return NULL;
			}
			node = LinkedList_GetNode(&stream->data, 0);
			chunk = node->data;
			LinkedList_Unlink(&stream->data, node);
			LinkedListNode_Delete(node);
			LCUIMutex_Unlock(&stream->mutex);
			if (chunk->type != DATA_CHUNK_BUFFER &&
			    chunk->type != DATA_CHUNK_FILE) {
				if (count == 0) {
					return NULL;
				}
				break;
			}
			stream->chunk = chunk;
		}
		if (chunk->type == DATA_CHUNK_FILE) {
			p = fgets(buf, (int)size, chunk->file);
			if (!p || feof(chunk->file)) {
				FileStreamChunk_Release(chunk);
				stream->chunk = NULL;
			}
			return p;
		}
		while (chunk->cur < chunk->size && p < end) {
			*p = *(chunk->data + chunk->cur);
			++chunk->cur;
			if (*p == '\n') {
				break;
			}
			p++;
		}
		if (chunk->cur >= chunk->size) {
			FileStreamChunk_Release(chunk);
			stream->chunk = NULL;
		}
		count += 1;
	} while (*p != '\n' && p < end);
	if (*p == '\n') {
		*(p + 1) = 0;
	} else {
		*p = 0;
	}
	return buf;
}

Connection Connection_Create(void)
{
	Connection conn;
	conn = NEW(ConnectionRec, 1);
	conn->closed = TRUE;
	conn->id = 0;
	LCUICond_Init(&conn->cond);
	LCUIMutex_Init(&conn->mutex);
	return conn;
}

size_t Connection_Read(Connection conn, char *buf, size_t size, size_t count)
{
	if (conn->closed) {
		return -1;
	}
	return FileStream_Read(conn->input, buf, size, count);
}

size_t Connection_Write(Connection conn, char *buf, size_t size, size_t count)
{
	if (conn->closed) {
		return -1;
	}
	return FileStream_Write(conn->output, buf, size, count);
}

int Connection_ReadChunk(Connection conn, FileStreamChunk *chunk)
{
	if (conn->closed) {
		return -1;
	}
	return FileStream_ReadChunk(conn->input, chunk);
}

int Connection_WriteChunk(Connection conn, FileStreamChunk *chunk)
{
	if (conn->closed) {
		return -1;
	}
	return FileStream_WriteChunk(conn->output, chunk);
}

void Connection_Close(Connection conn)
{
	LCUIMutex_Lock(&conn->output->mutex);
	conn->closed = TRUE;
	LCUICond_Signal(&conn->output->cond);
	LCUIMutex_Unlock(&conn->output->mutex);
}

void Connection_Destroy(Connection conn)
{
	LCUIMutex_Destroy(&conn->mutex);
	LCUICond_Destroy(&conn->cond);
	conn->input = NULL;
	conn->output = NULL;
	free(conn);
}

static void OnDestroyConnection(void *data)
{
	Connection_Destroy(data);
}

static int FileService_GetFileImageStatus(FileRequest *request,
					  FileStreamChunk *chunk)
{
	int width, height;
	FileResponse *response = &chunk->response;
#ifdef _WIN32
	char *path = EncodeANSI(request->path);
#else
	char *path = EncodeUTF8(request->path);
#endif
	if (LCUI_GetImageSize(path, &width, &height) == 0) {
		response->file.image = NEW(FileImageStatus, 1);
		response->file.image->width = width;
		response->file.image->height = height;
		free(path);
		return 0;
	}
	response->file.image = NULL;
	free(path);
	return -1;
}

static int GetStatusByErrorCode(int code)
{
	switch (abs(code)) {
	case 0:
		return RESPONSE_STATUS_OK;
	case EACCES:
		return RESPONSE_STATUS_FORBIDDEN;
	case ENOENT:
		return RESPONSE_STATUS_NOT_FOUND;
	case EINVAL:
	case EEXIST:
	case EMFILE:
	default:
		break;
	}
	return RESPONSE_STATUS_ERROR;
}

static int FileService_GetFileStatus(FileRequest *request,
				     FileStreamChunk *chunk)
{
	int ret;
	struct stat buf;
	FileResponse *response = &chunk->response;
	ret = wgetfilestat(request->path, &buf);
	if (ret == 0) {
		response->status = RESPONSE_STATUS_OK;
		response->file.ctime = buf.st_ctime;
		response->file.mtime = buf.st_mtime;
		response->file.size = buf.st_size;
		if (S_ISDIR(buf.st_mode)) {
			response->file.type = FILE_TYPE_DIRECTORY;
		} else {
			response->file.type = FILE_TYPE_ARCHIVE;
		}
		if (request->params.with_image_status) {
			FileService_GetFileImageStatus(request, chunk);
		}
		return 0;
	}
	response->status = GetStatusByErrorCode(ret);
	return ret;
}

static int FileService_RemoveFile(const wchar_t *path, FileResponse *response)
{
	int ret = -1;    // MoveFileToTrashW(path);
	response->status = GetStatusByErrorCode(ret);
	return ret;
}

static int FileService_GetFiles(Connection conn, FileRequest *request,
				FileStreamChunk *chunk)
{
	int ret;
	LCUI_Dir dir;
	LCUI_DirEntry *entry;
	char buf[PATH_LEN + 3];

	ret = LCUI_OpenDirW(request->path, &dir);
	chunk->response.status = GetStatusByErrorCode(ret);
	if (ret != 0) {
		return ret;
	}
	while (!conn->closed && (entry = LCUI_ReadDirW(&dir))) {
		size_t size;
		wchar_t *name = LCUI_GetFileNameW(entry);
		/* 忽略 . 和 .. 文件夹 */
		if (name[0] == '.') {
			if (name[1] == 0) {
				continue;
			}
			if (name[1] == '.' && name[2] == 0) {
				continue;
			}
		}
		switch (request->params.filter) {
		case FILE_FILTER_FILE:
			if (!LCUI_FileIsRegular(entry)) {
				continue;
			}
			if (!IsImageFile(name)) {
				continue;
			}
			buf[0] = '-';
			break;
		case FILE_FILTER_FOLDER:
			if (!LCUI_FileIsDirectory(entry)) {
				continue;
			}
			buf[0] = 'd';
			break;
		default:
			if (LCUI_FileIsRegular(entry)) {
				if (!IsImageFile(name)) {
					continue;
				}
				buf[0] = '-';
			} else if (LCUI_FileIsDirectory(entry)) {
				buf[0] = 'd';
			} else {
				continue;
			}
			break;
		}
		size = LCUI_EncodeUTF8String(buf + 1, name, PATH_LEN) + 1;
		buf[size++] = '\n';
		buf[size] = 0;
		Connection_Write(conn, buf, sizeof(char), size);
	}
	return 0;
}

static int FileService_GetFile(Connection conn, FileRequest *request,
			       FileStreamChunk *chunk)
{
	int ret;
	char *path;
	FILE *fp;
	LCUI_Graph img;
	LCUI_ImageReaderRec reader = { 0 };
	FileResponse *response = &chunk->response;
	FileRequestParams *params = &request->params;

	ret = FileService_GetFileStatus(request, chunk);
	if (response->status != RESPONSE_STATUS_OK) {
		return ret;
	}
	if (response->file.type == FILE_TYPE_DIRECTORY) {
		Connection_WriteChunk(conn, chunk);
		return FileService_GetFiles(conn, request, chunk);
	}
	Graph_Init(&img);
	path = EncodeANSI(request->path);
	LOG("[file service] load image: %s\n", path);
	fp = fopen(path, "rb");
	free(path);
	if (!fp) {
		response->status = RESPONSE_STATUS_NOT_FOUND;
		return -1;
	}
	do {
		LCUI_SetImageReaderForFile(&reader, fp);
		reader.fn_prog = request->params.progress;
		reader.prog_arg = request->params.progress_arg;
		ret = LCUI_InitImageReader(&reader);
		if (ret != 0) {
			LOG("[file service] cannot initialize image reader\n");
			break;
		}
		if (LCUI_SetImageReaderJump(&reader)) {
			LOG("[file service] cannot set jump point\n");
			break;
		}
		if (LCUI_ReadImageHeader(&reader) != 0) {
			LOG("[file service] cannot read image header\n");
			break;
		}
		response->file.image = NEW(FileImageStatus, 1);
		response->file.image->width = reader.header.width;
		response->file.image->height = reader.header.height;
		if (LCUI_ReadImage(&reader, &img) != 0) {
			free(response->file.image);
			response->file.image = NULL;
			break;
		}
		LCUI_DestroyImageReader(&reader);
		fclose(fp);
		LOG("[file service] load image success, size: (%d, %d)\n",
		    img.width, img.height);
		Connection_WriteChunk(conn, chunk);
		if (!params->get_thumbnail) {
			chunk->type = DATA_CHUNK_IMAGE;
			chunk->image = img;
			return 0;
		}
		Graph_Init(&chunk->thumb);
		if ((params->width > 0 && img.width > (int)params->width) ||
		    (params->height > 0 && img.height > (int)params->height)) {
			/* FIXME: 大图的缩小效果并不好，需要改进 */
			Graph_ZoomBilinear(&img, &chunk->thumb, TRUE,
					   params->width, params->height);
			Graph_Free(&img);
		} else {
			chunk->thumb = img;
		}
		chunk->type = DATA_CHUNK_THUMB;
		return 0;
	} while (0);
	LOG("[file service] load image failed\n");
	response->status = RESPONSE_STATUS_NOT_ACCEPTABLE;
	LCUI_DestroyImageReader(&reader);
	Graph_Free(&img);
	fclose(fp);
	return -1;
}

static void FileService_HandleRequest(Connection conn, FileRequest *request)
{
	FileStreamChunk chunk = { 0 };
	const wchar_t *path = request->path;

	chunk.type = DATA_CHUNK_RESPONSE;
	switch (request->method) {
	case REQUEST_METHOD_HEAD:
		FileService_GetFileStatus(request, &chunk);
		break;
	case REQUEST_METHOD_POST:
	case REQUEST_METHOD_GET:
		FileService_GetFile(conn, request, &chunk);
		break;
	case REQUEST_METHOD_DELETE:
		FileService_RemoveFile(path, &chunk.response);
		break;
	case REQUEST_METHOD_PUT:
		chunk.response.status = RESPONSE_STATUS_NOT_IMPLEMENTED;
		break;
	default:
		chunk.response.status = RESPONSE_STATUS_BAD_REQUEST;
		break;
	}
	Connection_WriteChunk(conn, &chunk);
	chunk.type = DATA_CHUNK_END;
	chunk.size = chunk.cur = 0;
	chunk.data = NULL;
	Connection_WriteChunk(conn, &chunk);
}

void FileService_Handler(void *arg)
{
	int n;
	FileStreamChunk chunk;
	Connection conn = arg;

	LOG("[file service][thread %d] started, connection: %d\n",
	    LCUIThread_SelfID(), conn->id);
	while (!conn->closed) {
		LOG("[file service][thread %d] waitting, connection: %d\n",
	    	LCUIThread_SelfID(), conn->id);
		n = Connection_ReadChunk(conn, &chunk);
		LOG("[file service][thread %d] readed, n: %d, connection: %d\n",
	    	LCUIThread_SelfID(), n, conn->id);
		if (n == -1) {
			break;
		} else if (n == 0) {
			continue;
		}
		LOG("[file service][thread %d] handle request, connection: %d\n",
	    	LCUIThread_SelfID(), n, conn->id);
		chunk.request.stream = conn->input;
		FileService_HandleRequest(conn, &chunk.request);
	}
	LOG("[file service][thread %d] stopped, connection: %d\n",
	    LCUIThread_SelfID(), conn->id);
	Connection_Destroy(conn);
	LCUIThread_Exit(NULL);
}

size_t FileService_Listen(int backlog)
{
	LCUIMutex_Lock(&service.mutex);
	service.backlog = backlog;
	while (service.active && service.requests.length < 1) {
		LCUICond_Wait(&service.cond, &service.mutex);
	}
	LCUIMutex_Unlock(&service.mutex);
	return service.requests.length;
}

ConnectionHub ConnectionHub_Create(void)
{
	static unsigned base_id = 0;
	ConnectionHub conn = NEW(ConnectionHubRec, 1);

	conn->streams[0] = FileStream_Create();
	conn->streams[1] = FileStream_Create();
	conn->node.data = conn;
	conn->id = ++base_id;
	return conn;
}

void ConnectionHub_Destroy(ConnectionHub conn)
{
	FileStream_Destroy(conn->streams[0]);
	FileStream_Destroy(conn->streams[1]);
	free(conn);
}

static void OnDestroyConnectionHub(void *data)
{
	ConnectionHub_Destroy(data);
}

Connection FileService_Accept(void)
{
	ConnectionHub conn;

	if (!service.active) {
		return NULL;
	}
	conn = ConnectionHub_Create();

	LCUIMutex_Lock(&service.mutex);
	conn->client = LinkedList_Get(&service.requests, 0);
	conn->service = Connection_Create();
	LinkedList_Delete(&service.requests, 0);

	LCUIMutex_Lock(&conn->client->mutex);
	conn->client->id = conn->id;
	conn->client->closed = FALSE;
	conn->client->input = conn->streams[0];
	conn->client->output = conn->streams[1];
	LCUICond_Signal(&conn->client->cond);
	LCUIMutex_Unlock(&conn->client->mutex);

	conn->service->id = conn->id;
	conn->service->closed = FALSE;
	conn->service->input = conn->streams[1];
	conn->service->output = conn->streams[0];

	LinkedList_AppendNode(&service.connections, &conn->node);
	LCUICond_Signal(&service.cond);
	LCUIMutex_Unlock(&service.mutex);
	return conn->service;
}

void FileService_Run(void)
{
	Connection conn;
	LCUIMutex_Lock(&service.mutex);
	service.active = TRUE;
	LCUICond_Signal(&service.cond);
	LCUIMutex_Unlock(&service.mutex);
	LOG("[file service] file service started\n");
	while (service.active) {
		LOG("[file service] listen...\n");
		if (FileService_Listen(5) == 0) {
			continue;
		}
		conn = FileService_Accept();
		LOG("[file service] accept connection %d\n", conn->id);
		if (!conn) {
			continue;
		}
		LCUIThread_Create(&conn->thread, FileService_Handler, conn);
	}
	LOG("[file service] file service stopped\n");
}

static void FileService_Thread(void *arg)
{
	FileService_Run();
	LCUIThread_Exit(NULL);
}

void FileService_RunAsync(void)
{
	LCUIThread_Create(&service.thread, FileService_Thread, NULL);
}

void FileService_Close(void)
{
	LCUIMutex_Lock(&service.mutex);
	service.active = FALSE;
	LCUICond_Signal(&service.cond);
	LCUIMutex_Unlock(&service.mutex);
	if (service.thread) {
		LCUIThread_Join(service.thread, NULL);
	}
	LinkedList_Clear(&service.requests, OnDestroyConnection);
	LinkedList_ClearData(&service.connections, OnDestroyConnectionHub);
}

void FileService_Init(void)
{
	service.backlog = 1;
	service.active = FALSE;
	LinkedList_Init(&service.connections);
	LinkedList_Init(&service.requests);
	LCUICond_Init(&service.cond);
	LCUIMutex_Init(&service.mutex);
}

int Connection_SendRequest(Connection conn, const FileRequest *request)
{
	FileStreamChunk chunk;
	chunk.request = *request;
	chunk.type = DATA_CHUNK_REQUEST;
	if (Connection_WriteChunk(conn, &chunk) > 0) {
		return 1;
	}
	return 0;
}

int Connection_ReceiveRequest(Connection conn, FileRequest *request)
{
	size_t ret;
	FileStreamChunk chunk;
	ret = Connection_ReadChunk(conn, &chunk);
	if (ret > 0) {
		if (chunk.type != DATA_CHUNK_REQUEST) {
			return 0;
		}
		*request = chunk.request;
		return 1;
	}
	return 0;
}

int Connection_SendResponse(Connection conn, const FileResponse *response)
{
	FileStreamChunk chunk;
	chunk.response = *response;
	chunk.type = DATA_CHUNK_RESPONSE;
	if (Connection_WriteChunk(conn, &chunk) > 0) {
		return 1;
	}
	return 0;
}

int Connection_ReceiveResponse(Connection conn, FileResponse *response)
{
	size_t ret;
	FileStreamChunk chunk;
	ret = Connection_ReadChunk(conn, &chunk);
	if (ret > 0) {
		if (chunk.type != DATA_CHUNK_RESPONSE) {
			return 0;
		}
		*response = chunk.response;
		return 1;
	}
	return 0;
}

FileClient FileClient_Create(void)
{
	FileClient client;
	client = NEW(FileClientRec, 1);
	client->thread = 0;
	client->active = FALSE;
	client->connection = NULL;
	LCUICond_Init(&client->cond);
	LCUIMutex_Init(&client->mutex);
	LinkedList_Init(&client->tasks);
	return client;
}

int FileClient_Connect(FileClient client)
{
	int timeout = 0;
	LinkedListNode *node;
	Connection conn;

	LOG("[file client] connecting file service...\n");
	LCUIMutex_Lock(&service.mutex);
	while (!service.active) {
		LOG("[file client] wait...\n");
		LCUICond_TimedWait(&service.cond, &service.mutex, 1000);
		if (timeout++ >= 5) {
			LCUIMutex_Unlock(&service.mutex);
			LOG("[file client] timeout\n");
			return -1;
		}
	}
	timeout = 0;
	while (service.active && service.requests.length > service.backlog) {
		LCUICond_TimedWait(&service.cond, &service.mutex, 1000);
		if (timeout++ >= 5) {
			LCUIMutex_Unlock(&service.mutex);
			LOG("[file client] timeout\n");
			return -1;
		}
	}
	if (!service.active) {
		LCUIMutex_Unlock(&service.mutex);
		LOG("[file client] service stopped\n");
		return -2;
	}
	conn = Connection_Create();
	node = LinkedList_Append(&service.requests, conn);
	LCUICond_Signal(&service.cond);
	LCUIMutex_Unlock(&service.mutex);
	LCUIMutex_Lock(&conn->mutex);
	LOG("[file client] waitting file service accept connection...\n");
	for (timeout = 0; conn->closed && timeout < 60; ++timeout) {
		LCUICond_TimedWait(&conn->cond, &conn->mutex, 1000);
	}
	LCUIMutex_Unlock(&conn->mutex);
	if (timeout >= 60 && conn->closed) {
		LinkedList_Unlink(&service.requests, node);
		Connection_Destroy(conn);
		LOG("[file client] timeout\n");
		return -1;
	}
	client->connection = conn;
	LOG("[file client][connection %d] created\n", conn->id);
	return 0;
}

void FileClient_Destroy(FileClient client)
{
	client->active = FALSE;
	LCUIThread_Join(client->thread, NULL);
	if (client->connection) {
		Connection_Destroy(client->connection);
	}
	LinkedList_Clear(&client->tasks, OnDestroyFileClientTask);
	LCUICond_Destroy(&client->cond);
	LCUIMutex_Destroy(&client->mutex);
	client->connection = NULL;
	client->thread = 0;
	free(client);
}

void FileClient_Run(FileClient client)
{
	int n;
	LinkedListNode *node;
	FileClientTask *task;
	FileResponse response;
	Connection conn = client->connection;

	LOG("[file client][%u] work started\n", client->thread);
	client->active = TRUE;
	while (client->active) {
		LCUIMutex_Lock(&client->mutex);
		while (client->tasks.length < 1 && client->active) {
			LCUICond_Wait(&client->cond, &client->mutex);
		}
		node = LinkedList_GetNode(&client->tasks, 0);
		LCUIMutex_Unlock(&client->mutex);
		if (!client->active) {
			break;
		}
		if (!node) {
			continue;
		}
		LinkedList_Unlink(&client->tasks, node);
		task = node->data;
		LOG("[file client] send request, "
		    "method: %d, path len: %lu\n",
		    task->request.method,
		    wcslen(task->request.path));
		n = Connection_SendRequest(conn, &task->request);
		if (n == 0) {
			continue;
		}
		while (client->active) {
			n = Connection_ReceiveResponse(conn, &response);
			if (n != 0) {
				break;
			}
		}
		if (!client->active) {
			break;
		}
		response.stream = conn->input;
		task->handler.callback(&response, task->handler.data);
		if (response.file.image) {
			free(response.file.image);
			response.file.image = NULL;
		}
		FileClientTask_Destroy(task);
		free(node);
	}
	LOG("[file client][%u] work stopped\n", client->thread);
	LCUIThread_Exit(NULL);
}

static void FileClient_Thread(void *arg)
{
	FileClient_Run(arg);
	FileClient_Destroy(arg);
	LCUIThread_Exit(NULL);
}

void FileClient_Close(FileClient client)
{
	LOG("[file client][%u] close...\n", client->thread);
	LCUIMutex_Lock(&client->mutex);
	client->active = FALSE;
	Connection_Close(client->connection);
	LCUICond_Signal(&client->cond);
	LCUIMutex_Unlock(&client->mutex);
	if (client->thread) {
		LOG("[file client][%u] waiting...\n", client->thread);
		LCUIThread_Join(client->thread, NULL);
		LOG("[file client][%u] closed!\n", client->thread);
	}
}

void FileClient_RunAsync(FileClient client)
{
	LCUIThread_Create(&client->thread, FileClient_Thread, client);
}

void FileClient_SendRequest(FileClient client, const FileRequest *request,
			    const FileRequestHandler *handler)
{
	FileClientTask *task;
	task = NEW(FileClientTask, 1);
	task->handler = *handler;
	task->request = *request;
	LCUIMutex_Lock(&client->mutex);
	LinkedList_Append(&client->tasks, task);
	LCUICond_Signal(&client->cond);
	LCUIMutex_Unlock(&client->mutex);
}
