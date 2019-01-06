#ifndef LCFINDER_SETTINGS_VIEW_H
#define LCFINDER_SETTINGS_VIEW_H

LCUI_Widget SourceListItem_Create(DB_Dir dir);

void SettingsView_InitPrivateSpace(void);
void SettingsView_InitThumbCache(void);
void SettingsView_InitLanguage(void);
void SettingsView_InitSource(void);
void SettingsView_InitLicense(void);
void SettingsView_InitScaling(void);
void SettingsView_InitDetector(void);

#endif
