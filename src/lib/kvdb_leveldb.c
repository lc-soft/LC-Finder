/* ***************************************************************************
 * kvdb.c -- key-value database, based on LevelDB
 *
 * Copyright (C) 2018 by Liu Chao <lc-soft@live.cn>
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

#define HAVE_THUMB_DB_ENGINE
#include "build.h"
#ifdef LCFINDER_USE_LEVELDB
#include "kvdb.h"
#include "thumb_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <leveldb/c.h>
#include <LCUI_Build.h>
#include <LCUI/util/dirent.h>

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#define PATH_LEN 1024

typedef struct kvdb_t {
	leveldb_t *db;
	leveldb_options_t *options;
	leveldb_readoptions_t *roptions;
	leveldb_writeoptions_t *woptions;
} kvdb_t;

static leveldb_options_t *kvdb_options_create(void)
{
	leveldb_options_t *options = leveldb_options_create();
	leveldb_options_set_info_log(options, NULL);
	leveldb_options_set_write_buffer_size(options, 100000);
	leveldb_options_set_max_open_files(options, 10);
	leveldb_options_set_block_size(options, 1024);
	leveldb_options_set_block_restart_interval(options, 8);
	leveldb_options_set_compression(options, leveldb_no_compression);
	return options;
}

kvdb_t *kvdb_open(const char *name)
{
	char *err = NULL;
	kvdb_t *db = malloc(sizeof(kvdb_t));

	db->options = kvdb_options_create();
	db->woptions = leveldb_writeoptions_create();
	db->roptions = leveldb_readoptions_create();
	leveldb_options_set_create_if_missing(db->options, 1);
	leveldb_readoptions_set_fill_cache(db->roptions, 0);
	leveldb_readoptions_set_verify_checksums(db->roptions, 1);
	leveldb_writeoptions_set_sync(db->woptions, 1);
	db->db = leveldb_open(db->options, name, &err);
	if (err) {
		printf("[kvdb] error: %s\n", err);
		return NULL;
	}
	return db;
}

void kvdb_close(kvdb_t *db)
{
	assert(db && db->db);
	leveldb_readoptions_destroy(db->roptions);
	leveldb_writeoptions_destroy(db->woptions);
	leveldb_options_destroy(db->options);
	leveldb_close(db->db);
	free(db);
}

int kvdb_destroy_db(const char *name)
{
	char *err = NULL;
	leveldb_options_t *options = kvdb_options_create();
	leveldb_destroy_db(options, name, &err);
	if (err) {
		printf("[kvdb] error: %s\n", err);
		return -1;
	}
	return 0;
}

int kvdb_get_db_size(const char *name, int64_t *size)
{
	size_t len;
	char *filename;
	char path[PATH_LEN];
	struct stat buf;

	LCUI_Dir dir;
	LCUI_DirEntry *entry;

	if (LCUI_OpenDirA(name, &dir) != 0) {
		return -1;
	}
	*size = 0;
	len = strlen(name);
	strcpy(path, name);
	path[len++] = PATH_SEP;
	path[len] = 0;
	while ((entry = LCUI_ReadDirA(&dir))) {
		filename = LCUI_GetFileNameA(entry);
		if (filename[0] == '.') {
			continue;
		}
		if (!LCUI_FileIsRegular(entry)) {
			continue;
		}
		strcpy(path + len, filename);
		if (stat(path, &buf) == 0) {
			*size += buf.st_size;
		}
	}
	return 0;
}

void *kvdb_get(kvdb_t *db, const char *key, size_t keylen, size_t *vallen)
{
	char *err = NULL;
	char *value = leveldb_get(db->db, db->roptions, key,
				  keylen, vallen, &err);
	if (err) {
		printf("[kvdb] error: %s\n", err);
		return NULL;
	}
	return value;
}

int kvdb_put(kvdb_t *db, const char *key, size_t keylen,
	     const void *val, size_t vallen)
{
	char *err = NULL;
	leveldb_put(db->db, db->woptions, key, keylen, val, vallen, &err);
	if (err) {
		printf("[kvdb] error: %s\n", err);
		return -1;
	}
	return 0;
}

int kvdb_delete(kvdb_t *db, const char *key, size_t keylen)
{
	char *err = NULL;
	leveldb_delete(db->db, db->woptions, key, keylen, &err);
	if (err) {
		printf("[kvdb] error: %s\n", err);
		return -1;
	}
	return 0;
}

size_t kvdb_each(kvdb_t *db, kvdb_each_callback_t callback, void *privdata)
{
	size_t count = 0;
	size_t keylen, vallen;
	const char *key, *val;

	leveldb_iterator_t *iter;

	iter = leveldb_create_iterator(db->db, db->roptions);
	leveldb_iter_seek_to_first(iter);

	while (leveldb_iter_valid(iter)) {
		key = leveldb_iter_key(iter, &keylen);
		val = leveldb_iter_value(iter, &vallen);

		callback(key, keylen, val, vallen, privdata);

		leveldb_iter_next(iter);
		++count;
	}
	leveldb_iter_destroy(iter);
	return count;
}
#endif
