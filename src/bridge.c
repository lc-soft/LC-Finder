/* ***************************************************************************
 * bridge.c -- a bridge, provides a cross-platform implementation for some
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
 * bridge.c -- 桥梁，为某些功能提供跨平台实现.
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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "finder.h"
#include <LCUI/display.h>
#include <LCUI/font/charset.h>
#include <LCUI/gui/widget.h>
#include "ui.h"
#include "dialog.h"

#ifdef PLATFORM_LINUX
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

/* clang-format off */

#define MAX_DIRPATH_LEN			2048
#define DIALOG_TITLE_ADD_DIR		L"添加源文件夹"
#define DIALOG_PLACEHOLDER_ADD_DIR	L"文件夹的位置"
#define APP_FOLDER_NAME			"lc-finder"
#define APP_OWNER_FOLDER_NAME		".lc-soft"
/* clang-format on */

static LCUI_BOOL CheckDir(const wchar_t *dirpath)
{
	if (wgetcharcount(dirpath, L":\"\'\\\n\r\t") > 0) {
		return FALSE;
	}
	if (wcslen(dirpath) >= MAX_DIRPATH_LEN) {
		return FALSE;
	}
	return TRUE;
}

int SelectFolder(char *dirpath, int max_len)
{
	wchar_t wdirpath[MAX_DIRPATH_LEN];
	LCUI_Widget window = LCUIWidget_GetById(ID_WINDOW_MAIN);
	if (0 != LCUIDialog_Prompt(window, DIALOG_TITLE_ADD_DIR,
				   DIALOG_PLACEHOLDER_ADD_DIR, NULL, wdirpath,
				   MAX_DIRPATH_LEN, CheckDir)) {
		return -1;
	}
	return LCUI_EncodeString(dirpath, wdirpath, max_len, ENCODING_UTF8);
}

int GetAppDataFolderW(wchar_t *buf, int max_len)
{
#ifdef PLATFORM_LINUX
	int status;
	struct passwd pwd;
	struct passwd *result;
	char *pwbuf, data_dir[PATH_LEN];
	size_t pwbufsize, len;

	pwbufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (pwbufsize == -1) {
		pwbufsize = 16384;
	}
	pwbuf = malloc(pwbufsize);
	if (pwbuf == NULL) {
		return -1;
	}
	getpwuid_r(getuid(), &pwd, pwbuf, pwbufsize, &result);
	if (result == NULL) {
		return -1;
	}
	pathjoin(data_dir, result->pw_dir, APP_OWNER_FOLDER_NAME);
	status = mkdir(data_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (status != 0 && errno != EEXIST) {
		return -1;
	}
	pathjoin(data_dir, data_dir, APP_FOLDER_NAME);
	status = mkdir(data_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (status != 0 && errno != EEXIST) {
		return -1;
	}
	len = LCUI_DecodeString(buf, data_dir, max_len, ENCODING_UTF8);
	buf[len] = 0;
	if (len > 0) {
		return 0;
	}
	return -1;
#else
	return -1;
#endif
}

int GetAppInstalledLocationW(wchar_t *buf, int max_len)
{
#ifdef PLATFORM_LINUX
	size_t i;
	char str[max_len + 1];
	size_t len = readlink("/proc/self/exe", str, max_len);

	if (len < 1) {
		return -1;
	}
	str[len] = 0;
	for (i = len; i > 1; --i) {
		if (str[i] == '/') {
			str[i] = 0;
			break;
		}
	}
	if (LCUI_DecodeString(buf, str, max_len, ENCODING_UTF8) > 0) {
		return 0;
	}
	return -1;
#else
	return -1;
#endif
}

void OpenUriW(const wchar_t *uri)
{
}

void OpenFileManagerW(const wchar_t *filepath)
{
}

int MoveFileToTrashW(const wchar_t *filepath)
{
	return -1;
}

int MoveFileToTrash(const char *filepath)
{
	return -1;
}

void SelectFolderAsyncW(void (*callback)(const wchar_t *, const wchar_t *))
{
	return;
}

void RemoveFolderAccessW(const wchar_t *token)
{
	return;
}

void LCFinder_InitLicense(void)
{
	finder.license.is_active = TRUE;
	finder.license.is_trial = FALSE;
}
