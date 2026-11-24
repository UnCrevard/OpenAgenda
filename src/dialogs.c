#include "dialogs.h"
#include "date_utils.h"
#include "notify.h"
#include "settings.h"
#include "storage.h"
#include "theme.h"
#include "themed_ctrls.h"

/* Control IDs for Event Editor */
#define IDC_EV_TITLE        2001
#define IDC_EV_DESC         2002
#define IDC_EV_DATE         2003
#define IDC_EV_TIME         2004
#define IDC_EV_ALLDAY       2005
#define IDC_EV_DUR_COMBO    2006
#define IDC_EV_DUR_CUSTOM   2007
#define IDC_EV_DUR_UNIT     2008
#define IDC_EV_COLOR        2009
#define IDC_EV_REC_TYPE     2010
#define IDC_EV_REC_INTERVAL 2011
#define IDC_EV_REC_INT_LBL  2012
#define IDC_EV_NTH_ORD      2013
#define IDC_EV_NTH_WD       2014
#define IDC_EV_NTH_OF_MONTH 2015
#define IDC_EV_HAS_END      2016
#define IDC_EV_END_DATE     2017
#define IDC_EV_NOTIFY_MODE  2018
#define IDC_EV_NOTIFY_MINS  2019
#define IDC_EV_NOTIFY_MLBL  2020
#define IDC_EV_BTN_SAVE     2021
#define IDC_EV_BTN_DELETE   2022
#define IDC_EV_BTN_CANCEL   2023
#define IDC_EV_YEAR         2024
#define IDC_EV_BDAY_INFO    2025
#define IDC_EV_WD_BASE      2030 /* 2030..2036 Mon..Sun */

/* Control IDs for Settings Dialog */
#define IDC_CFG_FOLDER      3001
#define IDC_CFG_BROWSE_DIR  3002
#define IDC_CFG_VER_INFO    3003
#define IDC_CFG_START_MIN   3004
#define IDC_CFG_MIN_TRAY    3005
#define IDC_CFG_START_WIN   3006
#define IDC_CFG_SOUND_EN    3007
#define IDC_CFG_SOUND_PATH  3008
#define IDC_CFG_BROWSE_WAV  3009
#define IDC_CFG_CLEAR_WAV   3010
#define IDC_CFG_TEST_NOTIFY 3011
#define IDC_CFG_BTN_SAVE    3012
#define IDC_CFG_BTN_CANCEL  3013
#define IDC_CFG_FONT_TEXT   3014
#define IDC_CFG_CHOOSE_FONT 3015
#define IDC_CFG_RESET_FONT  3016

#define PROP_CHECKED        L"AgendaChecked"
#define PROP_IS_CHECKBOX    L"AgendaIsCheckbox"
#define PROP_BTN_ROLE       L"AgendaBtnRole"

#define BTN_ROLE_DEFAULT    0
#define BTN_ROLE_PRIMARY    1
#define BTN_ROLE_DANGER     2

#define MAX_EDIT_FRAMES     8

static LONG DlgScale(UINT uDpi, LONG lPx)
{
    return MulDiv(lPx, (int)uDpi, 96);
}

static UINT GetWindowDpiSafe(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);
    UINT uDpi = 96;
    if (hdc) {
        uDpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hwnd, hdc);
    }
    return (uDpi > 0) ? uDpi : 96;
}

static HFONT CreateDialogFont(UINT uDpi, LONG lPt, int nWeight)
{
    return Theme_CreateAppFont(uDpi, lPt - 9, nWeight, FALSE);
}

static BOOL Dlg_IsChecked(HWND hwndCtrl)
{
    return (GetPropW(hwndCtrl, PROP_CHECKED) != NULL);
}

static void Dlg_SetChecked(HWND hwndCtrl, BOOL bChecked)
{
    SetPropW(hwndCtrl, PROP_CHECKED, (HANDLE)(INT_PTR)(bChecked ? 1 : 0));
    InvalidateRect(hwndCtrl, NULL, FALSE);
}

static void Dlg_ToggleChecked(HWND hwndCtrl)
{
    Dlg_SetChecked(hwndCtrl, !Dlg_IsChecked(hwndCtrl));
}

static BOOL Dlg_IsOwnerDrawCheckbox(HWND hwndCtrl)
{
    return (GetPropW(hwndCtrl, PROP_IS_CHECKBOX) != NULL);
}

static HWND MakeStaticCtrl(
    HWND hwndParent, HINSTANCE hInst, HFONT hFont,
    const WCHAR* pszText, DWORD dwStyle,
    int x, int y, int w, int h, int nId
)
{
    HWND hwnd = CreateWindowExW(
        0, L"STATIC", pszText, WS_CHILD | WS_VISIBLE | dwStyle,
        x, y, w, h, hwndParent, (HMENU)(INT_PTR)nId, hInst, NULL
    );
    if (hwnd) {
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)hFont, FALSE);
    }
    return hwnd;
}

/* Creates a borderless EDIT inset inside a themed frame painted by parent WM_PAINT */
static HWND MakeBorderedEdit(
    HWND hwndParent, HINSTANCE hInst, HFONT hFont, const APP_THEME* pTheme,
    const WCHAR* pszText, DWORD dwStyle,
    int x, int y, int w, int h, int nId, UINT uDpi,
    RECT* pFrameOut, HWND* phEditOut
)
{
    LONG lPadX = DlgScale(uDpi, 6);
    LONG lPadY = DlgScale(uDpi, 4);
    HWND hwnd = CreateWindowExW(
        0, L"EDIT", pszText, WS_CHILD | WS_VISIBLE | WS_TABSTOP | dwStyle,
        x + lPadX, y + lPadY, w - lPadX * 2, h - lPadY * 2,
        hwndParent, (HMENU)(INT_PTR)nId, hInst, NULL
    );
    if (hwnd) {
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)hFont, FALSE);
        Theme_ApplyToControl(hwnd, pTheme->bDarkMode, FALSE);
    }
    if (pFrameOut) {
        pFrameOut->left = x;
        pFrameOut->top = y;
        pFrameOut->right = x + w;
        pFrameOut->bottom = y + h;
    }
    if (phEditOut) {
        *phEditOut = hwnd;
    }
    return hwnd;
}

static HWND MakeThemedButton(
    HWND hwndParent, HINSTANCE hInst, HFONT hFont,
    const WCHAR* pszText, int x, int y, int w, int h, int nId, INT_PTR nRole
)
{
    HWND hwnd = CreateWindowExW(
        0, L"BUTTON", pszText,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x, y, w, h, hwndParent, (HMENU)(INT_PTR)nId, hInst, NULL
    );
    if (hwnd) {
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)hFont, FALSE);
        SetPropW(hwnd, PROP_BTN_ROLE, (HANDLE)nRole);
    }
    return hwnd;
}

static HWND MakeThemedCheckbox(
    HWND hwndParent, HINSTANCE hInst, HFONT hFont,
    const WCHAR* pszText, int x, int y, int w, int h, int nId, BOOL bInitialChecked
)
{
    HWND hwnd = CreateWindowExW(
        0, L"BUTTON", pszText,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x, y, w, h, hwndParent, (HMENU)(INT_PTR)nId, hInst, NULL
    );
    if (hwnd) {
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)hFont, FALSE);
        SetPropW(hwnd, PROP_IS_CHECKBOX, (HANDLE)(INT_PTR)1);
        SetPropW(hwnd, PROP_CHECKED, (HANDLE)(INT_PTR)(bInitialChecked ? 1 : 0));
    }
    return hwnd;
}

static void Dlg_DrawPushButton(const DRAWITEMSTRUCT* pDis, const APP_THEME* pT, HFONT hFont, UINT uDpi)
{
    HDC hdc = pDis->hDC;
    RECT rc = pDis->rcItem;
    WCHAR szText[128];
    INT_PTR nRole = (INT_PTR)GetPropW(pDis->hwndItem, PROP_BTN_ROLE);
    BOOL bPressed = (pDis->itemState & ODS_SELECTED) != 0;
    BOOL bFocused = (pDis->itemState & ODS_FOCUS) != 0;
    BOOL bDisabled = (pDis->itemState & ODS_DISABLED) != 0;
    COLORREF crBg;
    COLORREF crBorder;
    COLORREF crText;
    HBRUSH hBrBg;
    HBRUSH hBrFill;
    HPEN hPen;
    HBRUSH hOldBr;
    HPEN hOldPen;
    HFONT hOldFont;
    int nRad = DlgScale(uDpi, 6);

    if (bDisabled) {
        crBg = pT->crBgGridAlt;
        crBorder = pT->crBorderSubtle;
        crText = pT->crTextMuted;
    } else if (nRole == BTN_ROLE_PRIMARY) {
        crBg = bPressed ? pT->crBgSelected : pT->crAccent;
        crBorder = pT->crAccent;
        crText = pT->crAccentText;
    } else if (nRole == BTN_ROLE_DANGER) {
        crBg = bPressed ? pT->crEventBgTint[2] : pT->crBgSurface;
        crBorder = pT->crEventColors[2];
        crText = pT->crEventColors[2];
    } else {
        crBg = bPressed ? pT->crBgSelected : pT->crBgSurface;
        crBorder = bFocused ? pT->crAccent : pT->crBorderStrong;
        crText = pT->crTextPrimary;
    }

    hBrBg = CreateSolidBrush(pT->crBgWindow);
    FillRect(hdc, &rc, hBrBg);
    DeleteObject(hBrBg);

    hBrFill = CreateSolidBrush(crBg);
    hPen = CreatePen(PS_SOLID, bFocused ? 2 : 1, crBorder);
    hOldBr = (HBRUSH)SelectObject(hdc, hBrFill);
    hOldPen = (HPEN)SelectObject(hdc, hPen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, nRad, nRad);
    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrFill);
    DeleteObject(hPen);

    GetWindowTextW(pDis->hwndItem, szText, 128);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, crText);
    hOldFont = (HFONT)SelectObject(hdc, hFont);
    DrawTextW(hdc, szText, -1, &rc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    SelectObject(hdc, hOldFont);
}

