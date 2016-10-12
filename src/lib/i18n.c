/* ***************************************************************************
 * i18n.c -- internationalization suport module.
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
 * textview_i18n.c -- 国际化支持模块。
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

#include <yaml.h>
#include <LCUI_Build.h>
#include <LCUI/util/dict.h>
#include "common.h"
#include "i18n.h"

enum DictValueType {
	NONE,
	STRING,
	DICT
};

typedef struct DictStringValueRec_ {
	char *data;
	size_t length;
} DictStringValueRec, *DictStringValue;

typedef struct DictValueRec_ DictValueRec, *DictValue;
struct DictValueRec_ {
	int type;
	char *key;
	union {
		DictStringValueRec string;
		Dict *dict;
	};
	Dict *parent_dict;
	DictValue parent_value;
};

static char *yaml_get_token_string( yaml_token_t *token )
{
	size_t len = token->data.scalar.length + 1;
	char *str = malloc( sizeof( char ) * len );
	if( str ) {
		strncpy( str, token->data.scalar.value, len );
	}
	return str;
}

static void DeleteDictValue( void *privdata, void *data )
{
	DictValue value = data;
	if( value->type == DICT ) {
		Dict_Release( value->dict );
	} else if( value->type == STRING ) {
		free( value->string.data );
	}
	free( value->key );
	free( data );
}

Dict *I18n_LoadFile( const char *path )
{
	FILE *file;
	yaml_token_t token;
	yaml_parser_t parser;
	Dict *dict, *parent_dict;
	DictValue value, parent_value;
	int state = 0;

	file = fopen( path, "r" );
	if( !file ) {
		fprintf( stderr, "[i18n] Failed to open file: %s\n", path );
		return NULL;
	}
	parent_value = value = NULL;
	parent_dict = dict = StrDict_Create( NULL, DeleteDictValue );
	if( !yaml_parser_initialize( &parser ) ) {
		fputs( "[i18n] Failed to initialize parser!\n", stderr );
		return NULL;
	}
	yaml_parser_set_input_file( &parser, file );
	do {
		if( !yaml_parser_scan( &parser, &token ) ) {
			Dict_Release( dict );
			dict = NULL;
			break;
		}
		switch( token.type ) {
		case YAML_KEY_TOKEN: state = 0; break;
		case YAML_VALUE_TOKEN: state = 1; break;
		case YAML_BLOCK_MAPPING_START_TOKEN: 
			if( !value ) {
				break;
			}
			value->type = DICT;
			value->dict = StrDict_Create( NULL, DeleteDictValue );
			value->parent_value = parent_value;
			parent_dict = value->dict;
			parent_value = value;
			break;
		case YAML_BLOCK_END_TOKEN: 
			if( parent_value ) {
				parent_value = parent_value->parent_value;
			}
			break;
		case YAML_SCALAR_TOKEN:
			if( state == 0 ) {
				value = malloc( sizeof( DictValueRec ) );
				value->type = NONE;
				value->parent_dict = parent_dict;
				value->parent_value = parent_value;
				value->key = yaml_get_token_string( &token );
				Dict_Add( parent_dict, value->key, value );
				break;
			}
			value->type = STRING;
			value->string.length = token.data.scalar.length;
			value->string.data = yaml_get_token_string( &token );
			break;
		default: break;
		}
		if( token.type != YAML_STREAM_END_TOKEN ) {
			yaml_token_delete( &token );
		}
	} while( token.type != YAML_STREAM_END_TOKEN );
	yaml_token_delete( &token );
	yaml_parser_delete( &parser );
	fclose( file );
	return dict;
}

const char *I18n_GetText( Dict *dict, const char *keystr )
{
	int i;
	char key[256];
	const char *p;
	DictValue value;

	for( i = 0, p = keystr, value = NULL; *p; ++p, ++i ) {
		if( *p != '.' ) {
			key[i] = *p;
			continue;
		}
		key[i] = 0;
		value = Dict_FetchValue( dict, key );
		if( value ) {
			if( value->type == DICT ) {
				dict = value->dict;
			} else {
				break;
			}
		}
		i = -1;
	}
	if( i > 0 ) {
		key[i] = 0;
		value = Dict_FetchValue( dict, key );
	}
	if( value && value->type == STRING ) {
		return value->string.data;
	}
	return NULL;
}

/** 测试用例 */
int TestI18n( void )
{
	Dict *dict = I18n_LoadFile( "lang/en.yaml" );
	if( !dict ) {
		return -1;
	}
	printf( "============ test i18n api ============\n"
		"name: %s\nstrings.title: %s\n"
		"strings.window.title: %s\n"
		"=======================================\n",
		I18n_GetText( dict, "name" ),
		I18n_GetText( dict, "strings.title" ),
		I18n_GetText( dict, "strings.window.title" ) );
	return 0;
}
