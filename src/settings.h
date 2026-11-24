#ifndef AGENDA_SETTINGS_H
#define AGENDA_SETTINGS_H

#include "types.h"

/* Initializes INI path in the current directory and default data folder */
void Settings_InitDefaults(APP_SETTINGS* pSettings);

/* Loads and saves settings strictly to the local INI file (no registry writes) */
BOOL Settings_Load(APP_SETTINGS* pSettings);
BOOL Settings_Save(const APP_SETTINGS* pSettings);

/* Manages Windows Startup .lnk shortcut in shell:startup */
BOOL Settings_IsStartupShortcutPresent(void);
BOOL Settings_SyncStartupShortcut(BOOL bEnable);

#endif /* AGENDA_SETTINGS_H */
