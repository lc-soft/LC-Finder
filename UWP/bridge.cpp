/* ***************************************************************************
 * bridge.cpp -- a bridge, provides a cross-platform implementation for some
 * interfaces.
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
 * bridge.cpp -- 桥梁，为某些功能提供跨平台实现.
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
using namespace Windows::ApplicationModel::Store;

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
		callback( folder->Path->Data(), token->Data() );
	} );
}

void RemoveFolderAccessW( const wchar_t *token )
{
	auto str = ref new Platform::String( token );
	FutureAccessList->Remove( str );
}

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
	// 当前版本暂不提供文件删除功能
	return -1;
}

static void OnLicenseChanged( void )
{
	LicenseInformation ^license = CurrentApp::LicenseInformation;
	finder.license.is_active = license->IsActive;
	finder.license.is_trial = license->IsTrial;
	LCFinder_TriggerEvent( EVENT_LICENSE_CHG, NULL );
}

void LCFinder_InitLicense( void )
{
	LicenseInformation ^license = CurrentApp::LicenseInformation;
	license->LicenseChanged += ref new LicenseChangedEventHandler( OnLicenseChanged );
	finder.license.is_active = license->IsActive;
	finder.license.is_trial = license->IsTrial;
}

int MoveFileToTrash( const char *filepath )
{
	return -1;
}
