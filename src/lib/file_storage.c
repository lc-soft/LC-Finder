/* ***************************************************************************
 * file_storage.c -- File related operating interface, based on file storage
 * service.
 *
 * Copyright (C) 2016-2017 by Liu Chao <lc-soft@live.cn>
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
 * file_storage.c -- 基于文件存储服务而实现的文件相关操作接口
 *
 * 版权所有 (C) 2016-2017 归属于 刘超 <lc-soft@live.cn>
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
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/graph.h>
#include "build.h"
#include "bridge.h"
#include "file_storage.h"

enum HandlerDataType {
	HANDLER_ON_GET_FILE,
	HANDLER_ON_GET_IMAGE,
	HANDLER_ON_GET_THUMB,
	HANDLER_ON_GET_PROPS
};

typedef struct HandlerDataPackRec_ {
	int type;
	union {
		HandlerOnGetFile on_get_file;
		HandlerOnGetThumbnail on_get_thumb;
		HandlerOnGetStatus on_get_status;
		HandlerOnGetImage on_get_image;
	};
	HandlerOnGetProgress on_get_prog;
	void *data;
} HandlerDataPackRec, *HandlerDataPack;

typedef struct FileStorageConnectionRec_ {
	int id;
	FileClient client;
	LCUI_BOOL active;
	LinkedListNode node;
} FileStorageConnectionRec, *FileStorageConnection;

static struct FileStorageModule {
	int base_id;
	LinkedList clients;
} self;

void FileStorage_Init( void )
{
	self.base_id = 1;
	FileService_Init();
	FileService_RunAsync();
	LinkedList_Init( &self.clients );
}

static FileStorageConnection FileStorage_GetConnection( int id )
{
	LinkedListNode *node;
	for( LinkedList_Each(node, &self.clients) ) {
		FileStorageConnection conn = node->data;
		if( conn && conn->id == id ) {
			return conn;
		}
	}
	return NULL;
}

int FileStorage_Connect( void )
{
	int ret;
	ASSIGN( conn, FileStorageConnection );
	conn->active = FALSE;
	conn->client = FileClient_Create();
	ret = FileClient_Connect( conn->client );
	if( ret == 0 ) {
		conn->active = TRUE;
		FileClient_RunAsync( conn->client );
	} else {
		free( conn );
		LOG( "[file storage] connect failed, code: %d\n", ret );
		return -1;
	}
	conn->node.data = conn;
	conn->id = self.base_id++;
	LinkedList_AppendNode( &self.clients, &conn->node );
	return conn->id;
}

void FileStorage_Close( int id )
{
	FileStorageConnection conn = FileStorage_GetConnection( id );
	if( conn ) {
		FileClient_Close( conn->client );
		conn->client = NULL;
		conn->active = FALSE;
	}
}

void FileStorage_Exit( void )
{
	FileService_Close();
}

static void OnResponse( FileResponse *response, void *data )
{
	int n;
	FileStreamChunk chunk;
	HandlerDataPack pack = data;

	switch( pack->type ) {
	case HANDLER_ON_GET_FILE:
		if( response->status != RESPONSE_STATUS_OK ) {
			pack->on_get_file( NULL, NULL, pack->data );
			break;
		}
		pack->on_get_file( &response->file, 
				   response->stream, pack->data );
		break;
	case HANDLER_ON_GET_THUMB:
		if( response->status != RESPONSE_STATUS_OK ) {
			pack->on_get_thumb( NULL, NULL, pack->data );
			break;
		}
		n = FileStream_ReadChunk( response->stream, &chunk );
		if( n == 0 || chunk.type != DATA_CHUNK_THUMB ) {
			pack->on_get_thumb( NULL, NULL, pack->data );
			break;
		}
		pack->on_get_thumb( &response->file,
				    &chunk.thumb, pack->data );
		break;
	case HANDLER_ON_GET_IMAGE:
		if( response->status != RESPONSE_STATUS_OK ) {
			pack->on_get_image( NULL, pack->data );
			break;
		}
		n = FileStream_ReadChunk( response->stream, &chunk );
		if( n == 0 || chunk.type != DATA_CHUNK_IMAGE ) {
			pack->on_get_image( NULL, pack->data );
			break;
		}
		pack->on_get_image( &chunk.image, pack->data );
		break;
	case HANDLER_ON_GET_PROPS:
		if( response->status != RESPONSE_STATUS_OK ) {
			pack->on_get_status( NULL, pack->data );
			break;
		}
		pack->on_get_status( &response->file, pack->data );
		break;
	default: break;
	}
	FileStreamChunk_Destroy( &chunk );
	free( pack );
}

int FileStorage_GetFile( int conn_id, const wchar_t *filename,
			 HandlerOnGetFile callback, void *data )
{
	HandlerDataPack pack;
	FileRequestHandler handler;
	FileStorageConnection conn;
	FileRequest request = { 0 };

	conn = FileStorage_GetConnection( conn_id );
	if( !conn || !conn->active ) {
		return -1;
	}
	pack = NEW( HandlerDataPackRec, 1 );
	pack->type = HANDLER_ON_GET_FILE;
	pack->on_get_file = callback;
	pack->data = data;
	request.method = REQUEST_METHOD_GET;
	wcsncpy( request.path, filename, 255 );
	handler.callback = OnResponse;
	handler.data = pack;
	FileClient_SendRequest( conn->client, &request, &handler );
	return 0;
}

int FileStorage_GetFiles( int conn_id, const wchar_t *filename,
			  HandlerOnGetFile callback, void *data )
{
	HandlerDataPack pack;
	FileRequestHandler handler;
	FileStorageConnection conn;
	FileRequest request = { 0 };

	conn = FileStorage_GetConnection( conn_id );
	if( !conn || !conn->active ) {
		return -1;
	}
	pack = NEW( HandlerDataPackRec, 1 );
	pack->type = HANDLER_ON_GET_FILE;
	pack->on_get_file = callback;
	pack->data = data;
	request.method = REQUEST_METHOD_GET;
	request.params.filter = FILE_FILTER_FILE;
	wcsncpy( request.path, filename, 255 );
	handler.callback = OnResponse;
	handler.data = pack;
	FileClient_SendRequest( conn->client, &request, &handler );
	return 0;
}

int FileStorage_GetFolders( int conn_id, const wchar_t *filename,
			    HandlerOnGetFile callback, void *data )
{
	HandlerDataPack pack;
	FileRequestHandler handler;
	FileStorageConnection conn;
	FileRequest request = { 0 };

	conn = FileStorage_GetConnection( conn_id );
	if( !conn || !conn->active ) {
		return -1;
	}
	pack = NEW( HandlerDataPackRec, 1 );
	pack->type = HANDLER_ON_GET_FILE;
	pack->on_get_file = callback;
	pack->data = data;
	request.method = REQUEST_METHOD_GET;
	request.params.filter = FILE_FILTER_FOLDER;
	wcsncpy( request.path, filename, 255 );
	handler.callback = OnResponse;
	handler.data = pack;
	FileClient_SendRequest( conn->client, &request, &handler );
	return 0;
}

static void FileStorgage_OnGetProgress( void *data, float progress )
{
	HandlerDataPack pack = data;
	pack->on_get_prog( progress, pack->data );
}

int FileStorage_GetImage( int conn_id, const wchar_t *filename, 
			  HandlerOnGetImage callback, 
			  HandlerOnGetProgress progress,
			  void *data )
{
	HandlerDataPack pack;
	FileRequestHandler handler;
	FileStorageConnection conn;
	FileRequest request = { 0 };

	conn = FileStorage_GetConnection( conn_id );
	if( !conn || !conn->active ) {
		return -1;
	}
	pack = NEW( HandlerDataPackRec, 1 );
	pack->type = HANDLER_ON_GET_IMAGE;
	pack->on_get_image = callback;
	pack->on_get_prog = progress;
	pack->data = data;
	request.method = REQUEST_METHOD_GET;
	request.params.progress_arg = pack;
	request.params.progress = FileStorgage_OnGetProgress;
	wcsncpy( request.path, filename, 255 );
	handler.callback = OnResponse;
	handler.data = pack;
	FileClient_SendRequest( conn->client, &request, &handler );
	return 0;
}

int FileStorage_GetThumbnail( int conn_id, const wchar_t *filename,
			      int width, int height,
			      HandlerOnGetThumbnail callback, void *data )
{
	HandlerDataPack pack;
	FileRequestHandler handler;
	FileStorageConnection conn;
	FileRequest request = { 0 };

	conn = FileStorage_GetConnection( conn_id );
	if( !conn || !conn->active ) {
		return -1;
	}
	pack = NEW( HandlerDataPackRec, 1 );
	pack->type = HANDLER_ON_GET_THUMB;
	pack->on_get_thumb = callback;
	pack->data = data;
	request.method = REQUEST_METHOD_GET;
	request.params.get_thumbnail = TRUE;
	request.params.width = width;
	request.params.height = height;
	wcsncpy( request.path, filename, 255 );
	handler.callback = OnResponse;
	handler.data = pack;
	FileClient_SendRequest( conn->client, &request, &handler );
	return 0;
}

int FileStorage_GetStatus( int conn_id, const wchar_t *filename,
			   LCUI_BOOL with_extra,
			   HandlerOnGetStatus callback, void *data )
{
	HandlerDataPack pack;
	FileRequestHandler handler;
	FileStorageConnection conn;
	FileRequest request = { 0 };

	conn = FileStorage_GetConnection( conn_id );
	if( !conn || !conn->active ) {
		return -1;
	}
	pack = NEW( HandlerDataPackRec, 1 );
	pack->type = HANDLER_ON_GET_PROPS;
	pack->on_get_status = callback;
	pack->data = data;
	request.method = REQUEST_METHOD_HEAD;
	request.params.with_image_status = with_extra;
	wcsncpy( request.path, filename, 255 );
	handler.callback = OnResponse;
	handler.data = pack;
	FileClient_SendRequest( conn->client, &request, &handler );
	return 0;
}
