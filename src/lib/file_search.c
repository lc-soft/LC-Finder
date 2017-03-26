/* ***************************************************************************
 * file_search.c -- file indexing and searching.
 *
 * Copyright (C) 2015-2017 by Liu Chao <lc-soft@live.cn>
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
 * file_search.c -- 文件信息的索引与搜索。
 *
 * 版权所有 (C) 2015-2017 归属于 刘超 <lc-soft@live.cn>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"
#define LCFINDER_FILE_SEARCH_C
#include "file_search.h"

#define SQL_BUF_SIZE 1024

#ifdef _WIN32
#define strdup _strdup
#endif

enum SQLCodeList {
	SQL_ADD_FILE,
	SQL_DEL_FILE,
	SQL_GET_FILE,
	SQL_GET_FILE_TAGS,
	SQL_ADD_FILE_TAG,
	SQL_DEL_FILE_TAG,
	SQL_SET_FILE_SIZE,
	SQL_SET_FILE_SCORE,
	SQL_SET_FILE_TIME,
	SQL_SET_FILE_TIME_BY_PATH,
	SQL_GET_DIR_TOTAL,
	SQL_GET_DIR_LIST,
	SQL_ADD_TAG,
	SQL_GET_TAG,
	SQL_ADD_DIR,
	SQL_GET_DIR,
	SQL_DEL_DIR,
	SQL_TOTAL
};

typedef struct DB_QueryRec_ {
	char sql_tables[128];
	char sql_terms[1024];
	char sql_orderby[128];
	char sql_groupby[128];
	char sql_having[128];
	char sql_limit[128];
	sqlite3_stmt *stmt;
} DB_QueryRec;

static struct DB_Module {
	sqlite3 *db;
	const char *sqls[SQL_TOTAL];
	sqlite3_stmt *stmts[SQL_TOTAL];
} self;

#define STATIC_STR static const char*

STATIC_STR sql_init = "\
PRAGMA foreign_keys=ON;\
CREATE TABLE IF NOT EXISTS dir (\
	id INTEGER PRIMARY KEY AUTOINCREMENT,\
	path TEXT NOT NULL, \
	visible INTEGER DEFAULT 1,\
	token TEXT DEFAULT NULL\
);\
CREATE TABLE IF NOT EXISTS file (\
	id INTEGER PRIMARY KEY AUTOINCREMENT,\
	did INTEGER NOT NULL, \
	path TEXT NOT NULL,\
	score INTEGER DEFAULT 0,\
	width INTEGER DEFAULT 0,\
	height INTEGER DEFAULT 0,\
	create_time INTEGER NOT NULL,\
	modify_time INTEGER NOT NULL,\
	FOREIGN KEY(did) REFERENCES dir(id) ON DELETE CASCADE\
);\
CREATE TABLE IF NOT EXISTS tag (\
	id INTEGER PRIMARY KEY AUTOINCREMENT,\
	name TEXT NOT NULL \
);\
CREATE TABLE IF NOT EXISTS file_tag_relation (\
	fid INTEGER NOT NULL,\
	tid INTEGER NOT NULL,\
	FOREIGN KEY (fid) REFERENCES file(id) ON DELETE CASCADE,\
	FOREIGN KEY (tid) REFERENCES tag(id) ON DELETE CASCADE,\
	UNIQUE(fid, tid)\
);";

STATIC_STR sql_get_dir_total = "SELECT COUNT(*) FROM dir;";
STATIC_STR sql_get_tag_total = "SELECT COUNT(*) FROM tag;";
STATIC_STR sql_del_dir = "DELETE FROM dir WHERE id = ?;";
STATIC_STR sql_add_tag = "INSERT INTO tag(name) VALUES(?);";
STATIC_STR sql_del_tag = "DELETE FROM tag WHERE id = %d;";
STATIC_STR sql_del_file = "DELETE FROM file WHERE path = ?;";
STATIC_STR sql_get_tag = "SELECT id FROM tag WHERE name = ?;";
STATIC_STR sql_file_set_score = "UPDATE file SET score = ? WHERE id = ?;";
STATIC_STR sql_count_files = "SELECT COUNT(*) FROM ";

STATIC_STR sql_add_dir = "\
INSERT INTO dir(path, token, visible) VALUES(?, ?, ?);";

STATIC_STR sql_get_dir = \
"SELECT id, path, token, visible FROM dir WHERE path = ?;";

STATIC_STR sql_get_dir_list = "\
SELECT id, path, token, visible FROM dir ORDER BY PATH ASC;";

STATIC_STR sql_get_tag_list = "\
SELECT t.id, t.name, COUNT(ftr.tid) FROM tag t, file_tag_relation ftr \
WHERE ftr.tid = t.id GROUP BY ftr.tid ORDER BY NAME ASC;";

STATIC_STR sql_file_set_size = "\
UPDATE file SET width = ?, height = ? WHERE id = ?;";

STATIC_STR sql_file_set_time = "\
UPDATE file SET create_time = ?, modify_time = ? WHERE id = ?;";

STATIC_STR sql_file_set_time_by_path = "\
UPDATE file SET create_time = ?, modify_time = ? \
WHERE did = ? AND path = ?;";

STATIC_STR sql_file_add_tag = "\
REPLACE INTO file_tag_relation(fid, tid) VALUES(?, ?);";

STATIC_STR sql_file_del_tag = "\
DELETE FROM file_tag_relation WHERE fid = ? AND tid = ?;";

STATIC_STR sql_add_file = "\
INSERT INTO file(did, path, create_time, modify_time) VALUES(?, ?, ?, ?);";

STATIC_STR sql_get_file = "\
SELECT f.id, f.did, f.score, f.path, f.width, f.height, f.create_time, \
f.modify_time FROM file f WHERE f.path = ?;";

STATIC_STR sql_get_file_tags = "\
SELECT t.id, t.name, count(*) FROM tag t, file_tag_relation ftr \
WHERE t.id = ftr.tid and ftr.fid = ? GROUP BY t.id ORDER BY count(*) ASC;";

STATIC_STR sql_search_files = "SELECT f.id, f.did, f.score, f.path, \
f.width, f.height, f.create_time, f.modify_time FROM file f ";


/** 检测目录的下一级文件列表中是否有指定文件 */
static int DirHasFile( const char *dirpath, const char *filepath )
{
	const char *p1, *p2;
	for( p1 = dirpath, p2 = filepath; *p1 && *p2; ++p1, ++p2 ) {
		if( *p1 != *p2 ) {
			break;
		}
	}
	/* 目录路径必须比文件路径短 */
	if( *p1 || !*p2 ) {
		return 0;
	}
	if( p1 != dirpath ) {
		/* 如果目录路径最后一个字符不是路径分隔符 */
		if( p1[-1] != '\\' && p1[-1] != '/' ) {
			if( *p2 != '\\' && *p2 != '/' ) {
				return 0;
			}
		}
	}
	/* 检测剩余这段文件路径是否包含路径分隔符 */
	while( *p2++ ) {
		if( *p2 == '\\' || *p2 == '/' ) {
			return 0;
		}
	}
	return 1;
}

