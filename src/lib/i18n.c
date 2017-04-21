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
#include <LCUI/LCUI.h>
#include <LCUI/font/charset.h>
#include "build.h"
#include "common.h"
#include "i18n.h"

enum DictValueType {
	NONE,
	STRING,
	DICT
};

typedef struct DictStringValueRec_ {
	wchar_t *data;
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

static struct I18nModule {
	int length;		/**< 语言列表的长度 */
	Language *languages;	/**< 语言列表 */
	Language language;	/**< 当前选中的语言 */
	Dict *texts;		/**< 文本库 */
} self = {0};

static char *yaml_token_getstr( yaml_token_t *token )
{
	size_t len = token->data.scalar.length + 1;
	char *str = malloc( sizeof( char ) * len );
	if( str ) {
		strncpy( str, token->data.scalar.value, len );
		str[len - 1] = 0;
	}
	return str;
}

static wchar_t *yaml_token_getwcs( yaml_token_t *token )
{
	char *str = token->data.scalar.value;
	size_t len = token->data.scalar.length + 1;
	wchar_t *wcs = malloc( sizeof( wchar_t ) * len );
	LCUI_DecodeString( wcs, str, (int)len, ENCODING_UTF8 );
	return wcs;
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

static Language I18n_AddLanguage( const char *path, Dict *dict )
{
	int i, pos;
	DictValue name, code;
	Language lang, *langs;
	size_t len = strlen( path ) + 1;
	lang = malloc( sizeof( LanguageRec ) );
	name = Dict_FetchValue( dict, "name" );
	code = Dict_FetchValue( dict, "code" );
	if( !name || !code ) {
		return NULL;
	}
	lang->filename = malloc( sizeof( char ) * len );
	strncpy( lang->filename, path, len );
	lang->name = EncodeUTF8( name->string.data );
	lang->code = EncodeUTF8( code->string.data );
	len = self.length + 2;
	langs = realloc( self.languages, sizeof( Language ) * len );
	if( !langs ) {
		return NULL;
	}
	langs[len - 1] = NULL;
	for( i = 0, pos = -1; i < self.length; ++i ) {
		if( strcmp( lang->code, langs[i]->code ) < 0 ) {
			pos = i;
			break;
		}
	}
	if( pos == -1 ) {
		pos = self.length;
	} else {
		for( i = self.length; i > pos; --i ) {
			langs[i] = langs[i - 1];
		}
	}
	langs[pos] = lang;
	self.languages = langs;
	self.length += 1;
	return lang;
}

static char *LoadFileToBuffer( const char *path, size_t *size )
{
	char *buffer, *p;
	size_t buffer_size = 0, read_size = 256;
	FILE *file = fopen( path, "rb" );
	if( !file ) {
		return NULL;
	}
	buffer = NULL;
	while( !feof( file ) ) {
		p = realloc( buffer, buffer_size + sizeof( char ) * 256 );
		if( !p ) {
			free( buffer );
			fclose( file );
			return NULL;
		}
		buffer = p;
		p = buffer + buffer_size;
		read_size = fread( p, sizeof( char ), 256, file );
		buffer_size += read_size;
	};
	*size = buffer_size;
	buffer[buffer_size] = 0;
	fclose( file );
	return buffer;
}

Dict *I18n_LoadFile( const char *path )
{
	size_t size;
	char *buffer;
	yaml_token_t token;
	yaml_parser_t parser;
	Dict *dict, *parent_dict;
	DictValue value, parent_value;
	int state = 0;

	parent_value = value = NULL;
	parent_dict = dict = StrDict_Create( NULL, DeleteDictValue );
	if( !yaml_parser_initialize( &parser ) ) {
		LOG( "[i18n] failed to initialize parser!\n" );
		return NULL;
	}
	LOG( "[i18n] load language file: %s\n", path );
	buffer = LoadFileToBuffer( path, &size );
	if( !buffer ) {
		LOG( "[i18n] failed to open file: %s\n", path );
		return NULL;
	}
	yaml_parser_set_input_string( &parser, buffer, size );
	do {
		if( !yaml_parser_scan( &parser, &token ) ) {
			LOG( "[i18n] error: %s\n", parser.problem );
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
				if(parent_value ) {
					parent_dict = parent_value->dict;
				} else {
					parent_dict = dict;
				}
			}
			break;
		case YAML_SCALAR_TOKEN:
			if( state == 0 ) {
				value = malloc( sizeof( DictValueRec ) );
				value->type = NONE;
				value->parent_dict = parent_dict;
				value->parent_value = parent_value;
				value->key = yaml_token_getstr( &token );
				Dict_Add( parent_dict, value->key, value );
				break;
			}
			value->type = STRING;
			value->string.length = token.data.scalar.length;
			value->string.data = yaml_token_getwcs( &token );
			break;
		default: break;
		}
		if( token.type != YAML_STREAM_END_TOKEN ) {
			yaml_token_delete( &token );
		}
	} while( token.type != YAML_STREAM_END_TOKEN );
	yaml_token_delete( &token );
	yaml_parser_delete( &parser );
	free( buffer );
	return dict;
}

Language I18n_LoadLanguage( const char *filename )
{
	Dict *dict;
	Language lang;
	dict = I18n_LoadFile( filename );
	if( !dict ) {
		return NULL;
	}
	lang = I18n_AddLanguage( filename, dict );
	Dict_Release( dict );
	if( lang ) {
		printf( "[i18n] language loaded, name: %s, code: %s\n", 
			lang->name, lang->code );
	}
	return lang;
}

const wchar_t *I18n_GetText( const char *keystr )
{
	int i;
	char key[256];
	const char *p;
	Dict *dict;
	DictValue value;

	if( !self.texts ) {
		return NULL;
	}
	value = Dict_FetchValue( self.texts, "strings" );
	if( !value || value->type != DICT ) {
		return NULL;
	}
	dict = value->dict;
	for( i = 0, p = keystr, value = NULL; *p; ++p, ++i ) {
		if( *p != '.' ) {
			key[i] = *p;
			continue;
		}
		key[i] = 0;
		value = Dict_FetchValue( dict, key );
		if( value && value->type == DICT ) {
			dict = value->dict;
		} else {
			return NULL;
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

int I18n_GetLanguages( Language **languages )
{
	*languages = self.languages;
	return self.length;
}

Language I18n_SetLanguage( const char *lang_code )
{
	int i;
	for( i = 0; i < self.length; ++i ) {
		Dict *dict;
		Language lang;
		lang = self.languages[i];
		if( strcmp( lang->code, lang_code ) ) {
			continue;
		}
		dict = I18n_LoadFile( lang->filename );
		if( !dict ) {
			break;
		}
		self.texts = dict;
		self.language = lang;
		return lang;
	}
	return NULL;
}

#ifdef _WIN32

#include <windows.h>

int I18n_GetDefaultLanguage( char *lang, int max_len )
{
	wchar_t buf[64] = L"en-US";
#ifndef PLATFORM_WIN32_DESKTOP_XP
	GetUserDefaultLocaleName( buf, 63 );
#endif
	return LCUI_EncodeString( lang, buf, max_len, ENCODING_UTF8 );
}

#else

int I18n_GetDefaultLanguage( char *lang )
{
	strcpy( lang, "en-US" );
}

#endif
