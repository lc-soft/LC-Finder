/* ***************************************************************************
 * bridge.cpp -- a bridge, provides a cross-platform implementation for some
 * interfaces.
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
 * bridge.cpp -- 桥梁，为某些功能提供跨平台实现.
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

#include "pch.h"
#include "finder.h"
#include <sys/stat.h>

using namespace concurrency;
using namespace Platform;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::Foundation::Collections;
using namespace Windows::ApplicationModel;

#define FutureAccessList AccessCache::StorageApplicationPermissions::FutureAccessList

int GetAppDataFolderW( wchar_t *buf, int max_len )
{
	StorageFolder^ folder = ApplicationData::Current->LocalFolder;
	wcsncpy( buf, folder->Path->Data(), max_len );
	return 0;
}

int GetAppInstalledLocationW( wchar_t *buf, int max_len )
{
	StorageFolder^ folder = Package::Current->InstalledLocation;
	wcsncpy( buf, folder->Path->Data(), max_len );
	return 0;
}

static void TestGetFile( const wchar_t *wpath )
{
	String ^path = ref new String( wpath );
	create_task( StorageFile::GetFileFromPathAsync( path ) ).then( []( task<StorageFile^> fileTask ) {
		StorageFile^ file = nullptr;
		try {
			file = fileTask.get();
		} catch( Exception^ e ) {
			return file;
		}
		return file;
	} ).then( [](StorageFile^ file) {
		if( !file ) {
			return create_task( []() -> int {
				return -ENOMEM;
			} );
		}
		// TODO: Put code to handle the file when it is opened successfully.

		auto t = create_task( file->GetBasicPropertiesAsync() ).then( []( FileProperties::BasicProperties ^props ) {
			// TODO: Put code to handle the properties when it is get successfully.
			LOG( "mtime: %llu\n", props->DateModified.UniversalTime );
			return 0;
		} );
		if( true/* this file is image */ ) {
			t = t.then( [file](int ret) {
				return file->Properties->GetImagePropertiesAsync();
			} ).then( [file]( task<FileProperties::ImageProperties^> t ) {
				FileProperties::ImageProperties ^props;
				try {
					props = t.get();
					LOG( "image size: %d,%d\n", props->Width, props->Height );
				} catch( Exception^ e ) {

				}
				// TODO: Put code to handle the properties when it is get successfully.
				return 0;
			} );
		}
		return t;
	} ).then( []( int ret ) {
		LOG( "return: %d\n", ret );
	} );
}

void SelectFolderAsyncW( void( *callback )(const wchar_t*, const wchar_t*) )
{
	FolderPicker^ folderPicker = ref new FolderPicker();
	folderPicker->SuggestedStartLocation = PickerLocationId::Desktop;
	folderPicker->FileTypeFilter->Append( ".png" );
	folderPicker->FileTypeFilter->Append( ".bmp" );
	folderPicker->FileTypeFilter->Append( ".jpg" );
	folderPicker->FileTypeFilter->Append( ".jpeg" );
	create_task( folderPicker->PickSingleFolderAsync() )
		.then([callback]( StorageFolder^ folder ) {
		if( !folder ) {
			return;
		}
		auto token = FutureAccessList->Add( folder );
		//callback( folder->Path->Data(), token->Data() );
		TestGetFile( L"F:\\我的文件\\图片收藏\\测试-005\\001.png" );
	} );
}
/*
static void ScanImageFilesInFolder( StorageFolder ^folder,
				    FileHandlerAsync handler,
				    void (*callback)(void*), void *data )
{
	if( !folder ) {
		return;
	}
	auto options = ref new  Windows::Storage::Search::QueryOptions();
	options->FileTypeFilter->Append( ".png" );
	options->FileTypeFilter->Append( ".jpg" );
	options->FileTypeFilter->Append( ".jpeg" );
	options->FileTypeFilter->Append( ".bmp" );
	auto query = folder->CreateFileQueryWithOptions( options );
	auto task = create_task( query->GetFilesAsync() );
	task.then( [handler, callback, data]( IVectorView<StorageFile^>^ files ) {
		for( unsigned int i = 0; i < files->Size; i++ ) {
			StorageFile^ file = files->GetAt( i );
			FileIO::ReadBufferAsync( file );
			if( handler( data, file->Path->Data() ) != 0 ) {
				break;
			}
		}
		callback( data );
	} );
}

void ScanImageFilesAsyncW( const wchar_t *wpath, const wchar_t *wtoken,
			   FileHandlerAsync handler, void( *callback )(void*),
			   void *data )
{
	Platform::String ^token = ref new Platform::String( wtoken );
	auto task = create_task( FutureAccessList->GetFolderAsync( token ) );
	task.then( [handler, callback, data]( StorageFolder ^folder ) {
		ScanImageFilesInFolder( folder, handler, callback, data );
	} );
}
*/

void OpenUriW( const wchar_t *uristr )
{
	auto str = ref new Platform::String( uristr );
	auto uri = ref new Uri( str );
	Launcher::LaunchUriAsync( uri );
}

void OpenFileManagerW( const wchar_t *filepath )
{

}

int MoveFileToTrashW( const wchar_t *filepath )
{
	return -1;
}

int MoveFileToTrash( const char *filepath )
{
	return -1;
}
