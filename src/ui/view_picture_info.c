/* ***************************************************************************
* view_picture_info.c -- picture info view
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
* view_picture_info.c -- "图片信息" 视图
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

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "finder.h"
#include "ui.h"
#include <LCUI/timer.h>
#include <LCUI/display.h>
#include <LCUI/cursor.h>
#include <LCUI/graph.h>
#include <LCUI/gui/builder.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include "dialog.h"
#include "starrating.h"

#define DIALOG_TITLE_ADDTAG		L"添加标签"
#define DIALOG_TITLE_EDITTAG		L"编辑标签"
#define DIALOG_INPUT_PLACEHOLDER	L"请输入标签名称"
#define XML_PATH			"res/ui-view-picture-info.xml"

struct PictureInfoPanel {
	LCUI_Widget window;
	LCUI_Widget panel;
	LCUI_Widget txt_name;
	LCUI_Widget txt_time;
	LCUI_Widget txt_size;
	LCUI_Widget txt_fsize;
	LCUI_Widget txt_dirpath;
	LCUI_Widget view_tags;
	LCUI_Widget rating;
	wchar_t *filepath;
	uint64_t size;
	uint_t width;
	uint_t height;
	time_t mtime;
	DB_File file;
	DB_Tag *tags;
	int n_tags;
} this_view;

typedef struct TagInfoPackRec_ {
	LCUI_Widget widget;
	DB_Tag tag;
} TagInfoPackRec, *TagInfoPack;

static LCUI_BOOL CheckTagName( const wchar_t *tagname )
{
	if( wgetcharcount( tagname, L" ,;\n\r\t" ) > 0 ) {
		return FALSE;
	}
	return TRUE;
}

static void OnBtnDeleteTagClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	int i;
	TagInfoPack pack = e->data;
	wchar_t name[256], text[300];
	LCUI_DecodeString( name, pack->tag->name, 256, ENCODING_UTF8 );
	swprintf( text, 300, L"确定要移除 %s 标签？", name );
	if( !LCUIDialog_Confirm( this_view.window, L"提示", text ) ) {
		return;
	}
	LCFinder_TriggerEvent( EVENT_TAG_UPDATE, pack->tag );
	DBFile_RemoveTag( this_view.file, pack->tag );
	Widget_Destroy( pack->widget );
	pack->tag->count -= 1;
	for( i = 0; i < this_view.n_tags; ++i ) {
		if( this_view.tags[i]->id != pack->tag->id ) {
			return;
		}
		this_view.n_tags -= 1;
		for( ; i < this_view.n_tags; ++i ) {
			this_view.tags[i] = this_view.tags[i + 1];
		}
		this_view.tags[this_view.n_tags] = NULL;
	}
}

static void PictureInfo_AppendTag( DB_Tag tag )
{
	size_t size;
	DB_Tag *tags;
	TagInfoPack pack;
	LCUI_Widget box, txt_name, btn_del;

	this_view.n_tags += 1;
	size = (this_view.n_tags + 1) * sizeof( DB_Tag );
	tags = realloc( this_view.tags, size );
	if( !tags ) {
		this_view.n_tags -= 1;
		return;
	}
	tags[this_view.n_tags - 1] = tag;
	tags[this_view.n_tags] = NULL;
	pack = NEW( TagInfoPackRec, 1 );
	box = LCUIWidget_New( NULL );
	txt_name = LCUIWidget_New( "textview" );
	btn_del = LCUIWidget_New( "textview" );
	Widget_AddClass( box, "tag-list-item" );
	Widget_AddClass( btn_del, "btn-delete mdi mdi-close" );
	TextView_SetText( txt_name, tag->name );
	Widget_Append( box, txt_name );
	Widget_Append( box, btn_del );
	Widget_Append( this_view.view_tags, box );
	pack->widget = box;
	pack->tag = tag;
	this_view.tags = tags;
	Widget_BindEvent( btn_del, "click", OnBtnDeleteTagClick, pack, free );
}

static void OnSetRating( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	int rating = StarRating_GetRating( w );
	DBFile_SetScore( this_view.file, rating );
}

static void OnBtnHideClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	Widget_Hide( this_view.panel );
}

static void OnBtnOpenDirClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	wopenfilemanger( this_view.filepath );
}

static void OnBtnAddTagClick( LCUI_Widget w, LCUI_WidgetEvent e, void *arg )
{
	DB_Tag tag;
	char *tagname;
	int i, len, ret;
	wchar_t text[256];
	LCUI_Widget window = LCUIWidget_GetById( ID_WINDOW_PCITURE_VIEWER );
	ret = LCUIDialog_Prompt( window, DIALOG_TITLE_ADDTAG, 
				DIALOG_INPUT_PLACEHOLDER, NULL, 
				text, 255, CheckTagName );
	if( ret != 0 ) {
		return;
	}
	len = LCUI_EncodeString( NULL, text, 0, ENCODING_UTF8 ) + 1;
	tagname = NEW( char, len );
	LCUI_EncodeString( tagname, text, len, ENCODING_UTF8 );
	tag = LCFinder_GetTag( tagname );
	if( tag ) {
		for( i = 0; i < this_view.n_tags; ++i ) {
			if( this_view.tags[i]->id == tag->id ) {
				free( tagname );
				return;
			}
		}
	} else {
		tag = LCFinder_AddTag( tagname );
	}
	if( tag ) {
		tag->count += 1;
	}
	DBFile_AddTag( this_view.file, tag );
	LCFinder_TriggerEvent( EVENT_TAG_UPDATE, tag );
	PictureInfo_AppendTag( tag );
	free( tagname );
}

void UI_InitPictureInfoView( void )
{
	LCUI_Widget box, parent, btn_hide, btn_add_tag, btn_open;
	box = LCUIBuilder_LoadFile( XML_PATH );
	if( !box ) {
		return;
	}
	Widget_Append( this_view.panel, box );
	Widget_Unwrap( box );
	this_view.n_tags = 0;
	this_view.tags = NULL;
	this_view.filepath = NULL;
	parent = LCUIWidget_GetById( ID_WINDOW_PCITURE_VIEWER );
	btn_hide = LCUIWidget_GetById( ID_BTN_HIDE_PICTURE_INFO );
	btn_open = LCUIWidget_GetById( ID_BTN_OPEN_PICTURE_DIR );
	btn_add_tag = LCUIWidget_GetById( ID_BTN_ADD_PICTURE_TAG );
	this_view.txt_fsize = LCUIWidget_GetById( ID_TXT_PICTURE_FILE_SIZE );
	this_view.txt_size = LCUIWidget_GetById( ID_TXT_PICTURE_SIZE );
	this_view.txt_name = LCUIWidget_GetById( ID_TXT_PICTURE_NAME );
	this_view.txt_dirpath = LCUIWidget_GetById( ID_TXT_PICTURE_PATH );
	this_view.txt_time = LCUIWidget_GetById( ID_TXT_PICTURE_TIME );
	this_view.panel = LCUIWidget_GetById( ID_PANEL_PICTURE_INFO );
	this_view.view_tags = LCUIWidget_GetById( ID_VIEW_PICTURE_TAGS );
	this_view.window = LCUIWidget_GetById( ID_WINDOW_PCITURE_VIEWER );
	this_view.rating = LCUIWidget_GetById( ID_VIEW_PCITURE_RATING );
	Widget_BindEvent( this_view.rating, "click", OnSetRating, NULL, NULL );
	Widget_BindEvent( btn_hide, "click", OnBtnHideClick, NULL, NULL );
	Widget_BindEvent( btn_add_tag, "click", OnBtnAddTagClick, NULL, NULL );
	Widget_BindEvent( btn_open, "click", OnBtnOpenDirClick, NULL, NULL );
	Widget_Append( parent, this_view.panel );
	Widget_Hide( this_view.panel );
}

void UI_SetPictureInfoView( const char *filepath )
{
	size_t len;
	char *apath;
	DB_Tag *tags;
	int i, n, width, height;
	wchar_t str[256], *path, *dirpath;
	len = strlen( filepath ) + 1;
	apath = malloc( sizeof( wchar_t ) * len );
	path = malloc( sizeof( wchar_t ) * len );
	dirpath = malloc( sizeof( wchar_t ) * len );
	LCUI_DecodeString( path, filepath, len, ENCODING_UTF8 );
	LCUI_EncodeString( apath, path, len, ENCODING_ANSI );
	TextView_SetTextW( this_view.txt_name, wgetfilename( path ) );
	wgetdirpath( dirpath, len, path );
	TextView_SetTextW( this_view.txt_dirpath, dirpath );
	this_view.mtime = wgetfilemtime( path );
	wgettimestr( str, 256, this_view.mtime );
	TextView_SetTextW( this_view.txt_time, str );
	Graph_GetImageSize( apath, &width, &height );
	swprintf( str, 256, L"%dx%d", width, height );
	TextView_SetTextW( this_view.txt_size, str );
	this_view.width = width;
	this_view.height = height;
	this_view.size = wgetfilesize( path );
	getsizestr( str, 256, this_view.size );
	TextView_SetTextW( this_view.txt_fsize, str );
	if( this_view.filepath ) {
		free( this_view.filepath );
	}
	free( this_view.tags );
	this_view.n_tags = 0;
	this_view.tags = NULL;
	this_view.filepath = path;
	this_view.file = DB_GetFile( filepath );
	Widget_Empty( this_view.view_tags );
	/* 当没有文件记录时，说明该文件并不是源文件夹内的文件，暂时禁用部分操作 */
	if( !this_view.file ) {
		Widget_Hide( this_view.view_tags->parent );
		Widget_Hide( this_view.rating->parent );
		return;
	}
	Widget_Show( this_view.view_tags->parent );
	Widget_Show( this_view.rating->parent );
	StarRating_SetRating( this_view.rating, this_view.file->score );
	n = LCFinder_GetFileTags( this_view.file, &tags );
	for( i = 0; i < n; ++i ) {
		PictureInfo_AppendTag( tags[i] );
	}
	/* 因为标签记录是引用自 finder.tags 里的，所以这里只需要释放 tags */
	if( n > 0 ) {
		free( tags );
	}
}

void UI_ShowPictureInfoView( void )
{
	Widget_Show( this_view.panel );
}

void UI_HidePictureInfoView( void )
{
	Widget_Hide( this_view.panel );
}