static void sqlite3_hasfile( sqlite3_context *ctx, int argc, sqlite3_value **argv )
{
	const char *dirpath, *filepath;
	if( argc != 2 ) {
		return;
	}
	if( sqlite3_value_type( argv[0] ) != SQLITE_TEXT ) {
		return;
	}
	if( sqlite3_value_type( argv[1] ) != SQLITE_TEXT ) {
		return;
	}
	dirpath = sqlite3_value_text( argv[0] );
	filepath = sqlite3_value_text( argv[1] );
	sqlite3_result_int( ctx, DirHasFile( dirpath, filepath ) );
}

int DB_Init( const char *dbpath )
{
	int i, ret;
	char *errmsg;
	printf( "[database] init ...\n" );
	ret = sqlite3_open( dbpath, &self.db );
	if( ret != SQLITE_OK ) {
		printf("[database] open failed\n");
		return -1;
	}
	ret = sqlite3_exec( self.db, sql_init, NULL, NULL, &errmsg );
	if( ret != SQLITE_OK ) {
		printf( "[database] error: %s\n", errmsg );
		return -2;
	}
	sqlite3_create_function( self.db, "hasfile", 2, SQLITE_UTF8, NULL, 
				 sqlite3_hasfile, NULL, NULL );
	self.sqls[SQL_ADD_FILE] = sql_add_file;
	self.sqls[SQL_DEL_FILE] = sql_del_file;
	self.sqls[SQL_GET_FILE] = sql_get_file;
	self.sqls[SQL_ADD_DIR] = sql_add_dir;
	self.sqls[SQL_GET_DIR] = sql_get_dir;
	self.sqls[SQL_DEL_DIR] = sql_del_dir;
	self.sqls[SQL_ADD_TAG] = sql_add_tag;
	self.sqls[SQL_GET_TAG] = sql_get_tag;
	self.sqls[SQL_ADD_FILE_TAG] = sql_file_add_tag;
	self.sqls[SQL_DEL_FILE_TAG] = sql_file_del_tag;
	self.sqls[SQL_SET_FILE_SIZE] = sql_file_set_size;
	self.sqls[SQL_SET_FILE_SCORE] = sql_file_set_score;
	self.sqls[SQL_SET_FILE_TIME_BY_PATH] = sql_file_set_time_by_path;
	self.sqls[SQL_SET_FILE_TIME] = sql_file_set_time;
	self.sqls[SQL_GET_FILE_TAGS] = sql_get_file_tags;
	self.sqls[SQL_GET_DIR_LIST] = sql_get_dir_list;
	self.sqls[SQL_GET_DIR_TOTAL] = sql_get_dir_total;
	for( i = 0; i < SQL_TOTAL; ++i ) {
		sqlite3_stmt *stmt;
		const char *sql = self.sqls[i];
		ret = sqlite3_prepare_v2( self.db, sql, -1, &stmt, NULL );
		if( ret == SQLITE_OK ) {
			self.stmts[i] = stmt;
		} else {
			self.stmts[i] = NULL;
		}
		stmt = NULL;
	}
	printf( "[database] init done\n" );
	return 0;
}

