/* ***************************************************************************
 * build.h -- Configuration and macro definitions related to the build 
 * environment.
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
 * build.h -- 与构建环境相关的配置和宏定义
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

#ifndef LCFINDER_BUILD_H
#define LCFINDER_BUILD_H

#ifdef __cplusplus
#define LCFINDER_BEGIN_HEADER extern "C" {
#define LCFINDER_END_HEADER }
#else 
#define LCFINDER_BEGIN_HEADER
#define LCFINDER_END_HEADER
#endif

#define LCFINDER_NAME		L"LC's Finder"
#define LCFINDER_CONFIG_HEAD	"LCFinder Config Data"
#define LCFINDER_VER_MAJOR	0
#define LCFINDER_VER_MINOR	3
#define LCFINDER_VER_REVISION	0
#define LCFINDER_VER_TYPE	VERSION_BETA

#define LCFINDER_USE_UNQLITE

#ifdef _WIN32
#	define PLATFORM_WIN32
#	undef LCFINDER_USE_UNQLITE
#	define LCFINDER_USE_LEVELDB
// 如果需要编译成 Windows XP 系统上能跑的版本的话
//#define PLATFORM_WIN32_DESKTOP_XP
#	if (WINAPI_PARTITION_PC_APP == 1)
#		define PLATFORM_WIN32_PC_APP
#	else
#		define PLATFORM_WIN32_DESKTOP
#	endif
#else
#	define PLATFORM_LINUX
#endif

enum VersionType {
	VERSION_RELEASE,
	VERSION_RC,
	VERSION_BETA,
	VERSION_ALPHA
};

#endif
