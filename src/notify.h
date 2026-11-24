#ifndef AGENDA_NOTIFY_H
#define AGENDA_NOTIFY_H

#include "types.h"

/* Tracks fired notifications in memory */
void FiredList_Init(FIRED_NOTIFY_LIST* pList);
void FiredList_Free(FIRED_NOTIFY_LIST* pList);

/* System tray lifecycle */
BOOL Notify_InitTray(HWND hwnd, HICON hIcon);
BOOL Notify_UpdateTrayIcon(HWND hwnd, HICON hIcon);
void Notify_RemoveTray(HWND hwnd);

/* Shows a native Windows tray balloon/toast notification and optional sound */
BOOL Notify_ShowBalloon(
    HWND hwnd,
    const WCHAR* pszTitle,
    const WCHAR* pszDescription,
    const APP_SETTINGS* pSettings
);

/* Plays the configured notification sound */
void Notify_PlaySound(const APP_SETTINGS* pSettings);

/* Periodic timer check for due event notifications */
void Notify_CheckDueEvents(
    HWND hwnd,
    const EVENT_LIST* pEvents,
    const APP_SETTINGS* pSettings,
    FIRED_NOTIFY_LIST* pFired
);

#endif /* AGENDA_NOTIFY_H */
