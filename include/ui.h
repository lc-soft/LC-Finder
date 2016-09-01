/* ***************************************************************************
* ui.h -- ui managment module
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
* ui.h -- 图形界面管理模块
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

#ifndef LCFINDER_UI_H
#define LCFINDER_UI_H

/* 一些元素的 ID，命名格式为：ID_类型_名称 */
#define ID_WINDOW_MAIN			"main-window"
#define ID_WINDOW_PCITURE_VIEWER	"picture-viewer-window"
#define ID_PANEL_PICTURE_INFO		"picture-info-panel"
#define ID_TXT_VIEW_HOME_TITLE		"view-home-title"
#define ID_TXT_PICTURE_NAME		"picture-info-name"
#define ID_TXT_PICTURE_TIME		"picture-info-time"
#define ID_TXT_PICTURE_FILE_SIZE	"picture-info-file-size"
#define ID_TXT_PICTURE_SIZE		"picture-info-size"
#define ID_TXT_PICTURE_PATH		"picture-info-path"
#define ID_TXT_FILE_SYNC_STATS		"file-sync-tip-stats"
#define ID_TXT_FILE_SYNC_TITLE		"file-sync-tip-title"
#define ID_TXT_THUMB_DB_SPACE_USAGE	"text-thumb-db-space-usage"
#define ID_VIEW_PICTURE_TAGS		"picture-info-tags"
#define ID_VIEW_PICTURE_TARGET		"picture-viewer-target"
#define ID_VIEW_PCITURE_RATING		"picture-info-rating"
#define ID_VIEW_PICTURE_LOADING_TIP	"picture-loading-tip"
#define ID_VIEW_MAIN_SIDEBAR		"main-sidebar"
#define ID_VIEW_FILE_LIST		"current-file-list"
#define ID_VIEW_HOME			"view-home"
#define ID_VIEW_HOME_COLLECTIONS	"home-collection-list"
#define ID_VIEW_FOLDERS			"view-folders"
#define ID_VIEW_FOLDER_INFO		"view-folders-info-box"
#define ID_VIEW_FOLDER_INFO_NAME	"view-folders-info-box-name"
#define ID_VIEW_FOLDER_INFO_PATH	"view-folders-info-box-path"
#define ID_VIEW_SOURCE_LIST		"current-source-list"
#define ID_VIEW_SEARCH_TAGS		"view-search-tags"
#define ID_VIEW_SEARCH_RESULT		"view-search-result"
#define ID_VIEW_SEARCH_FILES		"view-search-files"
#define ID_VIEW_HOME_PROGRESS		"view-home-progress"
#define ID_BTN_SEARCH_FILES		"btn-search-files"
#define ID_BTN_SYNC_HOME_FILES		"btn-sync-home-files"
#define ID_BTN_TAG_HOME_FILES		"btn-tag-home-files"
#define ID_BTN_DELETE_HOME_FILES	"btn-delete-home-files"
#define ID_BTN_SELECT_HOME_FILES	"btn-select-home-files"
#define ID_BTN_CANCEL_HOME_SELECT	"btn-cancel-home-select"
#define ID_BTN_RETURN_ROOT_FOLDER	"btn-return-root-folder"
#define ID_BTN_SYNC_FOLDER_FILES	"btn-sync-folder-files"
#define ID_BTN_ADD_PICTURE_TAG		"btn-add-picture-tag"
#define ID_BTN_OPEN_PICTURE_DIR		"btn-open-picture-dir"
#define ID_BTN_HIDE_PICTURE_INFO	"btn-hide-picture-info"
#define ID_BTN_SHOW_PICTURE_INFO	"btn-show-picture-info"
#define ID_BTN_DELETE_PICTURE		"btn-delete-picture"
#define ID_BTN_HIDE_PICTURE_VIEWER	"btn-hide-picture-viewer"
#define ID_BTN_PICTURE_RESET_SIZE	"btn-picture-reset-size"
#define ID_BTN_PICTURE_ZOOM_IN		"btn-picture-zoom-in"
#define ID_BTN_PICTURE_ZOOM_OUT		"btn-picture-zoom-out"
#define ID_BTN_PCITURE_PREV		"btn-picture-prev"
#define ID_BTN_PCITURE_NEXT		"btn-picture-next"
#define ID_BTN_ADD_SOURCE		"btn-add-source"
#define ID_BTN_SIDEBAR_SETTINGS		"sidebar-btn-settings"
#define ID_BTN_SIDEBAR_SEEARCH		"sidebar-btn-search"
#define ID_BTN_CLEAR_THUMB_DB		"btn-clear-thumb-db"
#define ID_BTN_OPEN_LICENSE		"btn-open-license"
#define ID_BTN_OPEN_WEBSITE		"btn-open-website"
#define ID_BTN_OPEN_FEEDBACK		"btn-open-feedback"
#define ID_BTN_HIDE_SEARCH_RESULT	"btn-hide-search-result"
#define ID_BTN_OPEN_PICTURE_DIR		"btn-open-picture-dir"
#define ID_TIP_HOME_EMPTY		"tip-empty-collection"
#define ID_TIP_FOLDERS_EMPTY		"tip-empty-folder"
#define ID_TIP_SEARCH_TAGS_EMPTY	"tip-search-tags-empty"
#define ID_TIP_SEARCH_FILES_EMPTY	"tip-search-no-result"
#define ID_INPUT_SEARCH			"input-search"

/** 文件迭代器 */
typedef struct FileIteratorRec_* FileIterator;
typedef struct FileIteratorRec_ {
	unsigned int index;		/**< 当前索引位置 */
	unsigned int length;		/**< 文件列表总长度 */
	void *privdata;			/**< 私有数据 */
	char *filepath;			/**< 文件路径 */
	void (*next)(FileIterator);	/**< 切换至下一个文件 */
	void (*prev)(FileIterator);	/**< 切换至上一个文件 */
	void (*destroy)(FileIterator);	/**< 销毁文件迭代器 */
} FileIteratorRec;

/** 初始化用户界面 */
void UI_Init( void );

void UI_Exit( void );

/** 让用户界面开始工作 */
int UI_Run( void );

/** 初始化侧边栏 */
void UI_InitSidebar( void );

/** 初始化“设置”视图 */
void UI_InitSettingsView( void );

/** 初始化“文件夹”视图 */
void UI_InitFoldersView( void );

void UI_ExitFolderView( void );

/** 初始化文件同步时的提示框 */
void UI_InitFileSyncTip( void );

/** 初始化首页集锦视图 */
void UI_InitHomeView( void );

void UI_ExitHomeView( void );

/** 初始化图片视图 */
void UI_InitPictureView( void );

/** 退出图片视图并销毁相关资源 */
void UI_ExitPictureView( void );

/** 在图片视图中打开一张图片 */
void UI_OpenPictureView( const char *filepath );

/**
 * 为图片视图设置相关数据
 * @param[in] iter 文件迭代器，用于图片的上一张/下一张的切换功能
 */
void UI_SetPictureView( FileIterator iter );

/** 关闭图片视图 */
void UI_ClosePictureView( void );

/** 初始化图片信息视图 */
void UI_InitPictureInfoView( void );

/** 设置图片信息视图 */
void UI_SetPictureInfoView( const char *filepath );

/** 显示图片信息视图 */
void UI_ShowPictureInfoView( void );

/** 隐藏图片信息视图 */
void UI_HidePictureInfoView( void );

void UI_UpdateSearchView( void );

void UI_InitSearchView( void );

#endif
