#ifndef AGENDA_STORAGE_H
#define AGENDA_STORAGE_H

#include "types.h"

/* Ensures directory (and intermediate parents) exist on disk */
BOOL Storage_EnsureDirectory(const WCHAR* pszDirPath);

/* Loads events from <DataFolder>\events.json */
BOOL Storage_LoadEvents(APP_SETTINGS* pSettings, EVENT_LIST* pList);

/* Saves events atomically to events.json and creates a timestamped snapshot in versions/ */
BOOL Storage_SaveEvents(APP_SETTINGS* pSettings, const EVENT_LIST* pList);

/* Counts existing JSON snapshot files inside <DataFolder>\versions */
DWORD Storage_CountSnapshots(const WCHAR* pszDataFolder);

/* Opens native Windows folder picker dialog */
BOOL Storage_PickFolderDialog(HWND hOwner, const WCHAR* pszInitialFolder, WCHAR* pszOutFolder, DWORD cchOut);

#endif /* AGENDA_STORAGE_H */
