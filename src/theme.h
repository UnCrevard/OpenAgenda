#ifndef AGENDA_THEME_H
#define AGENDA_THEME_H

#include "types.h"

/* Enables process-wide dark mode support in uxtheme.dll on Windows 10/11 */
void Theme_InitProcessDarkMode(BOOL bDarkMode);

/* Reads Windows Personalize AppsUseLightTheme (read-only) */
BOOL Theme_IsSystemDarkMode(void);

/* Populates color palette for Light or Dark mode */
void Theme_BuildPalette(APP_THEME* pTheme, BOOL bDarkMode);

/* Applies DWM immersive dark title bar and uxtheme dark mode to a window */
void Theme_ApplyToWindow(HWND hwnd, BOOL bDarkMode);

/* Applies DarkMode_Explorer / DarkMode_CFD theme to child controls */
void Theme_ApplyToControl(HWND hwndCtrl, BOOL bDarkMode, BOOL bIsCfd);

/* Creates a crisp programmatic calendar icon (16x16 or 32x32) */
HICON Theme_CreateCalendarIcon(DWORD dwSize, BOOL bDarkMode);

/* Configures and retrieves the application's default font face and base point size */
void Theme_SetDefaultFont(const WCHAR* pszFaceName, DWORD dwBaseSizePt);
void Theme_GetDefaultFont(WCHAR* pszFaceNameOut, DWORD cchOut, DWORD* pdwBaseSizePtOut);

/* Creates a ClearType font using the active default font + relative point offset */
HFONT Theme_CreateAppFont(UINT uDpi, LONG lDeltaPt, int nWeight, BOOL bItalic);

#endif /* AGENDA_THEME_H */
