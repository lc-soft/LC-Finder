/* ***************************************************************************
 * file_search.h -- file indexing and searching.
 *
 * Copyright (C) 2015-2019 by Liu Chao <lc-soft@live.cn>
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
 * file_search.h -- 文件信息的索引与搜索。
 *
 * 版权所有 (C) 2015-2019 归属于 刘超 <lc-soft@live.cn>
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

#ifndef LCFINDER_FILE_SEARCH_H
#define LCFINDER_FILE_SEARCH_H

enum order { NONE, DESC, ASC };

typedef struct DB_TagRec_ {
	int id;     /**< 标签标识号 */
	char *name; /**< 标签名称 */
	int count;  /**< 与该标签关联的文件总数 */
} DB_TagRec, *DB_Tag;

typedef struct DB_DirRec_ {
	int id;      /**< 文件夹标识号 */
	char *path;  /**< 文件夹路径 */
	char *token; /**< 文件夹访问凭据 */
	int visible; /**< 是否可见 */
} DB_DirRec, *DB_Dir;

typedef struct DB_FileRec_ {
	int id;                   /**< 文件标识号 */
	int did;                  /**< 文件夹标识号 */
	int score;                /**< 文件评分 */
	char *path;               /**< 文件路径 */
	int width;                /**< 宽度 */
	int height;               /**< 高度 */
	unsigned int create_time; /**< 创建时间 */
	unsigned int modify_time; /**< 修改时间 */
} DB_FileRec, *DB_File;

/*< 搜索规则定义 */
typedef struct DB_QueryTermsRec_ {
	DB_Dir *dirs;  /**< 源文件夹列表 */
	DB_Tag *tags;  /**< 标签列表 */
	size_t n_dirs; /**< 文件夹数量 */
	size_t n_tags; /**< 标签数量 */
	size_t offset; /**< 从何处开始取数据记录 */
	size_t limit;  /**< 数据记录的最大数量 */
	int for_tree; /**< 是否搜索子级目录树，值为 0 时只搜索当前目录下的文件
		       */
	char *dirpath;          /**< 文件所在的目录路径 */
	enum order score;       /**< 按评分排序时使用的排序规则 */
	enum order create_time; /**< 按创建时间排序时使用的排序规则 */
	enum order modify_time; /**< 按修改时间排序时使用的排序规则 */
} DB_QueryTermsRec, *DB_QueryTerms;

#ifdef LCFINDER_FILE_SEARCH_C
typedef struct DB_QueryRec_ *DB_Query;
#else
typedef void *DB_Query;
#endif

/** 初始化数据库模块 */
int DB_Init(const char *dbpath);

void DB_Exit(void);

/** 添加一个文件夹 */
DB_Dir DB_AddDir(const char *dirpath, const char *token, int visible);

/** 删除一个文件夹 */
void DB_DeleteDir(DB_Dir dir);

/** 释放文件夹信息占用的资源 */
void DBDir_Release(DB_Dir dir);

/** 获取所有文件夹 */
int DB_GetDirs(DB_Dir **outlist);

/** 添加一个标签 */
DB_Tag DB_AddTag(const char *tagname);

/** 添加一个文件记录 */
void DB_AddFile(DB_Dir dir, const char *filepath, int ctime, int mtime);

/** 修改文件的时间信息 */
void DB_UpdateFileTime(DB_Dir dir, const char *filepath, int ctime, int mtime);

/** 删除一个文件记录 */
void DB_DeleteFile(const char *filepath);

/** 获取一个文件记录 */
DB_File DB_GetFile(const char *filepath);

/** 获取全部标签记录 */
size_t DB_GetTags(DB_Tag **outlist);

size_t DB_GetTagsOrderById(DB_Tag **outlist);

/** 为文件移除一个标签 */
int DBFile_RemoveTag(DB_File file, DB_Tag tag);

/** 为文件添加一个标签 */
int DBFile_AddTag(DB_File file, DB_Tag tag);

/** 获取文件拥有的标签列表 */
size_t DBFile_GetTags(DB_File file, DB_Tag **outtags);

/** 为文件评分 */
int DBFile_SetScore(DB_File file, int score);

/** 为文件设置尺寸 */
int DBFile_SetSize(DB_File file, int width, int height);

/** 为文件设置时间属性，包括创建时间、修改时间 */
int DBFile_SetTime(DB_File file, int ctime, int mtime);

/** 复制文件信息 */
DB_File DBFile_Dup(DB_File file);

/** 释放文件信息占用的资源 */
void DBFile_Release(DB_File file);

/** 复制标签信息 */
DB_Tag DBTag_Dup(DB_Tag tag);

/** 释放标签信息占用的资源 */
void DBTag_Release(DB_Tag tag);

/** 获取符合查询条件的文件总数 */
int DBQuery_GetTotalFiles(DB_Query query);

/** 从查询结果中获取下个文件 */
DB_File DBQuery_FetchFile(DB_Query query);

/** 新建一个查询实例 */
DB_Query DB_NewQuery(const DB_QueryTerms terms);

/** 删除一个查询实例 */
void DB_DeleteQuery(DB_Query query);

/** 事物开始 */
int DB_Begin(void);

/** 提交事务 */
int DB_Commit(void);

#endif
