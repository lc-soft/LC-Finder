/* ***************************************************************************
* db.h -- the database operation set.
*
* Copyright (C) 2015 by Liu Chao <lc-soft@live.cn>
*
* This file is part of the LC-Finder project, and may only be used, modified,
* and distributed under the terms of the GPLv2.
*
* (GPLv2 is abbreviation of GNU General Public License Version 2)
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
* db.h -- 数据库操作集。
*
* 版权所有 (C) 2015 归属于 刘超 <lc-soft@live.cn>
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

#ifndef __DB_H__
#define __DB_H__

enum order {
	NONE,
	DESC,
	ASC
};

typedef struct DB_TagRec_ {
	int id;			/**< 标签标识号 */
	char *name;		/**< 标签名称 */
	int count;		/**< 与该标签关联的文件总数 */
} DB_TagRec, *DB_Tag;

typedef struct DB_DirRec_ {
	int id;			/**< 文件夹标识号 */
	char *path;		/**< 文件夹路径 */
} DB_DirRec, *DB_Dir;

typedef struct DB_FileRec_ {
	int id;			/**< 文件标识号 */
	int did;		/**< 文件夹标识号 */
	int score;		/**< 文件评分 */
	char *path;		/**< 文件路径 */
} DB_FileRec, *DB_File;

typedef struct DB_QueryRec_ {
	DB_Dir *dirs;			/**< 文件夹列表 */
	DB_Tag *tags;			/**< 标签列表 */
	int n_dirs;			/**< 文件夹数量 */
	int n_tags;			/**< 标签数量 */
	int offset;			/**< 从何处开始取数据记录 */
	int limit;			/**< 数据记录的最大数量 */
	enum order score;		/**< 按评分排序时使用的排序规则 */
	enum order create_time;		/**< 按创建时间排序时使用的排序规则 */
} DB_QueryRec, *DB_Query;		/**< 搜索规则定义 */

/** 初始化数据库模块 */
int DB_Init( void );

/** 获取添加一个文件夹 */
DB_Dir DB_AddDir( const char *dirpath );

/** 获取所有文件夹 */
int DB_GetDirs( DB_Dir **outlist );

/** 添加一个标签 */
DB_Tag DB_AddTag( const char *tagname );

/** 添加一个文件记录 */
void DB_AddFile( DB_Dir dir, const char *filepath, int create_time );

/** 获取全部标签记录 */
int DB_GetTags( DB_Tag **outlist );

/** 移除一个标签记录 */
void DBTag_Remove( DB_Tag tag );

/** 为文件移除一个标签 */
void DBFile_RemoveTag( DB_File file, DB_Tag tag );

/** 为文件添加一个标签 */
void DBFile_AddTag( DB_File file, DB_Tag tag );

/** 为文件评分 */
void DBFile_SetScore( DB_File file, int score );

/** 
 * 按照指定条件搜索文件 
 * @param[in] q 搜索条件
 * @param[out] outfiles 搜索到的文件列表
 * @param[out] total 符合搜索条件的文件总数
 * @return 文件列表的长度
 */
int DB_SearchFiles( const DB_Query q, DB_File **outfiles, int *total );

/** 刷新缓存，将数据写入至数据库 */
int DB_Flush( void );

#endif
