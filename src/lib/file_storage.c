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

static struct FileStorageModule {
	FileClient client;
	int client_active;
} self;

int FileStorage_Init( void )
{
	int ret;
/* 非 UWP 应用则运行线程版的本地文件服务 */
#ifndef PLATFORM_WIN32_PC_APP
	FileService_Init();
	FileService_RunAsync();
#endif
	self.client_active = 0;
	self.client = FileClient_Create();
	ret = FileClient_Connect( self.client );
	if( ret == 0 ) {
		self.client_active = 1;
		FileClient_RunAsync( self.client );
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

int FileStorage_GetProperties( const wchar_t *filename,
			       void( *callback )(FileProperties*, void*),
			       void *data )
{
	return -1;
}
