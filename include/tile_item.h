
#ifndef __TILE_ITEM_H__
#define __TILE_ITEM_H__

/** 新建文件夹磁贴项 */
LCUI_Widget CreateFolderItem( const char *filepath );

/** 新建图片磁贴项 */
LCUI_Widget CreatePictureItem( const char *filepath, LCUI_Graph *pic );

#endif
