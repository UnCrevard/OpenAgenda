#ifndef AGENDA_DIALOGS_H
#define AGENDA_DIALOGS_H

#include "types.h"

/* Registers modal dialog window classes */
BOOL Dialogs_Register(HINSTANCE hInst);

/* Opens modal Event Editor dialog. Returns TRUE if saved or deleted. */
BOOL Dialog_ShowEventEditor(
    HWND hwndOwner,
    HINSTANCE hInst,
    const APP_THEME* pTheme,
    CALENDAR_EVENT* pEvent,
    BOOL bIsEdit,
    BOOL* pbDeleted
);

/* Opens modal Settings dialog. Returns TRUE if user clicked Save. */
BOOL Dialog_ShowSettings(
    HWND hwndOwner,
    HINSTANCE hInst,
    const APP_THEME* pTheme,
    APP_SETTINGS* pSettings,
    BOOL* pbDataFolderChanged
);

#endif /* AGENDA_DIALOGS_H */
