/* ***************************************************************************
 * kvdb.h -- key-value database
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

#ifndef KVDB_H
#define KVDB_H

#include <stdint.h>
#include <stddef.h>

typedef struct kvdb_t kvdb_t;

typedef void(*kvdb_each_callback_t)(
	const char*, size_t, const void*, size_t, void*
);

kvdb_t *kvdb_open(const char *name);

void kvdb_close(kvdb_t *db);

int kvdb_destroy_db(const char *name);

int kvdb_get_db_size(const char *name, int64_t *size);

void *kvdb_get(kvdb_t *db, const char *key, size_t keylen, size_t *vallen);

int kvdb_put(kvdb_t *db, const char *key, size_t keylen,
	     const void *val, size_t vallen);

int kvdb_delete(kvdb_t *db, const char *key, size_t keylen);

size_t kvdb_each(kvdb_t *db, kvdb_each_callback_t callback, void *privdata);

#endif