void DB_Exit( void )
{
	int i;
	for( i = 0; i < SQL_TOTAL; ++i ) {
		sqlite3_finalize( self.stmts[i] );
		self.stmts[i] = NULL;
	}
}

static DB_Dir DB_LoadDir( sqlite3_stmt *stmt )
{
	DB_Dir dir;
	const unsigned char *text;
	dir = malloc( sizeof( DB_DirRec ) );
	dir->id = sqlite3_column_int( stmt, 0 );
	dir->path = strdup( sqlite3_column_text( stmt, 1 ) );
	dir->visible = sqlite3_column_int( stmt, 3 );
	text = sqlite3_column_text( stmt, 2 );
	if( text ) {
		dir->token = strdup( text );
	} else {
		dir->token = NULL;
	}
	return dir;
}

DB_Dir DB_AddDir( const char *dirpath, const char *token, int visible )
{
	int ret;
	DB_Dir dir;
	sqlite3_stmt *stmt;
	stmt = self.stmts[SQL_ADD_DIR];
	sqlite3_reset( stmt );
	sqlite3_bind_text( stmt, 1, dirpath, -1, NULL );
	sqlite3_bind_int( stmt, 3, visible ? 1 : 0 );
	if( token ) {
		sqlite3_bind_text( stmt, 2, token, -1, NULL );
	}
	ret = sqlite3_step( stmt );
	if( ret != SQLITE_DONE ) {
		printf( "[database] error: %s\n", dirpath );
		return NULL;
	}
	stmt = self.stmts[SQL_GET_DIR];
	sqlite3_reset( stmt );
	sqlite3_bind_text( stmt, 1, dirpath, -1, NULL );
	ret = sqlite3_step( stmt );
	if( ret != SQLITE_ROW ) {
		return NULL;
	}
	dir = DB_LoadDir( stmt );
	return dir;
}

void DBDir_Release( DB_Dir dir )
{
	free( dir->path );
	if( dir->token ) {
		free( dir->token );
	}
	dir->path = NULL;
	dir->token = NULL;
	free( dir );
}

