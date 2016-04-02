
#ifndef __TILEITEM_H__
#define __TILEITEM_H__

/** 新建文件夹磁贴项 */
LCUI_Widget CreateFolderItem( const char *filepath );

/** 新建图片磁贴项 */
LCUI_Widget CreatePictureItem( ThumbCache cache, const char *filepath );

#endif
