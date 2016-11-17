/* ***************************************************************************
 * selectfolder.cpp -- select a folder
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
 * selectfolder.c -- 文件夹选择功能，封装了各个系统下的文件夹选择对话框的实现
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

#include "finder.h"
#include <LCUI/display.h>
#include <LCUI/font/charset.h>
#include "selectfolder.h"

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>

int GetAppDataFolderW( wchar_t *buf, int max_len )
{
	HWND hwnd;
	LCUI_Surface s;
	s = LCUIDisplay_GetSurfaceOwner( NULL );
	hwnd = (HWND)Surface_GetHandle( s );
	return SHGetSpecialFolderPathW( hwnd, buf, CSIDL_LOCAL_APPDATA, 1 );
}

/* 如果需要兼容 XP 的话 */
#ifdef _WINXP

#define TEXT_SELECT_DIR	L"选择文件夹"

int SelectFolder( char *dirpath, int max_len )
{
	int len;
	HWND hwnd;
	BROWSEINFOW bi;
	LCUI_Surface s;
	LPITEMIDLIST iids;
	wchar_t wdirpath[PATH_LEN] = L"";
	s = LCUIDisplay_GetSurfaceOwner( NULL );
	hwnd = (HWND)Surface_GetHandle( s );
	memset( &bi, 0, sizeof( bi ) );
	bi.hwndOwner = hwnd;
	bi.pidlRoot = NULL;
	bi.lpszTitle = TEXT_SELECT_DIR;
	bi.ulFlags = BIF_NEWDIALOGSTYLE;
	iids = SHBrowseForFolder( &bi );
	if( !iids ) {
		return -1;
	}
	SHGetPathFromIDList( iids, wdirpath );
	len = wcslen( wdirpath );
	if( len <= 0 ) {
		return -1;
	}
	LCUI_EncodeString( dirpath, wdirpath, max_len, ENCODING_UTF8 );
	return 0;
}

#else

/* 适用于 Win 7 及以上的系统 */

int SelectFolder( char *dirpath, int max_len )
{
	PWSTR pszPath;
	DWORD dwOptions;
	IShellItem *pItem;
	IFileDialog *pFile;
	HRESULT hr = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED |
				     COINIT_DISABLE_OLE1DDE );
	if( FAILED( hr ) ) {
		return -1;
	}
	hr = CoCreateInstance( CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
			       IID_PPV_ARGS( &pFile ) );
	if( FAILED( hr ) ) {
		CoUninitialize();
		return -2;
	}
	do {
		hr = pFile->GetOptions( &dwOptions );
		if( FAILED( hr ) ) {
			break;
		}
		pFile->SetOptions( dwOptions | FOS_PICKFOLDERS );
		hr = pFile->Show( NULL );
		if( FAILED( hr ) ) {
			break;
		}
		hr = pFile->GetResult( &pItem );
		if( FAILED( hr ) ) {
			break;
		}
		hr = pItem->GetDisplayName( SIGDN_FILESYSPATH, &pszPath );
		if( FAILED( hr ) ) {
			break;
		}
		LCUI_EncodeString( dirpath, pszPath, max_len, ENCODING_UTF8 );
		CoTaskMemFree( pszPath );
	} while( 0 );
	pFile->Release();
	CoUninitialize();
	return hr;
}

#endif
#else

#include "ui.h"
#include <LCUI/gui/widget.h>
#include "dialog.h"

#define MAX_DIRPATH_LEN			2048
#define DIALOG_TITLE_ADD_DIR		L"添加源文件夹"
#define DIALOG_PLACEHOLDER_ADD_DIR	L"文件夹的位置"

static LCUI_BOOL CheckDir( const wchar_t *dirpath )
{
	if( wgetcharcount( dirpath, L":\"\'\\\n\r\t" ) > 0 ) {
		return FALSE;
	}
	if( wcslen( dirpath ) >= MAX_DIRPATH_LEN ) {
		return FALSE;
	}
	return TRUE;
}

int SelectFolder( char *dirpath, int max_len )
{
	wchar_t wdirpath[MAX_DIRPATH_LEN];
	LCUI_Widget window = LCUIWidget_GetById( ID_WINDOW_MAIN );
	if( 0 != LCUIDialog_Prompt( window, DIALOG_TITLE_ADD_DIR,
				    DIALOG_PLACEHOLDER_ADD_DIR, NULL,
				    wdirpath, MAX_DIRPATH_LEN, CheckDir ) ) {
		return -1;
	}
	return LCUI_EncodeString( dirpath, wdirpath, max_len, ENCODING_UTF8 );
}

int GetAppDataFolderW( wchar_t *buff, int max_len )
{
	// ...
	return 0;
}
#endif
