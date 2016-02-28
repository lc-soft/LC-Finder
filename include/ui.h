#ifndef __UI_H__
#define __UI_H__

/** 初始化用户界面 */
void UI_Init( void );

/** 让用户界面开始工作 */
int UI_Run( void );

/** 初始化侧边栏 */
void UI_InitSidebar( void );

/** 初始化“设置”视图 */
void UI_InitSettingsView( void );

/** 初始化“文件夹”视图 */
void UI_InitFoldersView( void );

/** 初始化文件同步时的提示框 */
void UI_InitFileSyncTip( void );

#endif
