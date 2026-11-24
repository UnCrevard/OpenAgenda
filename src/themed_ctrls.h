#ifndef AGENDA_THEMED_CTRLS_H
#define AGENDA_THEMED_CTRLS_H

#include "types.h"

#define AGENDA_THEMED_DATE_CLASS  L"AgendaWin32DatePickerClass"
#define AGENDA_THEMED_YEAR_CLASS  L"AgendaWin32YearPickerClass"
#define AGENDA_THEMED_TIME_CLASS  L"AgendaWin32TimePickerClass"
#define AGENDA_THEMED_COMBO_CLASS L"AgendaWin32ComboClass"
#define AGENDA_THEMED_MSGBOX_CLASS L"AgendaWin32MsgBoxClass"

/* Custom notification codes sent via WM_COMMAND (HIWORD(wParam)) */
#define TCN_DATE_CHANGED  0x1101
#define TCN_TIME_CHANGED  0x1102
#define TCN_YEAR_CHANGED  0x1103

/* Registers all custom themed control classes */
BOOL ThemedCtrls_Register(HINSTANCE hInst);

/* Custom themed Date Picker (100% Light/Dark themed popup calendar with Month & Year pickers) */
HWND ThemedDatePicker_Create(
    HWND hwndParent, HINSTANCE hInst, const APP_THEME* pTheme,
    HFONT hFont, UINT uDpi, int x, int y, int w, int h, int nId,
    const SYSTEMTIME* pstInitial
);
void ThemedDatePicker_GetDate(HWND hwnd, SYSTEMTIME* pstOut);
void ThemedDatePicker_SetDate(HWND hwnd, const SYSTEMTIME* pst);

/* Custom themed Year Picker (scrollable year dropdown + wheel + 4-digit keyboard entry) */
HWND ThemedYearPicker_Create(
    HWND hwndParent, HINSTANCE hInst, const APP_THEME* pTheme,
    HFONT hFont, UINT uDpi, int x, int y, int w, int h, int nId,
    WORD wYear
);
WORD ThemedYearPicker_GetYear(HWND hwnd);
void ThemedYearPicker_SetYear(HWND hwnd, WORD wYear);

/* Custom themed Time Picker (15-min easy dropdown + wheel + keyboard digits) */
HWND ThemedTimePicker_Create(
    HWND hwndParent, HINSTANCE hInst, const APP_THEME* pTheme,
    HFONT hFont, UINT uDpi, int x, int y, int w, int h, int nId,
    WORD wHour, WORD wMinute
);
void ThemedTimePicker_GetTime(HWND hwnd, WORD* pwHour, WORD* pwMinute);
void ThemedTimePicker_SetTime(HWND hwnd, WORD wHour, WORD wMinute);

/* Custom themed ComboBox (100% Light/Dark closed box & popup list) */
HWND ThemedCombo_Create(
    HWND hwndParent, HINSTANCE hInst, const APP_THEME* pTheme,
    HFONT hFont, UINT uDpi, int x, int y, int w, int h, int nId
);
void ThemedCombo_AddItem(HWND hwnd, const WCHAR* pszText, COLORREF crSwatch, BOOL bHasSwatch);
void ThemedCombo_SetCurSel(HWND hwnd, LONG lIndex);
LONG ThemedCombo_GetCurSel(HWND hwnd);

/* 100% Light/Dark themed modal confirmation / alert dialog */
INT Themed_MessageBox(
    HWND hwndOwner, HINSTANCE hInst, const APP_THEME* pTheme,
    const WCHAR* pszTitle, const WCHAR* pszMessage, BOOL bYesNo
);

#endif /* AGENDA_THEMED_CTRLS_H */