static void Dlg_DrawCheckbox(const DRAWITEMSTRUCT* pDis, const APP_THEME* pT, HFONT hFont, UINT uDpi)
{
    HDC hdc = pDis->hDC;
    RECT rc = pDis->rcItem;
    WCHAR szText[160];
    BOOL bChecked = Dlg_IsChecked(pDis->hwndItem);
    BOOL bDisabled = (pDis->itemState & ODS_DISABLED) != 0;
    BOOL bFocused = (pDis->itemState & ODS_FOCUS) != 0;
    LONG lBoxSize = DlgScale(uDpi, 16);
    LONG lBoxY = rc.top + ((rc.bottom - rc.top) - lBoxSize) / 2;
    RECT rcBox = {rc.left + 1, lBoxY, rc.left + 1 + lBoxSize, lBoxY + lBoxSize};
    RECT rcText = {rcBox.right + DlgScale(uDpi, 8), rc.top, rc.right, rc.bottom};
    HBRUSH hBrBg = CreateSolidBrush(pT->crBgWindow);
    HBRUSH hBrBox;
    HPEN hPenBox;
    HBRUSH hOldBr;
    HPEN hOldPen;
    HFONT hOldFont;
    int nRad = DlgScale(uDpi, 4);

    FillRect(hdc, &rc, hBrBg);
    DeleteObject(hBrBg);

    if (bChecked) {
        hBrBox = CreateSolidBrush(bDisabled ? pT->crBorderStrong : pT->crAccent);
        hPenBox = CreatePen(PS_SOLID, 1, bDisabled ? pT->crBorderStrong : pT->crAccent);
    } else {
        hBrBox = CreateSolidBrush(pT->crBgSurface);
        hPenBox = CreatePen(PS_SOLID, 1, bFocused ? pT->crAccent : pT->crBorderStrong);
    }
    hOldBr = (HBRUSH)SelectObject(hdc, hBrBox);
    hOldPen = (HPEN)SelectObject(hdc, hPenBox);
    RoundRect(hdc, rcBox.left, rcBox.top, rcBox.right, rcBox.bottom, nRad, nRad);
    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrBox);
    DeleteObject(hPenBox);

    if (bChecked) {
        HPEN hPenTick = CreatePen(PS_SOLID, DlgScale(uDpi, 2), pT->crAccentText);
        HPEN hOT = (HPEN)SelectObject(hdc, hPenTick);
        MoveToEx(hdc, rcBox.left + DlgScale(uDpi, 3), rcBox.top + lBoxSize / 2, NULL);
        LineTo(hdc, rcBox.left + DlgScale(uDpi, 6), rcBox.bottom - DlgScale(uDpi, 4));
        LineTo(hdc, rcBox.right - DlgScale(uDpi, 3), rcBox.top + DlgScale(uDpi, 4));
        SelectObject(hdc, hOT);
        DeleteObject(hPenTick);
    }

    GetWindowTextW(pDis->hwndItem, szText, 160);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, bDisabled ? pT->crTextMuted : pT->crTextPrimary);
    hOldFont = (HFONT)SelectObject(hdc, hFont);
    DrawTextW(hdc, szText, -1, &rcText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(hdc, hOldFont);

    if (bFocused) {
        RECT rcFocus = rcText;
        InflateRect(&rcFocus, 1, -1);
        DrawFocusRect(hdc, &rcFocus);
    }
}

static void Dlg_PaintEditFrames(HDC hdc, const APP_THEME* pT, const RECT* pFrames, const HWND* phEdits, DWORD dwCount, UINT uDpi)
{
    DWORD i;
    int nRad = DlgScale(uDpi, 5);
    HBRUSH hBrSurf = CreateSolidBrush(pT->crBgSurface);
    HPEN hPenBd = CreatePen(PS_SOLID, 1, pT->crBorderStrong);
    HBRUSH hOB = (HBRUSH)SelectObject(hdc, hBrSurf);
    HPEN hOP = (HPEN)SelectObject(hdc, hPenBd);

    for (i = 0; i < dwCount; ++i) {
        if (phEdits[i] && IsWindowVisible(phEdits[i])) {
            RoundRect(hdc, pFrames[i].left, pFrames[i].top, pFrames[i].right, pFrames[i].bottom, nRad, nRad);
        }
    }

    SelectObject(hdc, hOB);
    SelectObject(hdc, hOP);
    DeleteObject(hBrSurf);
    DeleteObject(hPenBd);
}

/* ========================================================================= */
/* EVENT EDITOR MODAL DIALOG                                                 */
/* ========================================================================= */

typedef struct _EVENT_DLG_STATE {
    HWND             hwnd;
    HWND             hwndOwner;
    HINSTANCE        hInst;
    const APP_THEME* pTheme;
    CALENDAR_EVENT*  pEvent;
    BOOL             bIsEdit;
    BOOL             bSaved;
    BOOL             bDeleted;
    BOOL             bClosed;
    UINT             uDpi;
    HFONT            hFont;
    HFONT            hFontBold;
    HBRUSH           hBrBg;
    HBRUSH           hBrEdit;
    RECT             rcEditFrames[MAX_EDIT_FRAMES];
    HWND             hEditCtrls[MAX_EDIT_FRAMES];
    DWORD            dwEditFrameCount;
} EVENT_DLG_STATE;

static void EventDlg_UpdateBirthdayPreview(EVENT_DLG_STATE* pS)
{
    HWND hDlg = pS->hwnd;
    LONG nRecType = ThemedCombo_GetCurSel(GetDlgItem(hDlg, IDC_EV_REC_TYPE));
    SYSTEMTIME stDate, stNow;
    WCHAR szInfo[160];
    LONG lAge;

    if (nRecType != REC_BIRTHDAY) {
        return;
    }

    ThemedDatePicker_GetDate(GetDlgItem(hDlg, IDC_EV_DATE), &stDate);
    GetLocalTime(&stNow);
    lAge = (LONG)stNow.wYear - (LONG)stDate.wYear;

    if (lAge > 0) {
        _snwprintf(szInfo, 160,
                   L"\u00C2ge calcul\u00E9 : %ld %s en %u",
                   (long)lAge, (lAge == 1) ? L"an" : L"ans", (UINT)stNow.wYear);
    } else if (lAge == 0) {
        _snwprintf(szInfo, 160,
                   L"\u00C2ge calcul\u00E9 : Naissance en %u",
                   (UINT)stNow.wYear);
    } else {
        _snwprintf(szInfo, 160,
                   L"Ann\u00E9e de naissance future (%u)",
                   (UINT)stDate.wYear);
    }
    szInfo[159] = L'\0';
    SetWindowTextW(GetDlgItem(hDlg, IDC_EV_BDAY_INFO), szInfo);
}