void DB_DeleteDir( DB_Dir dir )
{
	sqlite3_stmt *stmt = self.stmts[SQL_DEL_DIR];
	sqlite3_reset( stmt );
	sqlite3_bind_int( stmt, 1, dir->id );
	sqlite3_step( stmt );
}

int DB_GetDirs( DB_Dir **outlist )
{
	DB_Dir *list;
	sqlite3_stmt *stmt;
	int ret, i, total = 0;
	stmt = self.stmts[SQL_GET_DIR_TOTAL];
	sqlite3_reset( stmt );
	ret = sqlite3_step( stmt );
	if( ret == SQLITE_ROW ) {
		total = sqlite3_column_int( stmt, 0 );
	}
	if( total == 0 ) {
		*outlist = NULL;
		return 0;
	}
	list = malloc( sizeof(DB_Dir) * (total + 1) );
	if( !list ) {
		return -1;
	}
	list[total] = NULL;
	stmt = self.stmts[SQL_GET_DIR_LIST];
	sqlite3_reset( stmt );
	for( i = 0; i < total; ++i ) {
		ret = sqlite3_step( stmt );
		if( ret != SQLITE_ROW ) {
			list[i] = NULL;
			break;
		}
		list[i] = DB_LoadDir( stmt );
	}
	*outlist = list;
	return i;
}

DB_Tag DB_AddTag( const char *tagname )
{
	int ret;
	DB_Tag tag;
	sqlite3_stmt *stmt;
	stmt = self.stmts[SQL_ADD_TAG];
	sqlite3_reset( stmt );
	sqlite3_bind_text( stmt, 1, tagname, strlen( tagname ), NULL );
	ret = sqlite3_step( stmt );
	if( ret != SQLITE_DONE ) {
		printf( "[database] error: %s\n", sqlite3_errmsg( self.db ) );
		return NULL;
	}
	stmt = self.stmts[SQL_GET_TAG];
	sqlite3_reset( stmt );
	sqlite3_bind_text( stmt, 1, tagname, strlen( tagname ), NULL );
	ret = sqlite3_step( stmt );
	if( ret != SQLITE_ROW ) {
		return NULL;
	}
	tag = malloc( sizeof( DB_TagRec ) );
	tag->id = sqlite3_column_int( stmt, 0 );
	tag->name = strdup( tagname );
	tag->count = 0;
	return tag;
}

void DB_AddFile( DB_Dir dir, const char *filepath, int ctime, int mtime )
{
	sqlite3_stmt *stmt = self.stmts[SQL_ADD_FILE];
	sqlite3_reset( stmt );
	sqlite3_bind_int( stmt, 1, dir->id );
	sqlite3_bind_text( stmt, 2, filepath, strlen( filepath ), NULL );
	sqlite3_bind_int( stmt, 3, ctime );
	sqlite3_bind_int( stmt, 4, mtime );
	sqlite3_step( stmt );
}

void DB_UpdateFileTime( DB_Dir dir, const char *filepath, 
			int ctime, int mtime )
{
	sqlite3_stmt *stmt = self.stmts[SQL_SET_FILE_TIME_BY_PATH];
	sqlite3_reset( stmt );
	sqlite3_bind_int( stmt, 1, ctime );
	sqlite3_bind_int( stmt, 2, mtime );
	sqlite3_bind_int( stmt, 3, dir->id );
	sqlite3_bind_text( stmt, 4, filepath, strlen(filepath), NULL );
	sqlite3_step( stmt );
}

void DB_DeleteFile( const char *filepath )
{
	sqlite3_stmt *stmt = self.stmts[SQL_DEL_FILE];
	sqlite3_reset( stmt );
	sqlite3_bind_text( stmt, 1, filepath, strlen( filepath ), NULL );
	sqlite3_step( stmt );
}

DB_File DBFile_Dup( DB_File file )
{
	DB_File f = malloc( sizeof(DB_FileRec) );
	*f = *file;
	f->path = strdup( file->path );
	return f;
}

void DBFile_Release( DB_File file )
{
	free( file->path );
	file->path = NULL;
	free( file );
}

