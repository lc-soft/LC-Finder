/* ***************************************************************************
 * file_service.c -- file service
 *
 * Copyright (C) 2016 by Liu Chao <lc-soft@live.cn>
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
 * 版权所有 (C) 2016 归属于 刘超 <lc-soft@live.cn>
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

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/thread.h>
#include <stdio.h>

enum FileResponseStatus {
	FS_STATUS_OK = 200,
	FS_STATUS_BAD_REQUEST = 400,
	FS_STATUS_FORBIDDEN = 403,
	FS_STATUS_NOT_FOUND = 404
};

enum FileType {
	FILE_TYPE_ARCHIVE,
	FILE_TYPE_DIRECTORY
};

enum FileRequestMethod {
	FS_METHOD_HEAD,
	FS_METHOD_GET,
	FS_METHOD_POST,
	FS_METHOD_PUT,
	FS_METHOD_DELETE
};

typedef struct FileProperties_ {
	int type;
	size_t size;
	time_t ctime;
	time_t mtime;
} FileProperties;

typedef struct FileStream_ {
	LCUI_BOOL active;
	LCUI_BOOL closed;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	LinkedList data;
} FileStream;

typedef struct FileRequest_ {
	int method;
	char path[256];
	FileProperties file;
	FileStream *stream;
} FileRequest;

typedef struct FileResponse_ {
	int status;
	FileProperties file;
	FileStream *stream;
} FileResponse;

typedef struct FileRequestHandler_ {
	void( *callback )(FileResponse*, void *);
	void *data;
} FileRequestHandler;

enum FileStreamChunkType {
	DATA_CHUNK_REQUEST,
	DATA_CHUNK_RESPONSE,
	DATA_CHUNK_BUFFER,
	DATA_CHUNK_END
};

typedef struct FileStreamChunk_ {
	int type;
	union {
		FileRequest request;
		FileResponse response;
		char *data;
	};
	size_t cur;
	size_t size;
} FileStreamChunk;

typedef struct ConnectionRecord_ {
	FileStream streams[2];
	LinkedListNode node;
} ConnectionRecord;

typedef struct Connection_ {
	LCUI_BOOL closed;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	LCUI_Thread thread;
	FileStream *input;
	FileStream *output;
} Connection;

typedef struct FileClientTask_ {
	FileRequest request;
	FileRequestHandler handler;
} FileClientTask;

typedef struct FileClient_ {
	LCUI_BOOL active;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	LCUI_Thread thread;
	Connection *connection;
	LinkedList tasks;
} FileClient;

static struct FileService {
	LCUI_BOOL active;
	LCUI_Thread thread;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
	int backlog;
	LinkedList requests;
	LinkedList connections;
} service;

void FileStream_Init( FileStream *stream )
{
	stream->closed = FALSE;
	stream->active = TRUE;
	LinkedList_Init( &stream->data );
	LCUICond_Init( &stream->cond );
	LCUIMutex_Init( &stream->mutex );
}

void FileStream_Close( FileStream *stream )
{
	if( stream->active ) {
		LCUIMutex_Lock( &stream->mutex );
		stream->closed = TRUE;
		LCUICond_Signal( &stream->cond );
		LCUIMutex_Lock( &stream->mutex );
	}
}

LCUI_BOOL FileStream_Useable( FileStream *stream )
{
	if( stream->active ) {
		if( stream->data.length > 0 ) {
			return TRUE;
		}
		if( stream->closed ) {
			return FALSE;
		}
	}
	return TRUE;
}

void FileStream_Destroy( FileStream *stream )
{
	if( !stream->active ) {
		return;
	}
	stream->active = FALSE;
	LinkedList_Clear( &stream->data, free );
	LCUIMutex_Destroy( &stream->mutex );
	LCUICond_Destroy( &stream->cond );
}

size_t FileStream_ReadChunk( FileStream *stream, FileStreamChunk *chunk )
{
	LinkedListNode *node;
	if( !FileStream_Useable( stream ) ) {
		FileStream_Destroy( stream );
		return 0;
	}
	LCUIMutex_Lock( &stream->mutex );
	while( stream->data.length < 1 && !stream->closed ) {
		LCUICond_Wait( &stream->cond, &stream->mutex );
	}
	if( stream->data.length > 0 ) {
		node = LinkedList_GetNode( &stream->data, 0 );
		LinkedList_Unlink( &stream->data, node );
		*chunk = *((FileStreamChunk*)node->data);
		LinkedListNode_Delete( node );
		LCUIMutex_Unlock( &stream->mutex );
		return 1;
	}
	LCUIMutex_Unlock( &stream->mutex );
	return 0;
}

size_t FileStream_WriteChunk( FileStream *stream, FileStreamChunk *chunk )
{
	FileStreamChunk *buf;
	if( !FileStream_Useable( stream ) ) {
		FileStream_Destroy( stream );
		return -1;
	}
	LCUIMutex_Lock( &stream->mutex );
	while( !stream->closed ) {
		LCUICond_Wait( &stream->cond, &stream->mutex );
	}
	buf = NEW( FileStreamChunk, 1 );
	*buf = *chunk;
	buf->cur = 0;
	if( chunk->type == DATA_CHUNK_REQUEST ) {
		buf->size = sizeof( buf->request );
	} else {
		buf->size = sizeof( buf->response );
	}
	LinkedList_Append( &stream->data, buf );
	LCUICond_Signal( &stream->cond );
	LCUIMutex_Unlock( &stream->mutex );
	return 1;
}

size_t FileStream_Read( FileStream *stream, char *buf,
			size_t size, size_t count )
{
	FileStreamChunk *chunk;
	LinkedListNode *node;
	size_t read_count = 0, cur = 0;
	if( !FileStream_Useable( stream ) ) {
		FileStream_Destroy( stream );
		return 0;
	}
	while( 1 ) {
		size_t n, read_size;
		LCUIMutex_Lock( &stream->mutex );
		while( stream->data.length < 1 && !stream->closed ) {
			LCUICond_Wait( &stream->cond, &stream->mutex );
		}
		if( stream->data.length < 1 ) {
			LCUIMutex_Unlock( &stream->mutex );
			return read_count;
		}
		node = LinkedList_GetNode( &stream->data, 0 );
		chunk = node->data;
		if( chunk->type != DATA_CHUNK_BUFFER ) {
			LCUIMutex_Unlock( &stream->mutex );
			break;
		}
		n = (chunk->size - chunk->cur) / size;
		if( n < count ) {
			LinkedList_Unlink( &stream->data, node );
			LinkedListNode_Delete( node );
			read_count += n;
			count -= n;
		} else {
			read_count += count;
			count = 0;
		}
		read_size = size * n;
		memcpy( buf + cur, chunk->data + chunk->cur, read_size );
		chunk->cur += read_size;
		cur += read_size;
		LCUIMutex_Unlock( &stream->mutex );
	}
	return read_count;
}

size_t FileStream_Write( FileStream *stream, char *buf,
			 size_t size, size_t count )
{
	FileStreamChunk *chunk;
	if( !FileStream_Useable( stream ) ) {
		FileStream_Destroy( stream );
		return 0;
	}
	LCUIMutex_Lock( &stream->mutex );
	while( !stream->closed ) {
		LCUICond_Wait( &stream->cond, &stream->mutex );
	}
	chunk = NEW( FileStreamChunk, 1 );
	chunk->cur = 0;
	chunk->size = count * size;
	chunk->type = DATA_CHUNK_BUFFER;
	chunk->data = malloc( chunk->size );
	memcpy(chunk->data, buf, chunk->size );
	LinkedList_Append( &stream->data, chunk );
	LCUICond_Signal( &stream->cond );
	LCUIMutex_Unlock( &stream->mutex );
	return count;
}

Connection *Connection_Create( void )
{
	Connection *conn;
	conn = NEW( Connection, 1 );
	conn->closed = TRUE;
	LCUICond_Init( &conn->cond );
	LCUIMutex_Init( &conn->mutex );
	LCUIMutex_Lock( &service.mutex );
	return conn;
}

size_t Connection_Read( Connection *conn, char *buf, 
			size_t size, size_t count )
{
	if( conn->closed ) {
		return -1;
	}
	return FileStream_Read( conn->input, buf, size, count );
}

size_t Connection_Write( Connection *conn, char *buf,
			 size_t size, size_t count )
{
	if( conn->closed ) {
		return -1;
	}
	return FileStream_Write( conn->output, buf, size, count );
}

int Connection_ReadChunk( Connection *conn, FileStreamChunk *chunk )
{
	if( conn->closed ) {
		return -1;
	}
	return FileStream_ReadChunk( conn->input, chunk );
}

int Connection_WriteChunk( Connection *conn, FileStreamChunk *chunk )
{
	if( conn->closed ) {
		return -1;
	}
	return FileStream_WriteChunk( conn->output, chunk );
}

void Connection_Close( Connection *conn )
{
	LCUIMutex_Lock( &conn->output->mutex );
	conn->closed = TRUE;
	LCUICond_Signal( &conn->output->cond );
	LCUIMutex_Unlock( &conn->output->mutex );
}

void Connection_Destroy( Connection *conn )
{

}

void Service_HandleRequest( FileRequest *request )
{
	switch( request->method ) {
	case FS_METHOD_HEAD:
	case FS_METHOD_POST:
	case FS_METHOD_GET:
	case FS_METHOD_DELETE:
	case FS_METHOD_PUT:
	default: break;
	}
}

void Service_Handler( void *arg )
{
	int n;
	FileStreamChunk chunk;
	Connection *conn = arg;
	while( 1 ) {
		n = Connection_ReadChunk( conn, &chunk );
		if( n == -1 ) {
			break;
		} else if( n == 0 ) {
			continue;
		}
		chunk.request.stream = conn->input;
		Service_HandleRequest( &chunk.request );
	}
	Connection_Destroy( conn );
	LCUIThread_Exit( NULL );
}

int Service_Listen( int backlog )
{
	LCUIMutex_Lock( &service.mutex );
	service.backlog = backlog;
	while( service.active && service.requests.length == 0 ) {
		LCUICond_Wait( &service.cond, &service.mutex );
	}
	LCUIMutex_Unlock( &service.mutex );
	return service.requests.length;
}

Connection *Service_Accept( void )
{
	LinkedListNode *node;
	ConnectionRecord *conn;
	Connection *conn_client, *conn_service;
	if( !service.active ) {
		return NULL;
	}
	LCUIMutex_Lock( &service.mutex );
	node = LinkedList_GetNode( &service.requests, 0 );
	LinkedList_Unlink( &service.requests, node );
	LCUICond_Signal( &service.cond );
	LCUIMutex_Unlock( &service.mutex );
	conn_client = node->data;
	conn = NEW( ConnectionRecord, 1 );
	conn_service = Connection_Create();
	FileStream_Init( &conn->streams[0] );
	FileStream_Init( &conn->streams[1] );
	LCUIMutex_Lock( &conn_client->mutex );
	conn->node.data = conn;
	conn_client->closed = FALSE;
	conn_service->closed = FALSE;
	conn_client->input = &conn->streams[0];
	conn_client->output = &conn->streams[1];
	conn_service->input = &conn->streams[1];
	conn_service->output = &conn->streams[0];
	LinkedList_AppendNode( &service.connections, &conn->node );
	LCUICond_Signal( &conn_client->cond );
	LCUIMutex_Unlock( &conn_client->mutex );
	return conn_service;
}

void Service_Run( void )
{
	Connection *conn;
	service.active = TRUE;
	while( service.active ) {
		if( Service_Listen( 5 ) == 0 ) {
			continue;
		}
		conn = Service_Accept();
		if( !conn ) {
			continue;
		}
		LCUIThread_Create( &conn->thread, Service_Handler, conn );
	}
}

static void Service_Thread( void *arg )
{
	Service_Run();
	LCUIThread_Exit( NULL );
}

void Service_RunAsnyc( void )
{
	LCUIThread_Create( &service.thread, Service_Thread, NULL );
}

void Service_Close( void )
{
	LCUIMutex_Lock( &service.mutex );
	service.active = FALSE;
	LCUICond_Signal( &service.cond );
	LCUIMutex_Unlock( &service.mutex );
}

void Service_Init( void )
{
	service.active = FALSE;
	LCUICond_Init( &service.cond );
	LCUIMutex_Init( &service.mutex );
}

int Connection_SendRequest( Connection *conn, 
			    const FileRequest *request )
{
	FileStreamChunk chunk;
	chunk.request = *request;
	chunk.type = DATA_CHUNK_REQUEST;
	if( Connection_WriteChunk( conn, &chunk ) > 0 ) {
		return 1;
	}
	return 0;
}

int Connection_ReceiveRequest( Connection *conn,
			       FileRequest *request )
{
	size_t ret;
	FileStreamChunk chunk;
	ret = Connection_ReadChunk( conn, &chunk );
	if( ret > 0 ) {
		if( chunk.type != DATA_CHUNK_REQUEST ) {
			return 0;
		}
		*request = chunk.request;
		return 1;
	}
	return 0;
}

int Connection_SendResponse( Connection *conn, 
			     const FileResponse *response )
{
	FileStreamChunk chunk;
	chunk.response = *response;
	chunk.type = DATA_CHUNK_RESPONSE;
	if( Connection_WriteChunk( conn, &chunk ) > 0 ) {
		return 1;
	}
	return 0;
}

int Connection_ReceiveResponse( Connection *conn, 
				FileResponse *response )
{
	size_t ret;
	FileStreamChunk chunk;
	ret = Connection_ReadChunk( conn, &chunk );
	if( ret > 0 ) {
		if( chunk.type != DATA_CHUNK_RESPONSE ) {
			return 0;
		}
		*response = chunk.response;
		return 1;
	}
	return 0;
}

Connection *Client_Connect( void )
{
	int timeout = 0;
	Connection *conn;
	LCUIMutex_Lock( &service.mutex );
	while( service.requests.length > service.backlog && timeout++ < 5 ) {
		LCUICond_TimedWait( &service.cond, &service.mutex, 1000 );
	}
	if( timeout >= 5 ) {
		LCUIMutex_Unlock( &service.mutex );
		return NULL;
	}
	conn = Connection_Create();
	LinkedList_Append( &service.requests, conn );
	LCUICond_Signal( &service.cond );
	LCUIMutex_Unlock( &service.mutex );
	for( timeout = 0; conn->closed && timeout < 5; ++timeout ) {
		LCUICond_TimedWait( &conn->cond, &conn->mutex, 1000 );
	}
	if( timeout >= 5 ) {
		Connection_Destroy( conn );
		LCUIMutex_Unlock( &service.mutex );
		return NULL;
	}
	return conn;
}

void Client_Init( FileClient *client )
{
	client->active = FALSE;
	LCUICond_Init( &client->cond );
	LCUIMutex_Init( &client->mutex );
	client->connection = Client_Connect();
}

void Client_Destroy( FileClient *client )
{
	
}

void Client_Run( FileClient *client )
{
	int n;
	LinkedListNode *node;
	FileClientTask *task;
	FileResponse response;
	Connection *conn = client->connection;

	client->active = TRUE;
	while( client->active ) {
		LCUIMutex_Lock( &client->mutex );
		while( client->tasks.length < 1 && client->active ) {
			LCUICond_Wait( &client->cond, &client->mutex );
		}
		node = LinkedList_GetNode( &client->tasks, 0 );
		LinkedList_Unlink( &client->tasks, node );
		LCUIMutex_Unlock( &client->mutex );
		task = node->data;
		n = Connection_SendRequest( conn, &task->request );
		if( n == 0 ) {
			continue;
		}

		while( client->active ) {
			n = Connection_ReceiveResponse( conn, &response );
			if( n == 0 ) {
				continue;
			}
		}
		response.stream = conn->input;
		task->handler.callback( &response, task->handler.data );
	}
}

void Client_Thread( void *arg )
{
	Client_Run( arg );
	LCUIThread_Exit( NULL );
}

void Client_Close( FileClient *client )
{
	client->active = FALSE;
}

void Client_RunAsync( FileClient *client )
{
	LCUIThread_Create( &client->thread, Client_Thread, client );
}

void Client_SendRequest( FileClient *client,
			 FileRequest *request,
			 FileRequestHandler handler )
{
	FileClientTask *task;
	task = NEW( FileClientTask, 1 );
	LCUIMutex_Lock( &client->mutex );
	LinkedList_Append( &client->tasks, task );
	LCUICond_Signal( &client->cond );
	LCUIMutex_Unlock( &client->mutex );
}