static void EventDlg_UpdateDynamicControls(EVENT_DLG_STATE* pS)
{
    HWND hDlg = pS->hwnd;
    BOOL bAllDay = Dlg_IsChecked(GetDlgItem(hDlg, IDC_EV_ALLDAY));
    LONG nDurIdx = ThemedCombo_GetCurSel(GetDlgItem(hDlg, IDC_EV_DUR_COMBO));
    LONG nRecType = ThemedCombo_GetCurSel(GetDlgItem(hDlg, IDC_EV_REC_TYPE));
    BOOL bHasEnd = Dlg_IsChecked(GetDlgItem(hDlg, IDC_EV_HAS_END));
    LONG nNotifyIdx = ThemedCombo_GetCurSel(GetDlgItem(hDlg, IDC_EV_NOTIFY_MODE));
    BOOL bShowCustomDur = (!bAllDay && nDurIdx == 7);
    BOOL bShowCustomNotif = (nNotifyIdx == 9);
    BOOL bShowInterval = (nRecType != REC_NONE && nRecType != REC_BIRTHDAY);
    BOOL bShowEndDate = (nRecType != REC_NONE && nRecType != REC_BIRTHDAY);
    int i;

    EnableWindow(GetDlgItem(hDlg, IDC_EV_TIME), !bAllDay);
    EnableWindow(GetDlgItem(hDlg, IDC_EV_DUR_COMBO), !bAllDay);

    ShowWindow(GetDlgItem(hDlg, IDC_EV_DUR_CUSTOM), bShowCustomDur ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_EV_DUR_UNIT), bShowCustomDur ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_EV_COLOR), bShowCustomDur ? SW_HIDE : SW_SHOW);

    ShowWindow(GetDlgItem(hDlg, IDC_EV_REC_INT_LBL), bShowInterval ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_EV_REC_INTERVAL), bShowInterval ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_EV_HAS_END), bShowEndDate ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_EV_END_DATE), bShowEndDate ? SW_SHOW : SW_HIDE);
    EnableWindow(GetDlgItem(hDlg, IDC_EV_END_DATE), bShowEndDate && bHasEnd);

    for (i = 0; i < 7; ++i) {
        ShowWindow(GetDlgItem(hDlg, IDC_EV_WD_BASE + i), (nRecType == REC_WEEKLY) ? SW_SHOW : SW_HIDE);
    }

    ShowWindow(GetDlgItem(hDlg, IDC_EV_NTH_ORD), (nRecType == REC_MONTHLY_NTH) ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_EV_NTH_WD), (nRecType == REC_MONTHLY_NTH) ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_EV_NTH_OF_MONTH), (nRecType == REC_MONTHLY_NTH) ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_EV_BDAY_INFO), (nRecType == REC_BIRTHDAY) ? SW_SHOW : SW_HIDE);
    if (nRecType == REC_BIRTHDAY) {
        EventDlg_UpdateBirthdayPreview(pS);
    }

    ShowWindow(GetDlgItem(hDlg, IDC_EV_NOTIFY_MINS), bShowCustomNotif ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_EV_NOTIFY_MLBL), bShowCustomNotif ? SW_SHOW : SW_HIDE);

    InvalidateRect(hDlg, NULL, TRUE);
}

static BOOL EventDlg_SaveFromUi(EVENT_DLG_STATE* pS)
{
    HWND hDlg = pS->hwnd;
    CALENDAR_EVENT ev = *pS->pEvent;
    SYSTEMTIME stDate;
    WORD wHour = 9, wMin = 0;
    WCHAR szBuf[64];
    LONG nSel;
    int i;

    GetWindowTextW(GetDlgItem(hDlg, IDC_EV_TITLE), ev.szTitle, MAX_EVENT_TITLE);
    if (ev.szTitle[0] == L'\0') {
        Themed_MessageBox(hDlg, pS->hInst, pS->pTheme, AGENDA_APP_NAME,
                          L"Veuillez saisir un nom pour l'\u00E9v\u00E9nement.", FALSE);
        SetFocus(GetDlgItem(hDlg, IDC_EV_TITLE));
        return FALSE;
    }

    GetWindowTextW(GetDlgItem(hDlg, IDC_EV_DESC), ev.szDescription, MAX_EVENT_DESC);

    ThemedDatePicker_GetDate(GetDlgItem(hDlg, IDC_EV_DATE), &stDate);
    ThemedTimePicker_GetTime(GetDlgItem(hDlg, IDC_EV_TIME), &wHour, &wMin);

    ev.bAllDay = Dlg_IsChecked(GetDlgItem(hDlg, IDC_EV_ALLDAY));
    ev.stStart = stDate;
    if (ev.bAllDay) {
        ev.stStart.wHour = 0;
        ev.stStart.wMinute = 0;
        ev.stStart.wSecond = 0;
        ev.dwDurationMinutes = 1440;
    } else {
        ev.stStart.wHour = wHour;
        ev.stStart.wMinute = wMin;
        ev.stStart.wSecond = 0;

        nSel = ThemedCombo_GetCurSel(GetDlgItem(hDlg, IDC_EV_DUR_COMBO));
        switch (nSel) {
            case 0: ev.dwDurationMinutes = 15; break;
            case 1: ev.dwDurationMinutes = 30; break;
            case 2: ev.dwDurationMinutes = 45; break;
            case 3: ev.dwDurationMinutes = 60; break;
            case 4: ev.dwDurationMinutes = 90; break;
            case 5: ev.dwDurationMinutes = 120; break;
            case 6: ev.dwDurationMinutes = 180; break;
            default: {
                GetWindowTextW(GetDlgItem(hDlg, IDC_EV_DUR_CUSTOM), szBuf, 64);
                LONG lMins = _wtol(szBuf);
                ev.dwDurationMinutes = (lMins > 0) ? (DWORD)lMins : 60;
                break;
            }
        }
    }
    Date_NormalizeTime(&ev.stStart);

    nSel = ThemedCombo_GetCurSel(GetDlgItem(hDlg, IDC_EV_COLOR));
    ev.dwColorIndex = (nSel >= 0 && nSel < EVENT_COLOR_COUNT) ? (DWORD)nSel : 0;

    nSel = ThemedCombo_GetCurSel(GetDlgItem(hDlg, IDC_EV_REC_TYPE));
    ev.dwRecurrenceType = (nSel >= 0 && nSel <= REC_BIRTHDAY) ? (DWORD)nSel : REC_NONE;

    GetWindowTextW(GetDlgItem(hDlg, IDC_EV_REC_INTERVAL), szBuf, 64);
    {
        LONG lInt = _wtol(szBuf);
        ev.dwRecurrenceInterval = (ev.dwRecurrenceType == REC_BIRTHDAY) ? 1 : ((lInt >= 1) ? (DWORD)lInt : 1);
    }

    ev.dwWeekdaysMask = 0;
    for (i = 0; i < 7; ++i) {
        if (Dlg_IsChecked(GetDlgItem(hDlg, IDC_EV_WD_BASE + i))) {
            ev.dwWeekdaysMask |= (1U << i);
        }
    }
    if (ev.dwRecurrenceType == REC_WEEKLY && ev.dwWeekdaysMask == 0) {
        ev.dwWeekdaysMask = (1U << Date_MondayBasedDayOfWeek(&ev.stStart));
    }

    nSel = ThemedCombo_GetCurSel(GetDlgItem(hDlg, IDC_EV_NTH_ORD));
    ev.lNthOrdinal = (nSel == 4) ? -1 : (LONG)(nSel + 1);
    nSel = ThemedCombo_GetCurSel(GetDlgItem(hDlg, IDC_EV_NTH_WD));
    ev.dwNthWeekday = (nSel >= 0 && nSel < 7) ? (DWORD)((nSel + 1) % 7) : ev.stStart.wDayOfWeek;

    ev.bHasEndDate = (ev.dwRecurrenceType == REC_BIRTHDAY)
                     ? FALSE
                     : Dlg_IsChecked(GetDlgItem(hDlg, IDC_EV_HAS_END));
    ThemedDatePicker_GetDate(GetDlgItem(hDlg, IDC_EV_END_DATE), &ev.stEndDate);
    Date_NormalizeTime(&ev.stEndDate);

    nSel = ThemedCombo_GetCurSel(GetDlgItem(hDlg, IDC_EV_NOTIFY_MODE));
    switch (nSel) {
        case 0: ev.dwNotifyMode = NOTIFY_NONE; ev.dwNotifyMinutesBefore = 0; break;
        case 1: ev.dwNotifyMode = NOTIFY_IMMEDIATE; ev.dwNotifyMinutesBefore = 0; break;
        case 2: ev.dwNotifyMode = NOTIFY_BEFORE; ev.dwNotifyMinutesBefore = 5; break;
        case 3: ev.dwNotifyMode = NOTIFY_BEFORE; ev.dwNotifyMinutesBefore = 10; break;
        case 4: ev.dwNotifyMode = NOTIFY_BEFORE; ev.dwNotifyMinutesBefore = 15; break;
        case 5: ev.dwNotifyMode = NOTIFY_BEFORE; ev.dwNotifyMinutesBefore = 30; break;
        case 6: ev.dwNotifyMode = NOTIFY_BEFORE; ev.dwNotifyMinutesBefore = 60; break;
        case 7: ev.dwNotifyMode = NOTIFY_BEFORE; ev.dwNotifyMinutesBefore = 120; break;
        case 8: ev.dwNotifyMode = NOTIFY_BEFORE; ev.dwNotifyMinutesBefore = 1440; break;
        default: {
            GetWindowTextW(GetDlgItem(hDlg, IDC_EV_NOTIFY_MINS), szBuf, 64);
            LONG lNotify = _wtol(szBuf);
            if (lNotify <= 0) {
                ev.dwNotifyMode = NOTIFY_IMMEDIATE;
                ev.dwNotifyMinutesBefore = 0;
            } else {
                ev.dwNotifyMode = NOTIFY_BEFORE;
                ev.dwNotifyMinutesBefore = (DWORD)lNotify;
            }
            break;
        }
    }

    *pS->pEvent = ev;
    return TRUE;
}

static void AddEditFrame(EVENT_DLG_STATE* pS, HWND hEd, RECT rcFrame)
{
    if (pS->dwEditFrameCount < MAX_EDIT_FRAMES) {
        pS->hEditCtrls[pS->dwEditFrameCount] = hEd;
        pS->rcEditFrames[pS->dwEditFrameCount] = rcFrame;
        pS->dwEditFrameCount++;
    }
}