static DB_File DB_LoadFile( sqlite3_stmt *stmt )
{
	int len;
	DB_File file;
	const char *path;
	if( sqlite3_step( stmt ) != SQLITE_ROW ) {
		return NULL;
	}
	file = malloc( sizeof( DB_FileRec ) );
	file->id = sqlite3_column_int( stmt, 0 );
	file->did = sqlite3_column_int( stmt, 1 );
	file->score = sqlite3_column_int( stmt, 2 );
	path = sqlite3_column_text( stmt, 3 );
	file->width = sqlite3_column_int( stmt, 4 );
	file->height = sqlite3_column_int( stmt, 5 );
	file->create_time = sqlite3_column_int( stmt, 6 );
	file->modify_time = sqlite3_column_int( stmt, 7 );
	len = strlen( path ) + 1;
	file->path = malloc( len *sizeof( char ) );
	strncpy( file->path, path, len );
	return file;
}

DB_File DB_GetFile( const char *filepath )
{
	sqlite3_stmt *stmt = self.stmts[SQL_GET_FILE];
	sqlite3_reset( stmt );
	sqlite3_bind_text( stmt, 1, filepath, strlen( filepath ), NULL );
	return DB_LoadFile( stmt );
}

int DB_GetTags( DB_Tag **outlist )
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

int DBFile_RemoveTag( DB_File file, DB_Tag tag )
{
	int ret;
	sqlite3_stmt *stmt = self.stmts[SQL_DEL_FILE_TAG];
	sqlite3_reset( stmt );
	sqlite3_bind_int( stmt, 1, file->id );
	sqlite3_bind_int( stmt, 2, tag->id );
	ret = sqlite3_step( stmt );
	if( ret == SQLITE_DONE ) {
		return 0;
	}
	printf( "[database] error: %s\n", sqlite3_errmsg( self.db ) );
	return -1;
}

int DBFile_AddTag( DB_File file, DB_Tag tag )
{
	int ret;
	sqlite3_stmt *stmt = self.stmts[SQL_ADD_FILE_TAG];
	sqlite3_reset( stmt );
	sqlite3_bind_int( stmt, 1, file->id );
	sqlite3_bind_int( stmt, 2, tag->id );
	ret = sqlite3_step( stmt );
	if( ret == SQLITE_DONE ) {
		return 0;
	}
	printf( "[database] error: %s\n", sqlite3_errmsg( self.db ) );
	return -1;
}

int DBFile_GetTags( DB_File file, DB_Tag **outtags )
{
	const char *name;
	int len, total = 0;
	sqlite3_stmt *stmt;
	DB_Tag tag, *tags = NULL, *newtags;
	stmt = self.stmts[SQL_GET_FILE_TAGS];
	sqlite3_reset( stmt );
	sqlite3_bind_int( stmt, 1, file->id );
	while( sqlite3_step( stmt ) == SQLITE_ROW ) {
		++total;
		newtags = realloc( tags, (total + 1) * sizeof( DB_Tag ) );
		if( !newtags ) {
			free( tags );
			return -ENOMEM;
		}
		tags = newtags;
		tag = malloc( sizeof( DB_TagRec ) );
		tag->id = sqlite3_column_int( stmt, 0 );
		name = sqlite3_column_text( stmt, 1 );
		len = strlen( name ) + 1;
		tag->name = malloc( len * sizeof( char ) );
		strncpy( tag->name, name, len );
		tag->count = sqlite3_column_int( stmt, 2 );
		tags[total - 1] = tag;
	}
	if( total > 0 ) {
		tags[total] = NULL;
	}
	*outtags = tags;
	return total;
}

int DBFile_SetScore( DB_File file, int score )
{
	int ret;
	sqlite3_stmt *stmt = self.stmts[SQL_SET_FILE_SCORE];
	sqlite3_reset( stmt );
	sqlite3_bind_int( stmt, 1, score );
	sqlite3_bind_int( stmt, 2, file->id );
	ret = sqlite3_step( stmt );
	if( ret == SQLITE_DONE ) {
		return 0;
	}
	printf( "[database] error: %s\n", sqlite3_errmsg( self.db ) );
	return -1;
}

