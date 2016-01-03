/* ***************************************************************************
* db.c -- the database operation set.
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
* db.c -- 数据库操作集。
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"
#include "sqlite3.h"

#ifdef WIN32
#define strdup _strdup
#endif

static struct DB_Module {
	sqlite3 *db;
	char *sql_buffer;
} self;

static const char *sql_init = "\
create table if not exists dir (\
	id integer primary key autoincrement,\
	path text not null\
);\
create table if not exists file (\
	id integer primary key autoincrement,\
	did integer not null, \
	path text not null,\
	score integer default 0,\
	create_time int not null,\
	foreign key (did) references dir(id) on delete cascade\
);\
create table if not exists tag (\
	id integer primary key autoincrement,\
	name text\
);\
create table if not exists file_tag_relation (\
	fid integer not null,\
	tid integer not null,\
	foreign key (fid) references file(id) on delete cascade,\
	foreign key (tid) references tag(id) on delete cascade\
);";
static const char *sql_get_dir_list = "\
select id, path from dir order by path asc;\
";
static const char *sql_get_tag_list = "\
select t.id, t.name, count(ftr.tid) from tag t, file_tag_relation ftr\
where ftr.tid = t.id group by ftr.tid order by name asc;\
";
static const char *sql_get_dir_total = "select count(*) from dir;";
static const char *sql_get_tag_total = "select count(*) from tag;";
static const char *sql_add_dir = "insert into dir(path) values(\"%s\");";
static const char *sql_add_tag = "insert into tag(name) values(\"%s\");";
static const char *sql_add_file = "\
insert into file(did, path, create_time) values(%d, \"%s\", %d)\
";

int DB_Init( void )
{
	int ret;
	char *errmsg;
	printf( "[database] initializing database ...\n" );
	ret = sqlite3_open( "res/data.db", &self.db );
	if( ret != SQLITE_OK ) {
		printf("[database] open failed\n");
		return -1;
	}
	ret = sqlite3_exec( self.db, sql_init, NULL, NULL, &errmsg );
	if( ret != SQLITE_OK ) {
		printf( "[database] error: %s\n", errmsg );
		return -2;
	}
	self.sql_buffer = NULL;
	return 0;
}

int DB_AddDir( const char *dirpath )
{
	int ret;
	char *errmsg, sql[1024];
	sprintf( sql, sql_add_dir, dirpath );
	ret = sqlite3_exec( self.db, sql, NULL, NULL, &errmsg );
	if( ret != SQLITE_OK ) {
		printf( "[database] error: %s\n", errmsg );
		return -1;
	}
	return 0;
}

int DBDir_GetList( DB_Dir **outlist )
{
	DB_Dir *list, dir;
	sqlite3_stmt *stmt;
	int ret, i, total = 0;
	sqlite3_prepare_v2( self.db, sql_get_dir_total, -1, &stmt, NULL );
	ret = sqlite3_step( stmt );
	if( ret == SQLITE_ROW ) {
		total = sqlite3_column_int( stmt, 0 );
	}
	sqlite3_finalize( stmt );
	if( total == 0 ) {
		*outlist = NULL;
		return 0;
	}
	list = malloc( sizeof(DB_Dir) * (total + 1) );
	if( !list ) {
		return -1;
	}
	list[total] = NULL;
	sqlite3_prepare_v2( self.db, sql_get_dir_list, -1, &stmt, NULL );
	for( i = 0; i < total; ++i ) {
		ret = sqlite3_step( stmt );
		if( ret != SQLITE_ROW ) {
			list[i] = NULL;
			break;
		}
		dir = malloc( sizeof(DB_DirRec) );
		if( !dir ) {
			list[i] = NULL;
			break;
		}
		dir->id = sqlite3_column_int( stmt, 0 );
		dir->path = strdup( sqlite3_column_text( stmt, 1 ) );
		list[i] = dir;
	}
	sqlite3_finalize( stmt );
	*outlist = list;
	return i;
}

int DB_AddTag( const char *tagname )
{
	int ret;
	char *errmsg, sql[1024];
	sprintf( sql, sql_add_tag, tagname );
	ret = sqlite3_exec( self.db, sql, NULL, NULL, &errmsg );
	if( ret != SQLITE_OK ) {
		printf( "[database] error: %s\n", errmsg );
		return -1;
	}
	return 0;
}

int DBTag_GetList( DB_Tag **outlist )
{
	DB_Tag *list, tag;
	sqlite3_stmt *stmt;
	int ret, i, total = 0;
	sqlite3_prepare_v2( self.db, sql_get_tag_total, -1, &stmt, NULL );
	ret = sqlite3_step( stmt );
	if( ret == SQLITE_ROW ) {
		total = sqlite3_column_int( stmt, 0 );
	}
	sqlite3_finalize( stmt );
	if( total == 0 ) {
		*outlist = NULL;
		return 0;
	}
	list = malloc( sizeof( DB_Dir ) * (total + 1) );
	if( !list ) {
		return -1;
	}
	list[total] = NULL;
	sqlite3_prepare_v2( self.db, sql_get_tag_list, -1, &stmt, NULL );
	for( i = 0; i < total; ++i ) {
		ret = sqlite3_step( stmt );
		if( ret != SQLITE_ROW ) {
			list[i] = NULL;
			break;
		}
		tag = malloc( sizeof( DB_TagRec ) );
		if( !tag ) {
			list[i] = NULL;
			break;
		}
		tag->id = sqlite3_column_int( stmt, 0 );
		tag->name = strdup( sqlite3_column_text( stmt, 1 ) );
		tag->count = sqlite3_column_int( stmt, 2 );
		list[i] = tag;
	}
	sqlite3_finalize( stmt );
	*outlist = list;
	return i;
}

void DBTag_Remove( DB_Tag tag )
{

}

void DBFile_RemoveTag( DB_File file, const char *tagname )
{

}

void DBFile_AddTag( DB_File file, const char *tagname )
{

}

void DBFile_SetScore( DB_File file, int score )
{

}

int DB_Flush( void )
{
	int ret;
	char *errmsg, *sql;
	if( !self.sql_buffer ) {
		return 0;
	}
	sql = self.sql_buffer;
	self.sql_buffer = NULL;
	ret = sqlite3_exec( self.db, sql, NULL, NULL, &errmsg );
	if( ret != SQLITE_OK ) {
		printf( "[database] flush error: %s\n", errmsg );
		return -1;
	}
	free( sql );
	return 0;
}
