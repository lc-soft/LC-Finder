/* ***************************************************************************
 * file_storage.c -- File related operating interface, based on file storage
 * service.
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
 * file_storage.c -- 基于文件存储服务而实现的文件相关操作接口
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

#include <stdio.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/graph.h>
#include "build.h"
#include "bridge.h"
#include "file_storage.h"

enum HandlerDataType {
	HANDLER_ON_OPEN,
	HANDLER_ON_GET_PROPS
};

typedef struct HandlerDataPackRec_ {
	int type;
	union {
		void( *on_get_props )(FileProperties*, void*);
		void( *on_open )(FileProperties*, FileStream, void*);
	};
	void *data;
} HandlerDataPackRec, *HandlerDataPack;

static struct FileStorageModule {
	FileClient client;
	LCUI_BOOL client_active;
} self;

int FileStorage_Init( void )
{
	int ret;
	LOG( "[file storage] init...\n" );
/* 非 UWP 应用则运行线程版的本地文件服务 */
#ifndef PLATFORM_WIN32_PC_APP
	FileService_Init();
	FileService_RunAsync();
#endif
	self.client_active = FALSE;
	self.client = FileClient_Create();
	ret = FileClient_Connect( self.client );
	if( ret == 0 ) {
		self.client_active = TRUE;
		FileClient_RunAsync( self.client );
	} else {
		LOG( "[file storage] error, code: %d\n", ret );
	}
	return ret;
}

void FileStorage_Exit( void )
{
	FileClient_Close( self.client );
#ifndef PLATFORM_WIN32_PC_APP
	FileService_Close();
#endif
}

int FileStorage_Open( const wchar_t *filename,
		      void( *callback )(FileProperties*, FileStream, void*),
		      void *data )
{
	return -1;
}

static void OnResponse( FileResponse *response, void *data )
{
	HandlerDataPack pack = data;
	switch( pack->type ) {
	case HANDLER_ON_OPEN:
	case HANDLER_ON_GET_PROPS:
		if( response->status != RESPONSE_STATUS_OK ) {
			pack->on_get_props( NULL, pack->data );
			break;
		}
		pack->on_get_props( &response->file, pack->data );
		break;
	default: break;
	}
	free( pack );
}

int FileStorage_GetProperties( const wchar_t *filename,
			       void( *callback )(FileProperties*, void*),
			       void *data )
{
	HandlerDataPack pack;
	FileRequestHandler handler;
	FileRequest request = { 0 };
	if( !self.client_active ) {
		return -1;
	}
	pack = NEW( HandlerDataPackRec, 1 );
	pack->type = HANDLER_ON_GET_PROPS;
	pack->on_get_props = callback;
	pack->data = data;
	request.method = REQUEST_METHOD_HEAD;
	wcsncpy( request.path, filename, 255 );
	handler.callback = OnResponse;
	handler.data = pack;
	FileClient_SendRequest( self.client, &request, &handler );
	return 0;
}