int DBFile_SetTime( DB_File file, int ctime, int mtime )
{
	int ret;
	sqlite3_stmt *stmt = self.stmts[SQL_SET_FILE_TIME];
	sqlite3_reset( stmt );
	sqlite3_bind_int( stmt, 1, ctime );
	sqlite3_bind_int( stmt, 2, mtime );
	sqlite3_bind_int( stmt, 3, file->id );
	ret = sqlite3_step( stmt );
	if( ret == SQLITE_DONE ) {
		file->create_time = ctime;
		file->modify_time = mtime;
		return 0;
	}
	printf( "[database] error: %s\n", sqlite3_errmsg( self.db ) );
	return -1;
}

int DBFile_SetSize( DB_File file, int width, int height )
{
	int ret;
	sqlite3_stmt *stmt = self.stmts[SQL_SET_FILE_SIZE];
	sqlite3_reset( stmt );
	sqlite3_bind_int( stmt, 1, width );
	sqlite3_bind_int( stmt, 2, height );
	sqlite3_bind_int( stmt, 3, file->id );
	ret = sqlite3_step( stmt );
	if( ret == SQLITE_DONE ) {
		file->width = width;
		file->height = height;
		return 0;
	}
	printf( "[database] error: %s\n", sqlite3_errmsg( self.db ) );
	return -1;
}

void DBTag_Release( DB_Tag tag )
{
	free( tag->name );
	tag->name = NULL;
	free( tag );
}

int DBQuery_GetTotalFiles( DB_Query query )
{
	int total = 0;
	sqlite3_stmt *stmt;
	char sql[SQL_BUF_SIZE];
	if( !query ) {
		return 0;
	}
	sprintf( sql, "%s (SELECT * FROM file f %s %s%s%s)", 
		 sql_count_files, query->sql_tables, query->sql_terms, 
		 query->sql_groupby, query->sql_having );
	sqlite3_prepare_v2( self.db, sql, -1, &stmt, NULL );
	if( sqlite3_step( stmt ) == SQLITE_ROW ) {
		total = sqlite3_column_int( stmt, 0 );
	}
	sqlite3_finalize( stmt );
	return total;
}

static int escape( char *buf, const char *str )
{
	char *outptr = buf;
	const char *inptr = str;
	while( *inptr ) {
		switch( *inptr ) {
		case '_':
		case '%':
		case '\\':
			*outptr = '\\';
			++outptr;
		default: break;
		}
		*outptr = *inptr;
		++outptr;
		++inptr;
	}
	*outptr = 0;
	return outptr - buf;
}

DB_File DBQuery_FetchFile( DB_Query query )
{
	return DB_LoadFile( query->stmt );
}