static LRESULT CALLBACK EventDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    EVENT_DLG_STATE* pS = (EVENT_DLG_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCs = (CREATESTRUCTW*)lParam;
            UINT d;
            int y;
            int xLbl, wLbl, xVal, wVal;
            HWND hCmb, hEd;
            RECT rcFr;
            const CALENDAR_EVENT* pEv;
            WCHAR szNum[32];
            int i;
            static const WCHAR* szColorNames[EVENT_COLOR_COUNT] = {
                L"Bleu", L"Vert", L"Rouge", L"Orange", L"Violet", L"Sarcelle", L"Rose", L"Gris"
            };

            pS = (EVENT_DLG_STATE*)pCs->lpCreateParams;
            pS->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pS);

            d = pS->uDpi;
            pS->hFont = CreateDialogFont(d, 9, FW_NORMAL);
            pS->hFontBold = CreateDialogFont(d, 9, FW_SEMIBOLD);
            pS->hBrBg = CreateSolidBrush(pS->pTheme->crBgWindow);
            pS->hBrEdit = CreateSolidBrush(pS->pTheme->crBgSurface);
            Theme_ApplyToWindow(hwnd, pS->pTheme->bDarkMode);

            pEv = pS->pEvent;
            xLbl = DlgScale(d, 20);
            wLbl = DlgScale(d, 160);
            xVal = DlgScale(d, 188);
            wVal = DlgScale(d, 516);
            y = DlgScale(d, 18);

            /* 1. Event Name */
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFontBold, L"Nom de l'\u00E9v\u00E9nement :",
                           SS_LEFT | SS_CENTERIMAGE, xLbl, y, wLbl, DlgScale(d, 28), 0);
            MakeBorderedEdit(hwnd, pS->hInst, pS->hFont, pS->pTheme, pEv->szTitle,
                             ES_AUTOHSCROLL, xVal, y, wVal, DlgScale(d, 28), IDC_EV_TITLE, d, &rcFr, &hEd);
            AddEditFrame(pS, hEd, rcFr);
            SendMessageW(hEd, EM_LIMITTEXT, MAX_EVENT_TITLE - 1, 0);
            y += DlgScale(d, 38);

            /* 2. Description */
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFontBold, L"Description\n(dans la notification) :",
                           SS_LEFT, xLbl, y + DlgScale(d, 2), wLbl, DlgScale(d, 42), 0);
            MakeBorderedEdit(hwnd, pS->hInst, pS->hFont, pS->pTheme, pEv->szDescription,
                             ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
                             xVal, y, wVal, DlgScale(d, 74), IDC_EV_DESC, d, &rcFr, &hEd);
            AddEditFrame(pS, hEd, rcFr);
            SendMessageW(hEd, EM_LIMITTEXT, MAX_EVENT_DESC - 1, 0);
            y += DlgScale(d, 86);

            /* 3. Easy Date Picker, Year Picker, Time Picker & All-day checkbox */
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFontBold, L"Date et heure :",
                           SS_LEFT | SS_CENTERIMAGE, xLbl, y, wLbl, DlgScale(d, 28), 0);
            ThemedDatePicker_Create(hwnd, pS->hInst, pS->pTheme, pS->hFont, d,
                                    xVal, y, DlgScale(d, 176), DlgScale(d, 28), IDC_EV_DATE, &pEv->stStart);
            ThemedYearPicker_Create(hwnd, pS->hInst, pS->pTheme, pS->hFont, d,
                                    xVal + DlgScale(d, 182), y, DlgScale(d, 78), DlgScale(d, 28),
                                    IDC_EV_YEAR, pEv->stStart.wYear);
            ThemedTimePicker_Create(hwnd, pS->hInst, pS->pTheme, pS->hFont, d,
                                    xVal + DlgScale(d, 268), y, DlgScale(d, 98), DlgScale(d, 28),
                                    IDC_EV_TIME, pEv->stStart.wHour, pEv->stStart.wMinute);
            MakeThemedCheckbox(hwnd, pS->hInst, pS->hFont, L"Journ\u00E9e enti\u00E8re",
                               xVal + DlgScale(d, 374), y, DlgScale(d, 142), DlgScale(d, 28),
                               IDC_EV_ALLDAY, pEv->bAllDay);
            y += DlgScale(d, 38);

            /* 4. Duration & Color */
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFontBold, L"Dur\u00E9e et couleur :",
                           SS_LEFT | SS_CENTERIMAGE, xLbl, y, wLbl, DlgScale(d, 28), 0);
            hCmb = ThemedCombo_Create(hwnd, pS->hInst, pS->pTheme, pS->hFont, d,
                                      xVal, y, DlgScale(d, 216), DlgScale(d, 28), IDC_EV_DUR_COMBO);
            ThemedCombo_AddItem(hCmb, L"15 minutes", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"30 minutes", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"45 minutes", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"1 heure", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"1 heure 30", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"2 heures", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"3 heures", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Personnalis\u00E9e (min)", 0, FALSE);

            {
                int nDurSel = 7;
                if (pEv->dwDurationMinutes == 15) nDurSel = 0;
                else if (pEv->dwDurationMinutes == 30) nDurSel = 1;
                else if (pEv->dwDurationMinutes == 45) nDurSel = 2;
                else if (pEv->dwDurationMinutes == 60) nDurSel = 3;
                else if (pEv->dwDurationMinutes == 90) nDurSel = 4;
                else if (pEv->dwDurationMinutes == 120) nDurSel = 5;
                else if (pEv->dwDurationMinutes == 180) nDurSel = 6;
                ThemedCombo_SetCurSel(hCmb, nDurSel);
            }

            _snwprintf(szNum, 32, L"%u", (UINT)(pEv->dwDurationMinutes > 0 ? pEv->dwDurationMinutes : 60));
            szNum[31] = L'\0';
            MakeBorderedEdit(hwnd, pS->hInst, pS->hFont, pS->pTheme, szNum, ES_NUMBER,
                             xVal + DlgScale(d, 226), y, DlgScale(d, 72), DlgScale(d, 28),
                             IDC_EV_DUR_CUSTOM, d, &rcFr, &hEd);
            AddEditFrame(pS, hEd, rcFr);
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFont, L"minutes",
                           SS_LEFT | SS_CENTERIMAGE,
                           xVal + DlgScale(d, 304), y, DlgScale(d, 80), DlgScale(d, 28), IDC_EV_DUR_UNIT);

            hCmb = ThemedCombo_Create(hwnd, pS->hInst, pS->pTheme, pS->hFont, d,
                                      xVal + DlgScale(d, 226), y, DlgScale(d, 290), DlgScale(d, 28), IDC_EV_COLOR);
            for (i = 0; i < EVENT_COLOR_COUNT; ++i) {
                ThemedCombo_AddItem(hCmb, szColorNames[i], pS->pTheme->crEventColors[i], TRUE);
            }
            ThemedCombo_SetCurSel(hCmb, (LONG)(pEv->dwColorIndex % EVENT_COLOR_COUNT));
            y += DlgScale(d, 40);

            /* 5. Recurrence Type & Interval */
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFontBold, L"R\u00E9p\u00E9tition :",
                           SS_LEFT | SS_CENTERIMAGE, xLbl, y, wLbl, DlgScale(d, 28), 0);
            hCmb = ThemedCombo_Create(hwnd, pS->hInst, pS->pTheme, pS->hFont, d,
                                      xVal, y, DlgScale(d, 320), DlgScale(d, 28), IDC_EV_REC_TYPE);
            ThemedCombo_AddItem(hCmb, L"Unique (sans r\u00E9p\u00E9tition)", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Tous les jours", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Toutes les semaines", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Tous les mois (m\u00EAme date)", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Tous les mois (ni\u00E8me jour)", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Tous les ans", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Anniversaire (annuel avec \u00E2ge)", 0, FALSE);
            ThemedCombo_SetCurSel(hCmb, (LONG)pEv->dwRecurrenceType);

            MakeStaticCtrl(hwnd, pS->hInst, pS->hFont, L"Intervalle :",
                           SS_RIGHT | SS_CENTERIMAGE,
                           xVal + DlgScale(d, 328), y, DlgScale(d, 114), DlgScale(d, 28), IDC_EV_REC_INT_LBL);
            _snwprintf(szNum, 32, L"%u", (UINT)(pEv->dwRecurrenceInterval >= 1 ? pEv->dwRecurrenceInterval : 1));
            szNum[31] = L'\0';
            MakeBorderedEdit(hwnd, pS->hInst, pS->hFont, pS->pTheme, szNum, ES_NUMBER,
                             xVal + DlgScale(d, 450), y, DlgScale(d, 66), DlgScale(d, 28),
                             IDC_EV_REC_INTERVAL, d, &rcFr, &hEd);
            AddEditFrame(pS, hEd, rcFr);
            y += DlgScale(d, 38);

            /* 6. Recurrence Sub-row (Weekly checkboxes OR Monthly Nth combos OR Birthday preview) */
            for (i = 0; i < 7; ++i) {
                WCHAR szWd[16];
                Date_GetWeekdayShort((DWORD)i, szWd, 16);
                MakeThemedCheckbox(hwnd, pS->hInst, pS->hFont, szWd,
                                   xVal + i * DlgScale(d, 74), y, DlgScale(d, 68), DlgScale(d, 26),
                                   IDC_EV_WD_BASE + i, (pEv->dwWeekdaysMask & (1U << i)) != 0);
            }

            hCmb = ThemedCombo_Create(hwnd, pS->hInst, pS->pTheme, pS->hFont, d,
                                      xVal, y, DlgScale(d, 160), DlgScale(d, 28), IDC_EV_NTH_ORD);
            ThemedCombo_AddItem(hCmb, L"Premier", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Deuxi\u00E8me", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Troisi\u00E8me", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Quatri\u00E8me", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Dernier", 0, FALSE);
            {
                int nOrdSel = (pEv->lNthOrdinal == -1) ? 4 : (int)(pEv->lNthOrdinal - 1);
                if (nOrdSel < 0 || nOrdSel > 4) nOrdSel = 0;
                ThemedCombo_SetCurSel(hCmb, nOrdSel);
            }

            hCmb = ThemedCombo_Create(hwnd, pS->hInst, pS->pTheme, pS->hFont, d,
                                      xVal + DlgScale(d, 168), y, DlgScale(d, 188), DlgScale(d, 28), IDC_EV_NTH_WD);
            for (i = 0; i < 7; ++i) {
                WCHAR szFull[32];
                Date_GetWeekdayFull((DWORD)((i + 1) % 7), szFull, 32);
                ThemedCombo_AddItem(hCmb, szFull, 0, FALSE);
            }
            {
                DWORD dwMonWd = (pEv->dwNthWeekday + 6) % 7;
                ThemedCombo_SetCurSel(hCmb, (LONG)dwMonWd);
            }
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFont, L"du mois",
                           SS_LEFT | SS_CENTERIMAGE,
                           xVal + DlgScale(d, 364), y, DlgScale(d, 90), DlgScale(d, 28), IDC_EV_NTH_OF_MONTH);

            MakeStaticCtrl(hwnd, pS->hInst, pS->hFont, L"",
                           SS_LEFT | SS_CENTERIMAGE,
                           xVal, y, wVal, DlgScale(d, 28), IDC_EV_BDAY_INFO);
            y += DlgScale(d, 36);

            /* 7. Recurrence End Date */
            MakeThemedCheckbox(hwnd, pS->hInst, pS->hFont, L"Date de fin de r\u00E9p\u00E9tition :",
                               xVal, y, DlgScale(d, 266), DlgScale(d, 28),
                               IDC_EV_HAS_END, pEv->bHasEndDate);
            ThemedDatePicker_Create(hwnd, pS->hInst, pS->pTheme, pS->hFont, d,
                                    xVal + DlgScale(d, 274), y, DlgScale(d, 190), DlgScale(d, 28),
                                    IDC_EV_END_DATE, &pEv->stEndDate);
            y += DlgScale(d, 40);

            /* 8. Notification Timing */
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFontBold, L"Notification :",
                           SS_LEFT | SS_CENTERIMAGE, xLbl, y, wLbl, DlgScale(d, 28), 0);
            hCmb = ThemedCombo_Create(hwnd, pS->hInst, pS->pTheme, pS->hFont, d,
                                      xVal, y, DlgScale(d, 270), DlgScale(d, 28), IDC_EV_NOTIFY_MODE);
            ThemedCombo_AddItem(hCmb, L"Aucune notification", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Imm\u00E9diate (\u00E0 l'heure)", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"5 minutes avant", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"10 minutes avant", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"15 minutes avant", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"30 minutes avant", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"1 heure avant", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"2 heures avant", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"1 jour avant", 0, FALSE);
            ThemedCombo_AddItem(hCmb, L"Personnalis\u00E9e (minutes)", 0, FALSE);

            {
                int nNotSel = 9;
                if (pEv->dwNotifyMode == NOTIFY_NONE) nNotSel = 0;
                else if (pEv->dwNotifyMode == NOTIFY_IMMEDIATE || pEv->dwNotifyMinutesBefore == 0) nNotSel = 1;
                else if (pEv->dwNotifyMinutesBefore == 5) nNotSel = 2;
                else if (pEv->dwNotifyMinutesBefore == 10) nNotSel = 3;
                else if (pEv->dwNotifyMinutesBefore == 15) nNotSel = 4;
                else if (pEv->dwNotifyMinutesBefore == 30) nNotSel = 5;
                else if (pEv->dwNotifyMinutesBefore == 60) nNotSel = 6;
                else if (pEv->dwNotifyMinutesBefore == 120) nNotSel = 7;
                else if (pEv->dwNotifyMinutesBefore == 1440) nNotSel = 8;
                ThemedCombo_SetCurSel(hCmb, nNotSel);
            }

            _snwprintf(szNum, 32, L"%u", (UINT)(pEv->dwNotifyMinutesBefore > 0 ? pEv->dwNotifyMinutesBefore : 15));
            szNum[31] = L'\0';
            MakeBorderedEdit(hwnd, pS->hInst, pS->hFont, pS->pTheme, szNum, ES_NUMBER,
                             xVal + DlgScale(d, 278), y, DlgScale(d, 72), DlgScale(d, 28),
                             IDC_EV_NOTIFY_MINS, d, &rcFr, &hEd);
            AddEditFrame(pS, hEd, rcFr);
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFont, L"min avant",
                           SS_LEFT | SS_CENTERIMAGE,
                           xVal + DlgScale(d, 358), y, DlgScale(d, 110), DlgScale(d, 28), IDC_EV_NOTIFY_MLBL);
            y += DlgScale(d, 52);

            /* 9. Action Buttons */
            if (pS->bIsEdit) {
                MakeThemedButton(hwnd, pS->hInst, pS->hFont, L"Supprimer",
                                 xLbl, y, DlgScale(d, 116), DlgScale(d, 34), IDC_EV_BTN_DELETE, BTN_ROLE_DANGER);
            }
            MakeThemedButton(hwnd, pS->hInst, pS->hFontBold, L"Enregistrer",
                             xVal + wVal - DlgScale(d, 236), y, DlgScale(d, 114), DlgScale(d, 34), IDC_EV_BTN_SAVE, BTN_ROLE_PRIMARY);
            MakeThemedButton(hwnd, pS->hInst, pS->hFont, L"Annuler",
                             xVal + wVal - DlgScale(d, 114), y, DlgScale(d, 114), DlgScale(d, 34), IDC_EV_BTN_CANCEL, BTN_ROLE_DEFAULT);

            EventDlg_UpdateDynamicControls(pS);
            SetFocus(GetDlgItem(hwnd, IDC_EV_TITLE));
            return 0;
        }
        case WM_ERASEBKGND:
            if (pS && pS->hBrBg) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                FillRect((HDC)wParam, &rc, pS->hBrBg);
                return 1;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pS && pS->pTheme) {
                Dlg_PaintEditFrames(hdc, pS->pTheme, pS->rcEditFrames, pS->hEditCtrls, pS->dwEditFrameCount, pS->uDpi);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DRAWITEM:
            if (pS && pS->pTheme) {
                const DRAWITEMSTRUCT* pDis = (const DRAWITEMSTRUCT*)lParam;
                if (pDis->CtlType == ODT_BUTTON) {
                    if (Dlg_IsOwnerDrawCheckbox(pDis->hwndItem)) {
                        Dlg_DrawCheckbox(pDis, pS->pTheme, pS->hFont, pS->uDpi);
                    } else {
                        INT_PTR nRole = (INT_PTR)GetPropW(pDis->hwndItem, PROP_BTN_ROLE);
                        Dlg_DrawPushButton(pDis, pS->pTheme, (nRole == BTN_ROLE_PRIMARY) ? pS->hFontBold : pS->hFont, pS->uDpi);
                    }
                    return TRUE;
                }
            }
            break;

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            if (pS && pS->pTheme) {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, pS->pTheme->crTextPrimary);
                SetBkColor(hdc, pS->pTheme->crBgWindow);
                return (LRESULT)pS->hBrBg;
            }
            break;

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            if (pS && pS->pTheme) {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, pS->pTheme->crTextPrimary);
                SetBkColor(hdc, pS->pTheme->crBgSurface);
                return (LRESULT)pS->hBrEdit;
            }
            break;

        case WM_COMMAND: {
            WORD wId = LOWORD(wParam);
            WORD wNotif = HIWORD(wParam);
            HWND hwndCtrl = (HWND)lParam;
            if (!pS) break;

            if (wId == IDC_EV_DATE && wNotif == TCN_DATE_CHANGED) {
                SYSTEMTIME stPicked;
                ThemedDatePicker_GetDate(GetDlgItem(hwnd, IDC_EV_DATE), &stPicked);
                ThemedYearPicker_SetYear(GetDlgItem(hwnd, IDC_EV_YEAR), stPicked.wYear);
                EventDlg_UpdateBirthdayPreview(pS);
                return 0;
            }
            if (wId == IDC_EV_YEAR && wNotif == TCN_YEAR_CHANGED) {
                SYSTEMTIME stPicked;
                WORD wNewYear = ThemedYearPicker_GetYear(GetDlgItem(hwnd, IDC_EV_YEAR));
                DWORD dwMaxD;
                ThemedDatePicker_GetDate(GetDlgItem(hwnd, IDC_EV_DATE), &stPicked);
                stPicked.wYear = wNewYear;
                dwMaxD = Date_DaysInMonth(stPicked.wYear, stPicked.wMonth);
                if (stPicked.wDay > dwMaxD) {
                    stPicked.wDay = (WORD)dwMaxD;
                }
                ThemedDatePicker_SetDate(GetDlgItem(hwnd, IDC_EV_DATE), &stPicked);
                EventDlg_UpdateBirthdayPreview(pS);
                return 0;
            }
            if (wNotif == BN_CLICKED && hwndCtrl && Dlg_IsOwnerDrawCheckbox(hwndCtrl)) {
                Dlg_ToggleChecked(hwndCtrl);
                if (wId == IDC_EV_ALLDAY || wId == IDC_EV_HAS_END) {
                    EventDlg_UpdateDynamicControls(pS);
                }
                return 0;
            }
            if ((wId == IDC_EV_DUR_COMBO || wId == IDC_EV_REC_TYPE || wId == IDC_EV_NOTIFY_MODE) && wNotif == CBN_SELCHANGE) {
                EventDlg_UpdateDynamicControls(pS);
                return 0;
            }
            if (wId == IDC_EV_BTN_SAVE) {
                if (EventDlg_SaveFromUi(pS)) {
                    pS->bSaved = TRUE;
                    pS->bClosed = TRUE;
                    EnableWindow(pS->hwndOwner, TRUE);
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            if (wId == IDC_EV_BTN_DELETE && pS->bIsEdit) {
                if (Themed_MessageBox(hwnd, pS->hInst, pS->pTheme, AGENDA_APP_NAME,
                                      L"Voulez-vous vraiment supprimer cet \u00E9v\u00E9nement ?", TRUE) == IDYES) {
                    pS->bDeleted = TRUE;
                    pS->bClosed = TRUE;
                    EnableWindow(pS->hwndOwner, TRUE);
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            if (wId == IDC_EV_BTN_CANCEL) {
                pS->bClosed = TRUE;
                EnableWindow(pS->hwndOwner, TRUE);
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            if (pS) {
                pS->bClosed = TRUE;
                EnableWindow(pS->hwndOwner, TRUE);
            }
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (pS) {
                if (pS->hFont) DeleteObject(pS->hFont);
                if (pS->hFontBold) DeleteObject(pS->hFontBold);
                if (pS->hBrBg) DeleteObject(pS->hBrBg);
                if (pS->hBrEdit) DeleteObject(pS->hBrEdit);
                pS->bClosed = TRUE;
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

/* ========================================================================= */
/* SETTINGS MODAL DIALOG                                                     */
/* ========================================================================= */

typedef struct _CFG_DLG_STATE {
    HWND             hwnd;
    HWND             hwndOwner;
    HINSTANCE        hInst;
    const APP_THEME* pTheme;
    APP_SETTINGS*    pSettings;
    BOOL             bSaved;
    BOOL             bFolderChanged;
    BOOL             bClosed;
    UINT             uDpi;
    HFONT            hFont;
    HFONT            hFontBold;
    HBRUSH           hBrBg;
    HBRUSH           hBrEdit;
    RECT             rcEditFrames[MAX_EDIT_FRAMES];
    HWND             hEditCtrls[MAX_EDIT_FRAMES];
    DWORD            dwEditFrameCount;
    WCHAR            szFontName[LF_FACESIZE];
    DWORD            dwFontSizePt;
} CFG_DLG_STATE;

static void CfgDlg_UpdateSnapshotInfo(CFG_DLG_STATE* pS)
{
    WCHAR szFolder[MAX_PATH];
    WCHAR szInfo[128];
    DWORD dwSnap;

    GetWindowTextW(GetDlgItem(pS->hwnd, IDC_CFG_FOLDER), szFolder, MAX_PATH);
    dwSnap = Storage_CountSnapshots(szFolder);
    _snwprintf(szInfo, 128, L"Historique dans versions\\ : %u snapshot(s) JSON enregistr\u00E9(s)", (UINT)dwSnap);
    szInfo[127] = L'\0';
    SetWindowTextW(GetDlgItem(pS->hwnd, IDC_CFG_VER_INFO), szInfo);
}

static void CfgDlg_UpdateFontDisplay(CFG_DLG_STATE* pS)
{
    WCHAR szFontDesc[LF_FACESIZE + 32];
    _snwprintf(szFontDesc, LF_FACESIZE + 32, L"%s, %u pt", pS->szFontName, (UINT)pS->dwFontSizePt);
    szFontDesc[LF_FACESIZE + 31] = L'\0';
    SetWindowTextW(GetDlgItem(pS->hwnd, IDC_CFG_FONT_TEXT), szFontDesc);
}

static LRESULT CALLBACK CfgDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CFG_DLG_STATE* pS = (CFG_DLG_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCs = (CREATESTRUCTW*)lParam;
            UINT d;
            int y;
            int xLbl, wFull;
            const APP_SETTINGS* pCfg;
            HWND hEd;
            RECT rcFr;

            pS = (CFG_DLG_STATE*)pCs->lpCreateParams;
            pS->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pS);

            d = pS->uDpi;
            pS->hFont = CreateDialogFont(d, 9, FW_NORMAL);
            pS->hFontBold = CreateDialogFont(d, 9, FW_SEMIBOLD);
            pS->hBrBg = CreateSolidBrush(pS->pTheme->crBgWindow);
            pS->hBrEdit = CreateSolidBrush(pS->pTheme->crBgSurface);
            Theme_ApplyToWindow(hwnd, pS->pTheme->bDarkMode);

            pCfg = pS->pSettings;
            if (pCfg->szFontName[0] != L'\0') {
                wcsncpy(pS->szFontName, pCfg->szFontName, LF_FACESIZE - 1);
                pS->szFontName[LF_FACESIZE - 1] = L'\0';
            } else {
                wcsncpy(pS->szFontName, AGENDA_DEFAULT_FONT_NAME, LF_FACESIZE - 1);
                pS->szFontName[LF_FACESIZE - 1] = L'\0';
            }
            pS->dwFontSizePt = (pCfg->dwFontSizePt >= 8 && pCfg->dwFontSizePt <= 14)
                               ? pCfg->dwFontSizePt : AGENDA_DEFAULT_FONT_PT;

            xLbl = DlgScale(d, 20);
            wFull = DlgScale(d, 560);
            y = DlgScale(d, 18);

            /* Data folder */
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFontBold,
                           L"Dossier des donn\u00E9es JSON (et sous-dossier versions\\) :",
                           SS_LEFT, xLbl, y, wFull, DlgScale(d, 22), 0);
            y += DlgScale(d, 24);

            MakeBorderedEdit(hwnd, pS->hInst, pS->hFont, pS->pTheme, pCfg->szDataFolder,
                             ES_AUTOHSCROLL | ES_READONLY,
                             xLbl, y, DlgScale(d, 444), DlgScale(d, 28), IDC_CFG_FOLDER, d, &rcFr, &hEd);
            if (pS->dwEditFrameCount < MAX_EDIT_FRAMES) {
                pS->hEditCtrls[pS->dwEditFrameCount] = hEd;
                pS->rcEditFrames[pS->dwEditFrameCount] = rcFr;
                pS->dwEditFrameCount++;
            }
            MakeThemedButton(hwnd, pS->hInst, pS->hFont, L"Parcourir...",
                             xLbl + DlgScale(d, 454), y, DlgScale(d, 106), DlgScale(d, 28),
                             IDC_CFG_BROWSE_DIR, BTN_ROLE_DEFAULT);
            y += DlgScale(d, 32);

            MakeStaticCtrl(hwnd, pS->hInst, pS->hFont, L"", SS_LEFT,
                           xLbl, y, wFull, DlgScale(d, 22), IDC_CFG_VER_INFO);
            CfgDlg_UpdateSnapshotInfo(pS);
            y += DlgScale(d, 34);

            /* Tray & Startup options */
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFontBold, L"Comportement syst\u00E8me :",
                           SS_LEFT, xLbl, y, wFull, DlgScale(d, 22), 0);
            y += DlgScale(d, 26);

            MakeThemedCheckbox(hwnd, pS->hInst, pS->hFont,
                               L"D\u00E9marrage r\u00E9duit dans le tray (zone de notification)",
                               xLbl, y, wFull, DlgScale(d, 24), IDC_CFG_START_MIN, pCfg->bStartMinimized);
            y += DlgScale(d, 28);

            MakeThemedCheckbox(hwnd, pS->hInst, pS->hFont,
                               L"R\u00E9duire dans le tray (lors de la minimisation ou fermeture)",
                               xLbl, y, wFull, DlgScale(d, 24), IDC_CFG_MIN_TRAY, pCfg->bMinimizeToTray);
            y += DlgScale(d, 28);

            MakeThemedCheckbox(hwnd, pS->hInst, pS->hFont,
                               L"D\u00E9marrer avec Windows (raccourci .lnk dans D\u00E9marrage)",
                               xLbl, y, wFull, DlgScale(d, 24), IDC_CFG_START_WIN, pCfg->bStartWithWindows);
            y += DlgScale(d, 36);

            /* Notification sound */
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFontBold, L"Son de la notification :",
                           SS_LEFT, xLbl, y, wFull, DlgScale(d, 22), 0);
            y += DlgScale(d, 26);

            MakeThemedCheckbox(hwnd, pS->hInst, pS->hFont,
                               L"Jouer un son lors des notifications d'\u00E9v\u00E9nements",
                               xLbl, y, wFull, DlgScale(d, 24), IDC_CFG_SOUND_EN, pCfg->bNotifySoundEnabled);
            y += DlgScale(d, 30);

            MakeBorderedEdit(hwnd, pS->hInst, pS->hFont, pS->pTheme, pCfg->szNotifySoundPath,
                             ES_AUTOHSCROLL | ES_READONLY,
                             xLbl, y, DlgScale(d, 294), DlgScale(d, 28), IDC_CFG_SOUND_PATH, d, &rcFr, &hEd);
            if (pS->dwEditFrameCount < MAX_EDIT_FRAMES) {
                pS->hEditCtrls[pS->dwEditFrameCount] = hEd;
                pS->rcEditFrames[pS->dwEditFrameCount] = rcFr;
                pS->dwEditFrameCount++;
            }
            MakeThemedButton(hwnd, pS->hInst, pS->hFont, L"Choisir .wav",
                             xLbl + DlgScale(d, 302), y, DlgScale(d, 92), DlgScale(d, 28),
                             IDC_CFG_BROWSE_WAV, BTN_ROLE_DEFAULT);
            MakeThemedButton(hwnd, pS->hInst, pS->hFont, L"Par d\u00E9faut",
                             xLbl + DlgScale(d, 400), y, DlgScale(d, 82), DlgScale(d, 28),
                             IDC_CFG_CLEAR_WAV, BTN_ROLE_DEFAULT);
            MakeThemedButton(hwnd, pS->hInst, pS->hFont, L"Tester",
                             xLbl + DlgScale(d, 488), y, DlgScale(d, 72), DlgScale(d, 28),
                             IDC_CFG_TEST_NOTIFY, BTN_ROLE_DEFAULT);
            y += DlgScale(d, 40);

            /* Default application font */
            MakeStaticCtrl(hwnd, pS->hInst, pS->hFontBold, L"Police par d\u00E9faut de l'application :",
                           SS_LEFT, xLbl, y, wFull, DlgScale(d, 22), 0);
            y += DlgScale(d, 26);

            MakeBorderedEdit(hwnd, pS->hInst, pS->hFont, pS->pTheme, L"",
                             ES_AUTOHSCROLL | ES_READONLY,
                             xLbl, y, DlgScale(d, 332), DlgScale(d, 28), IDC_CFG_FONT_TEXT, d, &rcFr, &hEd);
            if (pS->dwEditFrameCount < MAX_EDIT_FRAMES) {
                pS->hEditCtrls[pS->dwEditFrameCount] = hEd;
                pS->rcEditFrames[pS->dwEditFrameCount] = rcFr;
                pS->dwEditFrameCount++;
            }
            CfgDlg_UpdateFontDisplay(pS);

            MakeThemedButton(hwnd, pS->hInst, pS->hFont, L"Choisir police...",
                             xLbl + DlgScale(d, 340), y, DlgScale(d, 122), DlgScale(d, 28),
                             IDC_CFG_CHOOSE_FONT, BTN_ROLE_DEFAULT);
            MakeThemedButton(hwnd, pS->hInst, pS->hFont, L"Par d\u00E9faut",
                             xLbl + DlgScale(d, 468), y, DlgScale(d, 92), DlgScale(d, 28),
                             IDC_CFG_RESET_FONT, BTN_ROLE_DEFAULT);
            y += DlgScale(d, 46);

            MakeThemedButton(hwnd, pS->hInst, pS->hFontBold, L"Enregistrer",
                             xLbl + wFull - DlgScale(d, 232), y, DlgScale(d, 112), DlgScale(d, 34),
                             IDC_CFG_BTN_SAVE, BTN_ROLE_PRIMARY);
            MakeThemedButton(hwnd, pS->hInst, pS->hFont, L"Annuler",
                             xLbl + wFull - DlgScale(d, 110), y, DlgScale(d, 110), DlgScale(d, 34),
                             IDC_CFG_BTN_CANCEL, BTN_ROLE_DEFAULT);

            return 0;
        }
        case WM_ERASEBKGND:
            if (pS && pS->hBrBg) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                FillRect((HDC)wParam, &rc, pS->hBrBg);
                return 1;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pS && pS->pTheme) {
                Dlg_PaintEditFrames(hdc, pS->pTheme, pS->rcEditFrames, pS->hEditCtrls, pS->dwEditFrameCount, pS->uDpi);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DRAWITEM:
            if (pS && pS->pTheme) {
                const DRAWITEMSTRUCT* pDis = (const DRAWITEMSTRUCT*)lParam;
                if (pDis->CtlType == ODT_BUTTON) {
                    if (Dlg_IsOwnerDrawCheckbox(pDis->hwndItem)) {
                        Dlg_DrawCheckbox(pDis, pS->pTheme, pS->hFont, pS->uDpi);
                    } else {
                        INT_PTR nRole = (INT_PTR)GetPropW(pDis->hwndItem, PROP_BTN_ROLE);
                        Dlg_DrawPushButton(pDis, pS->pTheme, (nRole == BTN_ROLE_PRIMARY) ? pS->hFontBold : pS->hFont, pS->uDpi);
                    }
                    return TRUE;
                }
            }
            break;

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            if (pS && pS->pTheme) {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, pS->pTheme->crTextPrimary);
                SetBkColor(hdc, pS->pTheme->crBgWindow);
                return (LRESULT)pS->hBrBg;
            }
            break;

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            if (pS && pS->pTheme) {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, pS->pTheme->crTextPrimary);
                SetBkColor(hdc, pS->pTheme->crBgSurface);
                return (LRESULT)pS->hBrEdit;
            }
            break;

        case WM_COMMAND: {
            WORD wId = LOWORD(wParam);
            WORD wNotif = HIWORD(wParam);
            HWND hwndCtrl = (HWND)lParam;
            if (!pS) break;

            if (wNotif == BN_CLICKED && hwndCtrl && Dlg_IsOwnerDrawCheckbox(hwndCtrl)) {
                Dlg_ToggleChecked(hwndCtrl);
                return 0;
            }
            if (wId == IDC_CFG_BROWSE_DIR) {
                WCHAR szCur[MAX_PATH];
                WCHAR szNew[MAX_PATH];
                GetWindowTextW(GetDlgItem(hwnd, IDC_CFG_FOLDER), szCur, MAX_PATH);
                if (Storage_PickFolderDialog(hwnd, szCur, szNew, MAX_PATH)) {
                    SetWindowTextW(GetDlgItem(hwnd, IDC_CFG_FOLDER), szNew);
                    CfgDlg_UpdateSnapshotInfo(pS);
                }
                return 0;
            }
            if (wId == IDC_CFG_BROWSE_WAV) {
                OPENFILENAMEW ofn;
                WCHAR szFile[MAX_PATH];
                ZeroMemory(&ofn, sizeof(ofn));
                szFile[0] = L'\0';
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = L"Fichiers audio WAV (*.wav)\0*.wav\0Tous les fichiers (*.*)\0*.*\0";
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameW(&ofn)) {
                    SetWindowTextW(GetDlgItem(hwnd, IDC_CFG_SOUND_PATH), szFile);
                }
                return 0;
            }
            if (wId == IDC_CFG_CLEAR_WAV) {
                SetWindowTextW(GetDlgItem(hwnd, IDC_CFG_SOUND_PATH), L"");
                return 0;
            }
            if (wId == IDC_CFG_TEST_NOTIFY) {
                APP_SETTINGS tmp = *pS->pSettings;
                tmp.bNotifySoundEnabled = Dlg_IsChecked(GetDlgItem(hwnd, IDC_CFG_SOUND_EN));
                GetWindowTextW(GetDlgItem(hwnd, IDC_CFG_SOUND_PATH), tmp.szNotifySoundPath, MAX_PATH);
                Notify_ShowBalloon(pS->hwndOwner, L"Test de notification",
                                   L"Exemple de description affich\u00E9e dans la notification Agenda.", &tmp);
                return 0;
            }
            if (wId == IDC_CFG_CHOOSE_FONT) {
                CHOOSEFONTW cf;
                LOGFONTW lf;
                ZeroMemory(&lf, sizeof(lf));
                lf.lfHeight = -MulDiv((int)pS->dwFontSizePt, (int)pS->uDpi, 72);
                lf.lfWeight = FW_NORMAL;
                lf.lfCharSet = DEFAULT_CHARSET;
                lf.lfQuality = CLEARTYPE_QUALITY;
                wcsncpy(lf.lfFaceName, pS->szFontName, LF_FACESIZE - 1);
                lf.lfFaceName[LF_FACESIZE - 1] = L'\0';

                ZeroMemory(&cf, sizeof(cf));
                cf.lStructSize = sizeof(cf);
                cf.hwndOwner = hwnd;
                cf.lpLogFont = &lf;
                cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_LIMITSIZE | CF_NOVERTFONTS | CF_FORCEFONTEXIST;
                cf.nSizeMin = 8;
                cf.nSizeMax = 14;
                if (ChooseFontW(&cf)) {
                    DWORD dwPt = (DWORD)((cf.iPointSize + 5) / 10);
                    if (dwPt < 8) dwPt = 8;
                    if (dwPt > 14) dwPt = 14;
                    if (lf.lfFaceName[0] != L'\0') {
                        wcsncpy(pS->szFontName, lf.lfFaceName, LF_FACESIZE - 1);
                        pS->szFontName[LF_FACESIZE - 1] = L'\0';
                    }
                    pS->dwFontSizePt = dwPt;
                    CfgDlg_UpdateFontDisplay(pS);
                }
                return 0;
            }
            if (wId == IDC_CFG_RESET_FONT) {
                wcsncpy(pS->szFontName, AGENDA_DEFAULT_FONT_NAME, LF_FACESIZE - 1);
                pS->szFontName[LF_FACESIZE - 1] = L'\0';
                pS->dwFontSizePt = AGENDA_DEFAULT_FONT_PT;
                CfgDlg_UpdateFontDisplay(pS);
                return 0;
            }
            if (wId == IDC_CFG_BTN_SAVE) {
                WCHAR szNewFolder[MAX_PATH];
                BOOL bStartWin;

                GetWindowTextW(GetDlgItem(hwnd, IDC_CFG_FOLDER), szNewFolder, MAX_PATH);
                if (_wcsicmp(szNewFolder, pS->pSettings->szDataFolder) != 0) {
                    pS->bFolderChanged = TRUE;
                    wcsncpy(pS->pSettings->szDataFolder, szNewFolder, MAX_PATH - 1);
                    pS->pSettings->szDataFolder[MAX_PATH - 1] = L'\0';
                }

                pS->pSettings->bStartMinimized = Dlg_IsChecked(GetDlgItem(hwnd, IDC_CFG_START_MIN));
                pS->pSettings->bMinimizeToTray = Dlg_IsChecked(GetDlgItem(hwnd, IDC_CFG_MIN_TRAY));
                pS->pSettings->bNotifySoundEnabled = Dlg_IsChecked(GetDlgItem(hwnd, IDC_CFG_SOUND_EN));
                GetWindowTextW(GetDlgItem(hwnd, IDC_CFG_SOUND_PATH), pS->pSettings->szNotifySoundPath, MAX_PATH);

                wcsncpy(pS->pSettings->szFontName, pS->szFontName, LF_FACESIZE - 1);
                pS->pSettings->szFontName[LF_FACESIZE - 1] = L'\0';
                pS->pSettings->dwFontSizePt = pS->dwFontSizePt;

                bStartWin = Dlg_IsChecked(GetDlgItem(hwnd, IDC_CFG_START_WIN));
                pS->pSettings->bStartWithWindows = bStartWin;
                Settings_SyncStartupShortcut(bStartWin);
                Settings_Save(pS->pSettings);

                pS->bSaved = TRUE;
                pS->bClosed = TRUE;
                EnableWindow(pS->hwndOwner, TRUE);
                DestroyWindow(hwnd);
                return 0;
            }
            if (wId == IDC_CFG_BTN_CANCEL) {
                pS->bClosed = TRUE;
                EnableWindow(pS->hwndOwner, TRUE);
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            if (pS) {
                pS->bClosed = TRUE;
                EnableWindow(pS->hwndOwner, TRUE);
            }
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (pS) {
                if (pS->hFont) DeleteObject(pS->hFont);
                if (pS->hFontBold) DeleteObject(pS->hFontBold);
                if (pS->hBrBg) DeleteObject(pS->hBrBg);
                if (pS->hBrEdit) DeleteObject(pS->hBrEdit);
                pS->bClosed = TRUE;
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

BOOL Dialogs_Register(HINSTANCE hInst)
{
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = EventDlgProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = AGENDA_DLG_EVENT_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    wc.lpfnWndProc = CfgDlgProc;
    wc.lpszClassName = AGENDA_DLG_CFG_CLASS;
    return (RegisterClassExW(&wc) != 0);
}

static void RunModalLoop(HWND hwndModal, HWND hwndOwner, BOOL* pbClosed)
{
    MSG msg;
    EnableWindow(hwndOwner, FALSE);
    ShowWindow(hwndModal, SW_SHOW);
    UpdateWindow(hwndModal);

    while (!(*pbClosed) && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            /* Only close modal dialog if a dropdown popup is NOT active */
            if (GetActiveWindow() == hwndModal) {
                PostMessageW(hwndModal, WM_CLOSE, 0, 0);
                continue;
            }
        }
        if (!IsDialogMessageW(hwndModal, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(hwndOwner, TRUE);
    SetForegroundWindow(hwndOwner);
}

BOOL Dialog_ShowEventEditor(
    HWND hwndOwner,
    HINSTANCE hInst,
    const APP_THEME* pTheme,
    CALENDAR_EVENT* pEvent,
    BOOL bIsEdit,
    BOOL* pbDeleted
)
{
    EVENT_DLG_STATE state;
    UINT uDpi = GetWindowDpiSafe(hwndOwner);
    DWORD dwStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    RECT rcClient = {0, 0, DlgScale(uDpi, 724), DlgScale(uDpi, 496)};
    RECT rcOwner;
    int nW, nH, x, y;
    HWND hwndDlg;

    ZeroMemory(&state, sizeof(state));
    state.hwndOwner = hwndOwner;
    state.hInst = hInst;
    state.pTheme = pTheme;
    state.pEvent = pEvent;
    state.bIsEdit = bIsEdit;
    state.uDpi = uDpi;

    AdjustWindowRectEx(&rcClient, dwStyle, FALSE, 0);
    nW = rcClient.right - rcClient.left;
    nH = rcClient.bottom - rcClient.top;

    GetWindowRect(hwndOwner, &rcOwner);
    x = rcOwner.left + ((rcOwner.right - rcOwner.left) - nW) / 2;
    y = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - nH) / 2;

    hwndDlg = CreateWindowExW(
        0,
        AGENDA_DLG_EVENT_CLASS,
        bIsEdit ? L"Modifier l'\u00E9v\u00E9nement" : L"Nouvel \u00E9v\u00E9nement",
        dwStyle,
        x, y, nW, nH,
        hwndOwner, NULL, hInst, &state
    );
    if (!hwndDlg) {
        return FALSE;
    }

    RunModalLoop(hwndDlg, hwndOwner, &state.bClosed);
    if (pbDeleted) {
        *pbDeleted = state.bDeleted;
    }
    return state.bSaved || state.bDeleted;
}

BOOL Dialog_ShowSettings(
    HWND hwndOwner,
    HINSTANCE hInst,
    const APP_THEME* pTheme,
    APP_SETTINGS* pSettings,
    BOOL* pbDataFolderChanged
)
{
    CFG_DLG_STATE state;
    UINT uDpi = GetWindowDpiSafe(hwndOwner);
    DWORD dwStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    RECT rcClient = {0, 0, DlgScale(uDpi, 600), DlgScale(uDpi, 454)};
    RECT rcOwner;
    int nW, nH, x, y;
    HWND hwndDlg;

    ZeroMemory(&state, sizeof(state));
    state.hwndOwner = hwndOwner;
    state.hInst = hInst;
    state.pTheme = pTheme;
    state.pSettings = pSettings;
    state.uDpi = uDpi;

    AdjustWindowRectEx(&rcClient, dwStyle, FALSE, 0);
    nW = rcClient.right - rcClient.left;
    nH = rcClient.bottom - rcClient.top;

    GetWindowRect(hwndOwner, &rcOwner);
    x = rcOwner.left + ((rcOwner.right - rcOwner.left) - nW) / 2;
    y = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - nH) / 2;

    hwndDlg = CreateWindowExW(
        0,
        AGENDA_DLG_CFG_CLASS,
        L"R\u00E9glages \u2014 Agenda",
        dwStyle,
        x, y, nW, nH,
        hwndOwner, NULL, hInst, &state
    );
    if (!hwndDlg) {
        return FALSE;
    }

    RunModalLoop(hwndDlg, hwndOwner, &state.bClosed);
    if (pbDataFolderChanged) {
        *pbDataFolderChanged = state.bFolderChanged;
    }
    return state.bSaved;
}
