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
#ifdef LCFINDER_USE_UNQLITE
#include "kvdb.h"
#include "thumb_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef LCUI_BUILD_IN_LINUX
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "unqlite.h"

#define MAX_KEY_LEN 1024

typedef struct kvdb_t {
	unqlite *db;
} kvdb_t;

kvdb_t *kvdb_open(const char *name)
{
	kvdb_t *db = malloc(sizeof(kvdb_t));

	if (unqlite_open(&db->db, name, UNQLITE_OPEN_CREATE) != UNQLITE_OK) {
		free(db);
		return NULL;
	}

	return db;
}

void kvdb_close(kvdb_t *db)
{
	unqlite_close(db->db);
	free(db);
}

int kvdb_destroy_db(const char *name)
{
	return remove(name);
}

int kvdb_get_db_size(const char *name, int64_t *size)
{
	struct stat buf;
	if (stat(name, &buf) == 0) {
		*size = buf.st_size;
		return 0;
	}
	return -1;
}

void *kvdb_get(kvdb_t *db, const char *key, size_t keylen, size_t *vallen)
{
	int rc;
	void *val;
	unqlite_int64 len = *vallen;

	rc = unqlite_kv_fetch(db->db, key, keylen, NULL, &len);
	if (rc != UNQLITE_OK) {
		return NULL;
	}
	val = malloc((size_t)len);
	rc = unqlite_kv_fetch(db->db, key, keylen, val, &len);
	if (rc != UNQLITE_OK) {
		free(val);
		return NULL;
	}
	*vallen = (size_t)len;
	return val;
}

int kvdb_put(kvdb_t *db, const char *key, size_t keylen, const void *val,
	     size_t vallen)
{
	if (unqlite_kv_store(db->db, key, keylen, val, vallen) == UNQLITE_OK) {
		unqlite_commit(db->db);
	}
	return 0;
}

int kvdb_delete(kvdb_t *db, const char *key, size_t keylen)
{
	if (unqlite_kv_delete(db->db, key, keylen) == UNQLITE_OK) {
		return 0;
	}
	return -1;
}

size_t kvdb_each(kvdb_t *db, kvdb_each_callback_t callback, void *privdata)
{
	int keylen;
	char key[MAX_KEY_LEN];
	char *val;

	size_t count = 0;
	unqlite_kv_cursor *cur;
	unqlite_int64 vallen;

	if (unqlite_kv_cursor_init(db->db, &cur) != UNQLITE_OK) {
		return 0;
	}
	if (unqlite_kv_cursor_first_entry(cur) != UNQLITE_OK) {
		unqlite_kv_cursor_release(db->db, cur);
		return 0;
	}
	while (unqlite_kv_cursor_valid_entry(cur)) {
		keylen = MAX_KEY_LEN;
		vallen = 0;

		unqlite_kv_cursor_key(cur, key, &keylen);
		unqlite_kv_cursor_data(cur, NULL, &vallen);

		val = malloc((size_t)vallen);
		unqlite_kv_cursor_data(cur, val, &vallen);
		unqlite_kv_cursor_next_entry(cur);

		callback(key, (size_t)keylen, val, (size_t)vallen, privdata);

		++count;
	}
	unqlite_kv_cursor_release(db->db, cur);
	return count;
}
#endif