DB_Query DB_NewQuery( const DB_QueryTerms terms )
{
	int i;
	char sql[SQL_BUF_SIZE];
	char buf_terms[256] = " WHERE ";
	char buf_having[256] = "HAVING ";
	char buf_orderby[256] = "ORDER BY ";
	char buf_groupby[256] = "GROUP BY ";
	DB_Query q = calloc( 1, sizeof(DB_QueryRec) );
	if( terms->n_dirs > 0 && terms->dirs ) {
		strcpy( q->sql_terms, buf_terms );
		strcat( q->sql_tables, ", dir d" );
		strcat( q->sql_terms, "f.did = d.id " );
		strcat( q->sql_terms, "AND f.did IN (" );
		for( i = 0; i < terms->n_dirs; ++i ) {
			sprintf( buf_terms, "%d", terms->dirs[i]->id );
			if( i > 0 ) {
				strcat( q->sql_terms, ", " );
			}
			strcat( q->sql_terms, buf_terms );
		}
		strcat( q->sql_terms, ") " );
		strcpy( buf_terms, " AND" );
	}
	if( terms->n_tags > 0 && terms->tags ) {
		strcat( q->sql_terms, buf_terms );
		if( terms->n_tags == 1 ) {
			sprintf( buf_terms, "ftr.tid = %d ", 
				 terms->tags[0]->id );
			strcat( q->sql_terms, buf_terms );
		} else {
			strcat( q->sql_terms, "ftr.tid IN (" );
			for( i = 0; i < terms->n_tags; ++i ) {
				sprintf( buf_terms, "%d", 
					 terms->tags[i]->id );
				if( i > 0 ) {
					strcat( q->sql_terms, ", " );
				}
				strcat( q->sql_terms, buf_terms );
			}
			strcat( q->sql_terms, ") " );
			strcpy( q->sql_having, buf_having );
			sprintf( buf_having, "COUNT(ftr.tid) = %d ", 
				 terms->n_tags );
			strcat( q->sql_having, buf_having );
		}
		strcat( q->sql_tables, ", file_tag_relation ftr " );
		strcat( q->sql_terms, "AND ftr.fid = f.id " );
		strcpy( buf_terms, "AND " );
		strcpy( q->sql_groupby, buf_groupby );
		strcat( q->sql_groupby, "ftr.fid " );
		strcpy( buf_having, "AND " );
		strcpy( buf_groupby, ", " );
	}
	if( terms->dirpath ) {
		char *path;
		i = 2 * strlen( terms->dirpath );
		path = malloc( i * sizeof( char ) );
		strcat( q->sql_terms, buf_terms );
		/* 如果是要在当前目录下的整个子级目录树中搜索文件 */
		if( terms->for_tree ) {
			char *path;
			i = 2 * strlen( terms->dirpath );
			path = malloc( i * sizeof( char ) );
			escape( path, terms->dirpath );
			strcat( q->sql_terms, " f.path LIKE '" );
			strcat( q->sql_terms, path );
			strcat( q->sql_terms, "%' ESCAPE '\\'" );
			free( path );
		} else {
			strcat( q->sql_terms, "HASFILE('" );
			strcat( q->sql_terms, terms->dirpath );
			strcat( q->sql_terms, "', f.path) " );
		}
	}
	if( terms->create_time == DESC ) {
		strcat( q->sql_orderby, buf_orderby );
		strcat( q->sql_orderby, "f.create_time DESC " );
		strcpy( buf_orderby, ", " );
	} else if( terms->create_time == ASC ) {
		strcat( q->sql_orderby, buf_orderby );
		strcat( q->sql_orderby, "f.create_time ASC " );
		strcpy( buf_orderby, ", " );
	}
	if( terms->modify_time == DESC ) {
		strcat( q->sql_orderby, buf_orderby );
		strcat( q->sql_orderby, "f.modify_time DESC " );
		strcpy( buf_orderby, ", " );
	} else if( terms->modify_time == ASC ) {
		strcat( q->sql_orderby, buf_orderby );
		strcat( q->sql_orderby, "f.modify_time ASC " );
		strcpy( buf_orderby, ", " );
	}
	if( terms->score != NONE ) {
		strcat( q->sql_orderby, buf_orderby );
		if( terms->score == DESC ) {
			strcat( q->sql_orderby, "f.score DESC " );
		} else {
			strcat( q->sql_orderby, "f.score ASC " );
		}
		strcpy( buf_orderby, ", " );
	}
	sprintf( q->sql_limit, " LIMIT %d OFFSET %d", 
		 terms->limit, terms->offset );
	strcpy( sql, sql_search_files );
	strcat( sql, q->sql_tables );
	strcat( sql, q->sql_terms );
	strcat( sql, q->sql_groupby );
	strcat( sql, q->sql_having );
	strcat( sql, q->sql_orderby );
	strcat( sql, q->sql_limit );
	i = sqlite3_prepare_v2( self.db, sql, -1, &q->stmt, NULL );
	if( i == SQLITE_OK ) {
		return q;
	}
	return NULL;
}

void DB_DeleteQuery( DB_Query query )
{
	sqlite3_finalize( query->stmt );
	query->stmt = NULL;
	free( query );
}

int DB_Begin( void )
{
	return sqlite3_exec( self.db, "begin;", NULL, NULL, NULL );
}

int DB_Commit( void )
{
	return sqlite3_exec( self.db, "commit;", NULL, NULL, NULL );
}
