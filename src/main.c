#include "types.h"
#include "calendar_view.h"
#include "date_utils.h"
#include "dialogs.h"
#include "notify.h"
#include "recurrence.h"
#include "settings.h"
#include "storage.h"
#include "theme.h"
#include "themed_ctrls.h"

/* Toolbar control IDs */
#define IDC_TB_PREV       101
#define IDC_TB_TODAY      102
#define IDC_TB_NEXT       103
#define IDC_TB_TITLE      104
#define IDC_TB_SEARCH     105
#define IDC_TB_VIEW_DAY   106
#define IDC_TB_VIEW_WEEK  107
#define IDC_TB_VIEW_MONTH 108
#define IDC_TB_VIEW_YEAR  109
#define IDC_TB_NEW_EVENT  110
#define IDC_TB_SETTINGS   111

/* Tray context menu command IDs */
#define IDM_TRAY_OPEN     4001
#define IDM_TRAY_NEW      4002
#define IDM_TRAY_TODAY    4003
#define IDM_TRAY_SETTINGS 4004
#define IDM_TRAY_QUIT     4005

#ifndef WM_DPICHANGED
#define WM_DPICHANGED     0x02E0
#endif

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER   0x1501
#endif

typedef BOOL (WINAPI *PFN_SetProcessDpiAwarenessContext)(HANDLE);
typedef HRESULT (WINAPI *PFN_SetCurrentProcessExplicitAppUserModelID)(PCWSTR);

typedef struct _MAIN_APP_STATE {
    HINSTANCE         hInst;
    HWND              hwndMain;
    HWND              hwndCanvas;
    APP_SETTINGS      settings;
    APP_THEME         theme;
    EVENT_LIST        events;
    FIRED_NOTIFY_LIST firedNotifs;
    UINT              uDpi;
    HFONT             hFontUi;
    HFONT             hFontTitle;
    HFONT             hFontBold;
    HBRUSH            hBrToolbar;
    HBRUSH            hBrSearch;
    HICON             hIconLarge;
    HICON             hIconSmall;
    BOOL              bForceQuit;
} MAIN_APP_STATE;

static LONG MainScale(UINT uDpi, LONG lPx)
{
    return MulDiv(lPx, (int)uDpi, 96);
}

static void EnableAppUserModelId(void)
{
    HMODULE hShell32 = GetModuleHandleW(L"shell32.dll");
    if (!hShell32) {
        hShell32 = LoadLibraryW(L"shell32.dll");
    }
    if (hShell32) {
        PFN_SetCurrentProcessExplicitAppUserModelID pfnSetAumid =
            (PFN_SetCurrentProcessExplicitAppUserModelID)(void*)GetProcAddress(
                hShell32, "SetCurrentProcessExplicitAppUserModelID");
        if (pfnSetAumid) {
            pfnSetAumid(AGENDA_APP_USER_MODEL_ID);
        }
    }
}

static void EnablePerMonitorDpi(void)
{
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        PFN_SetProcessDpiAwarenessContext pfnDpiCtx =
            (PFN_SetProcessDpiAwarenessContext)(void*)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pfnDpiCtx) {
            /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = (HANDLE)-4 */
            if (pfnDpiCtx((HANDLE)(INT_PTR)-4)) {
                return;
            }
        }
    }
    SetProcessDPIAware();
}

static HFONT CreateMainFont(UINT uDpi, LONG lPt, int nWeight)
{
    return Theme_CreateAppFont(uDpi, lPt - 9, nWeight, FALSE);
}

static void RebuildFontsAndBrushes(MAIN_APP_STATE* pApp)
{
    if (pApp->hFontUi) DeleteObject(pApp->hFontUi);
    if (pApp->hFontTitle) DeleteObject(pApp->hFontTitle);
    if (pApp->hFontBold) DeleteObject(pApp->hFontBold);
    if (pApp->hBrToolbar) DeleteObject(pApp->hBrToolbar);
    if (pApp->hBrSearch) DeleteObject(pApp->hBrSearch);

    pApp->hFontUi    = CreateMainFont(pApp->uDpi, 9, FW_NORMAL);
    pApp->hFontBold  = CreateMainFont(pApp->uDpi, 9, FW_SEMIBOLD);
    pApp->hFontTitle = CreateMainFont(pApp->uDpi, 13, FW_BOLD);
    pApp->hBrToolbar = CreateSolidBrush(pApp->theme.crBgHeader);
    pApp->hBrSearch  = CreateSolidBrush(pApp->theme.crBgSurface);
}

static void RefreshIcons(MAIN_APP_STATE* pApp)
{
    HICON hNewLarge = Theme_CreateCalendarIcon(32, pApp->theme.bDarkMode);
    HICON hNewSmall = Theme_CreateCalendarIcon(16, pApp->theme.bDarkMode);

    SendMessageW(pApp->hwndMain, WM_SETICON, ICON_BIG, (LPARAM)hNewLarge);
    SendMessageW(pApp->hwndMain, WM_SETICON, ICON_SMALL, (LPARAM)hNewSmall);
    Notify_UpdateTrayIcon(pApp->hwndMain, hNewSmall);

    if (pApp->hIconLarge) DestroyIcon(pApp->hIconLarge);
    if (pApp->hIconSmall) DestroyIcon(pApp->hIconSmall);
    pApp->hIconLarge = hNewLarge;
    pApp->hIconSmall = hNewSmall;
}

static void UpdateToolbarTitle(MAIN_APP_STATE* pApp)
{
    SYSTEMTIME stAnchor;
    WCHAR szTitle[128];

    CalendarCanvas_GetAnchorDate(pApp->hwndCanvas, &stAnchor);

    if (pApp->settings.dwViewMode == VIEW_DAY) {
        Date_FormatFrenchDate(&stAnchor, szTitle, 128);
    } else if (pApp->settings.dwViewMode == VIEW_WEEK) {
        SYSTEMTIME stMon;
        WCHAR szMonth[32];
        Date_GetWeekStartMonday(&stAnchor, &stMon);
        Date_GetMonthName(stMon.wMonth, szMonth, 32);
        _snwprintf(szTitle, 128, L"Semaine du %u %s %u", (UINT)stMon.wDay, szMonth, (UINT)stMon.wYear);
    } else if (pApp->settings.dwViewMode == VIEW_MONTH) {
        Date_FormatMonthYear(&stAnchor, szTitle, 128);
    } else {
        _snwprintf(szTitle, 128, L"Ann\u00E9e %u", (UINT)stAnchor.wYear);
    }
    szTitle[127] = L'\0';
    SetWindowTextW(GetDlgItem(pApp->hwndMain, IDC_TB_TITLE), szTitle);

    /* Redraw view switcher buttons so active state highlights cleanly */
    InvalidateRect(GetDlgItem(pApp->hwndMain, IDC_TB_VIEW_DAY), NULL, FALSE);
    InvalidateRect(GetDlgItem(pApp->hwndMain, IDC_TB_VIEW_WEEK), NULL, FALSE);
    InvalidateRect(GetDlgItem(pApp->hwndMain, IDC_TB_VIEW_MONTH), NULL, FALSE);
    InvalidateRect(GetDlgItem(pApp->hwndMain, IDC_TB_VIEW_YEAR), NULL, FALSE);
}

static void LayoutMainControls(MAIN_APP_STATE* pApp, int nW, int nH)
{
    UINT d = pApp->uDpi;
    LONG lBarH = MainScale(d, 56);
    LONG lY = MainScale(d, 12);
    LONG lBtnH = MainScale(d, 32);
    LONG lX = MainScale(d, 14);
    LONG lRightX = nW - MainScale(d, 14);
    LONG lSettingsW = MainScale(d, 96);
    LONG lNewW = MainScale(d, 152);
    LONG lViewW = MainScale(d, 74);
    LONG lSearchW = MainScale(d, 160);
    LONG lTitleW;

    if (nW < 400) return;

    MoveWindow(GetDlgItem(pApp->hwndMain, IDC_TB_PREV), lX, lY, MainScale(d, 34), lBtnH, TRUE);
    lX += MainScale(d, 38);
    MoveWindow(GetDlgItem(pApp->hwndMain, IDC_TB_TODAY), lX, lY, MainScale(d, 96), lBtnH, TRUE);
    lX += MainScale(d, 100);
    MoveWindow(GetDlgItem(pApp->hwndMain, IDC_TB_NEXT), lX, lY, MainScale(d, 34), lBtnH, TRUE);
    lX += MainScale(d, 46);

    /* Right-aligned action buttons and view tabs */
    lRightX -= lSettingsW;
    MoveWindow(GetDlgItem(pApp->hwndMain, IDC_TB_SETTINGS), lRightX, lY, lSettingsW, lBtnH, TRUE);
    lRightX -= (lNewW + MainScale(d, 8));
    MoveWindow(GetDlgItem(pApp->hwndMain, IDC_TB_NEW_EVENT), lRightX, lY, lNewW, lBtnH, TRUE);
    lRightX -= MainScale(d, 14);

    lRightX -= lViewW;
    MoveWindow(GetDlgItem(pApp->hwndMain, IDC_TB_VIEW_YEAR), lRightX, lY, lViewW, lBtnH, TRUE);
    lRightX -= (lViewW + MainScale(d, 2));
    MoveWindow(GetDlgItem(pApp->hwndMain, IDC_TB_VIEW_MONTH), lRightX, lY, lViewW, lBtnH, TRUE);
    lRightX -= (lViewW + MainScale(d, 2));
    MoveWindow(GetDlgItem(pApp->hwndMain, IDC_TB_VIEW_WEEK), lRightX, lY, lViewW, lBtnH, TRUE);
    lRightX -= (lViewW + MainScale(d, 2));
    MoveWindow(GetDlgItem(pApp->hwndMain, IDC_TB_VIEW_DAY), lRightX, lY, lViewW, lBtnH, TRUE);
    lRightX -= MainScale(d, 14);

    /* Optional search input if window width allows */
    if (lRightX - lX > MainScale(d, 340)) {
        lRightX -= lSearchW;
        MoveWindow(GetDlgItem(pApp->hwndMain, IDC_TB_SEARCH), lRightX + MainScale(d, 4), lY + MainScale(d, 5),
                   lSearchW - MainScale(d, 8), MainScale(d, 22), TRUE);
        ShowWindow(GetDlgItem(pApp->hwndMain, IDC_TB_SEARCH), SW_SHOW);
        lRightX -= MainScale(d, 12);
    } else {
        ShowWindow(GetDlgItem(pApp->hwndMain, IDC_TB_SEARCH), SW_HIDE);
    }

    lTitleW = lRightX - lX;
    if (lTitleW < MainScale(d, 100)) lTitleW = MainScale(d, 100);
    MoveWindow(GetDlgItem(pApp->hwndMain, IDC_TB_TITLE), lX, lY + MainScale(d, 3),
               lTitleW, MainScale(d, 26), TRUE);

    MoveWindow(pApp->hwndCanvas, 0, lBarH, nW, (nH > lBarH) ? (nH - lBarH) : 0, TRUE);
}

static void DrawToolbarButton(MAIN_APP_STATE* pApp, const DRAWITEMSTRUCT* pDis)
{
    const APP_THEME* pT = &pApp->theme;
    HDC hdc = pDis->hDC;
    RECT rc = pDis->rcItem;
    WCHAR szText[64];
    BOOL bPressed = (pDis->itemState & ODS_SELECTED) != 0;
    BOOL bIsPrimary = (pDis->CtlID == IDC_TB_NEW_EVENT);
    BOOL bIsActiveView =
        (pDis->CtlID == IDC_TB_VIEW_DAY   && pApp->settings.dwViewMode == VIEW_DAY) ||
        (pDis->CtlID == IDC_TB_VIEW_WEEK  && pApp->settings.dwViewMode == VIEW_WEEK) ||
        (pDis->CtlID == IDC_TB_VIEW_MONTH && pApp->settings.dwViewMode == VIEW_MONTH) ||
        (pDis->CtlID == IDC_TB_VIEW_YEAR  && pApp->settings.dwViewMode == VIEW_YEAR);
    COLORREF crBg;
    COLORREF crBorder;
    COLORREF crText;
    HBRUSH hBrBg;
    HBRUSH hBrBtn;
    HPEN hPen;
    HBRUSH hOldBr;
    HPEN hOldPen;
    HFONT hOldFont;
    int nRad = MainScale(pApp->uDpi, 6);

    if (bIsPrimary || bIsActiveView) {
        crBg = pT->crAccent;
        crBorder = pT->crAccent;
        crText = pT->crAccentText;
    } else {
        crBg = bPressed ? pT->crBgSelected : pT->crBgSurface;
        crBorder = pT->crBorderStrong;
        crText = pT->crTextPrimary;
    }

    hBrBg = CreateSolidBrush(pT->crBgHeader);
    FillRect(hdc, &rc, hBrBg);
    DeleteObject(hBrBg);

    hBrBtn = CreateSolidBrush(crBg);
    hPen = CreatePen(PS_SOLID, 1, crBorder);
    hOldBr = (HBRUSH)SelectObject(hdc, hBrBtn);
    hOldPen = (HPEN)SelectObject(hdc, hPen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, nRad, nRad);
    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrBtn);
    DeleteObject(hPen);

    GetWindowTextW(pDis->hwndItem, szText, 64);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, crText);
    hOldFont = (HFONT)SelectObject(hdc, (bIsPrimary || bIsActiveView) ? pApp->hFontBold : pApp->hFontUi);
    DrawTextW(hdc, szText, -1, &rc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    SelectObject(hdc, hOldFont);
}

static void RestoreMainWindow(MAIN_APP_STATE* pApp)
{
    ShowWindow(pApp->hwndMain, SW_RESTORE);
    ShowWindow(pApp->hwndMain, SW_SHOW);
    SetForegroundWindow(pApp->hwndMain);
}

static void OpenNewEventDialog(MAIN_APP_STATE* pApp, const SYSTEMTIME* pstInitial)
{
    CALENDAR_EVENT ev;
    BOOL bDeleted = FALSE;

    ZeroMemory(&ev, sizeof(ev));
    ev.dwId = pApp->settings.dwNextEventId;
    ev.dwVersion = 1;
    if (pstInitial) {
        ev.stStart = *pstInitial;
    } else {
        GetLocalTime(&ev.stStart);
        ev.stStart.wMinute = 0;
    }
    ev.stStart.wSecond = 0;
    Date_NormalizeTime(&ev.stStart);

    ev.dwDurationMinutes = 60;
    ev.dwRecurrenceType = REC_NONE;
    ev.dwRecurrenceInterval = 1;
    ev.dwWeekdaysMask = (1U << Date_MondayBasedDayOfWeek(&ev.stStart));
    ev.lNthOrdinal = 1;
    ev.dwNthWeekday = ev.stStart.wDayOfWeek;
    ev.stEndDate = ev.stStart;
    Date_AddMonths(&ev.stEndDate, 3);
    ev.dwNotifyMode = NOTIFY_BEFORE;
    ev.dwNotifyMinutesBefore = 15;
    ev.dwColorIndex = 0;

    if (Dialog_ShowEventEditor(pApp->hwndMain, pApp->hInst, &pApp->theme, &ev, FALSE, &bDeleted)) {
        pApp->settings.dwNextEventId += 1;
        EventList_Add(&pApp->events, &ev);
        Storage_SaveEvents(&pApp->settings, &pApp->events);
        CalendarCanvas_Refresh(pApp->hwndCanvas);
        Notify_CheckDueEvents(pApp->hwndMain, &pApp->events, &pApp->settings, &pApp->firedNotifs);
    }
}

static void OpenEditEventDialog(MAIN_APP_STATE* pApp, DWORD dwEventId)
{
    CALENDAR_EVENT* pExisting = EventList_FindById(&pApp->events, dwEventId);
    CALENDAR_EVENT evCopy;
    BOOL bDeleted = FALSE;

    if (!pExisting) return;
    evCopy = *pExisting;

    if (Dialog_ShowEventEditor(pApp->hwndMain, pApp->hInst, &pApp->theme, &evCopy, TRUE, &bDeleted)) {
        if (bDeleted) {
            EventList_Remove(&pApp->events, dwEventId);
        } else {
            evCopy.dwVersion += 1;
            EventList_Update(&pApp->events, &evCopy);
        }
        Storage_SaveEvents(&pApp->settings, &pApp->events);
        CalendarCanvas_Refresh(pApp->hwndCanvas);
        Notify_CheckDueEvents(pApp->hwndMain, &pApp->events, &pApp->settings, &pApp->firedNotifs);
    }
}

static void OpenSettingsDialog(MAIN_APP_STATE* pApp)
{
    BOOL bFolderChanged = FALSE;
    if (Dialog_ShowSettings(pApp->hwndMain, pApp->hInst, &pApp->theme, &pApp->settings, &bFolderChanged)) {
        int id;
        RECT rcClient;

        Theme_SetDefaultFont(pApp->settings.szFontName, pApp->settings.dwFontSizePt);
        RebuildFontsAndBrushes(pApp);
        SendMessageW(GetDlgItem(pApp->hwndMain, IDC_TB_TITLE), WM_SETFONT, (WPARAM)pApp->hFontTitle, FALSE);
        SendMessageW(GetDlgItem(pApp->hwndMain, IDC_TB_SEARCH), WM_SETFONT, (WPARAM)pApp->hFontUi, FALSE);
        for (id = IDC_TB_PREV; id <= IDC_TB_SETTINGS; ++id) {
            HWND hCtrl = GetDlgItem(pApp->hwndMain, id);
            if (hCtrl) {
                InvalidateRect(hCtrl, NULL, FALSE);
            }
        }
        GetClientRect(pApp->hwndMain, &rcClient);
        LayoutMainControls(pApp, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);

        if (bFolderChanged) {
            WCHAR szExistingJson[MAX_PATH];
            _snwprintf(szExistingJson, MAX_PATH, L"%s\\%s", pApp->settings.szDataFolder, AGENDA_JSON_FILENAME);
            szExistingJson[MAX_PATH - 1] = L'\0';

            if (GetFileAttributesW(szExistingJson) != INVALID_FILE_ATTRIBUTES) {
                int nChoice = Themed_MessageBox(
                    pApp->hwndMain,
                    pApp->hInst,
                    &pApp->theme,
                    AGENDA_APP_NAME,
                    L"Un fichier events.json existe d\u00E9j\u00E0 dans ce dossier.\n\n"
                    L"Oui : Charger les \u00E9v\u00E9nements de ce dossier.\n"
                    L"Non : Exporter vos \u00E9v\u00E9nements actuels dans ce dossier.",
                    TRUE
                );
                if (nChoice == IDYES) {
                    Storage_LoadEvents(&pApp->settings, &pApp->events);
                } else {
                    Storage_SaveEvents(&pApp->settings, &pApp->events);
                }
            } else {
                Storage_SaveEvents(&pApp->settings, &pApp->events);
            }
        }
        InvalidateRect(pApp->hwndMain, NULL, TRUE);
        CalendarCanvas_Refresh(pApp->hwndCanvas);
    }
}

static void ShowTrayContextMenu(MAIN_APP_STATE* pApp)
{
    POINT pt;
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Ouvrir Agenda");
    SetMenuDefaultItem(hMenu, IDM_TRAY_OPEN, FALSE);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_NEW, L"Nouvel \u00E9v\u00E9nement...");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_TODAY, L"Aujourd'hui");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SETTINGS, L"R\u00E9glages...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_QUIT, L"Quitter");

    GetCursorPos(&pt);
    SetForegroundWindow(pApp->hwndMain);
    TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, pApp->hwndMain, NULL);
    PostMessageW(pApp->hwndMain, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

static void ApplyThemeEverywhere(MAIN_APP_STATE* pApp)
{
    BOOL bDark = Theme_IsSystemDarkMode();
    Theme_InitProcessDarkMode(bDark);
    Theme_BuildPalette(&pApp->theme, bDark);
    Theme_ApplyToWindow(pApp->hwndMain, bDark);
    Theme_ApplyToControl(GetDlgItem(pApp->hwndMain, IDC_TB_SEARCH), bDark, TRUE);
    RebuildFontsAndBrushes(pApp);
    RefreshIcons(pApp);
    InvalidateRect(pApp->hwndMain, NULL, TRUE);
    CalendarCanvas_Refresh(pApp->hwndCanvas);
}

static HWND MakeToolbarBtn(MAIN_APP_STATE* pApp, const WCHAR* pszText, int nId)
{
    return CreateWindowExW(
        0, L"BUTTON", pszText,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 80, 30,
        pApp->hwndMain, (HMENU)(INT_PTR)nId, pApp->hInst, NULL
    );
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MAIN_APP_STATE* pApp = (MAIN_APP_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCs = (CREATESTRUCTW*)lParam;
            HDC hdc;
            HWND hTitle;
            HWND hSearch;

            pApp = (MAIN_APP_STATE*)pCs->lpCreateParams;
            pApp->hwndMain = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pApp);

            hdc = GetDC(hwnd);
            pApp->uDpi = hdc ? (UINT)GetDeviceCaps(hdc, LOGPIXELSY) : 96;
            if (hdc) ReleaseDC(hwnd, hdc);

            Theme_BuildPalette(&pApp->theme, Theme_IsSystemDarkMode());
            Theme_ApplyToWindow(hwnd, pApp->theme.bDarkMode);
            RebuildFontsAndBrushes(pApp);

            MakeToolbarBtn(pApp, L"<", IDC_TB_PREV);
            MakeToolbarBtn(pApp, L"Aujourd'hui", IDC_TB_TODAY);
            MakeToolbarBtn(pApp, L">", IDC_TB_NEXT);

            hTitle = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                                     0, 0, 200, 26, hwnd, (HMENU)(INT_PTR)IDC_TB_TITLE, pApp->hInst, NULL);
            SendMessageW(hTitle, WM_SETFONT, (WPARAM)pApp->hFontTitle, FALSE);

            hSearch = CreateWindowExW(0, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                      0, 0, 152, 22, hwnd, (HMENU)(INT_PTR)IDC_TB_SEARCH, pApp->hInst, NULL);
            SendMessageW(hSearch, WM_SETFONT, (WPARAM)pApp->hFontUi, FALSE);
            SendMessageW(hSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"Rechercher...");
            Theme_ApplyToControl(hSearch, pApp->theme.bDarkMode, TRUE);

            MakeToolbarBtn(pApp, L"Jour", IDC_TB_VIEW_DAY);
            MakeToolbarBtn(pApp, L"Semaine", IDC_TB_VIEW_WEEK);
            MakeToolbarBtn(pApp, L"Mois", IDC_TB_VIEW_MONTH);
            MakeToolbarBtn(pApp, L"Ann\u00E9e", IDC_TB_VIEW_YEAR);
            MakeToolbarBtn(pApp, L"+ Nouvel \u00E9v\u00E9nement", IDC_TB_NEW_EVENT);
            MakeToolbarBtn(pApp, L"R\u00E9glages", IDC_TB_SETTINGS);

            pApp->hwndCanvas = CalendarCanvas_Create(pApp->hInst, hwnd, &pApp->events, &pApp->theme);
            CalendarCanvas_SetDpi(pApp->hwndCanvas, pApp->uDpi);
            CalendarCanvas_SetView(pApp->hwndCanvas, pApp->settings.dwViewMode, NULL);

            pApp->hIconLarge = Theme_CreateCalendarIcon(32, pApp->theme.bDarkMode);
            pApp->hIconSmall = Theme_CreateCalendarIcon(16, pApp->theme.bDarkMode);
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)pApp->hIconLarge);
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)pApp->hIconSmall);

            Notify_InitTray(hwnd, pApp->hIconSmall);
            SetTimer(hwnd, TIMER_ID_NOTIFY, TIMER_INTERVAL_MS, NULL);

            UpdateToolbarTitle(pApp);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            if (pApp) {
                MINMAXINFO* pMmi = (MINMAXINFO*)lParam;
                pMmi->ptMinTrackSize.x = MainScale(pApp->uDpi, 820);
                pMmi->ptMinTrackSize.y = MainScale(pApp->uDpi, 520);
                return 0;
            }
            break;
        }
        case WM_SIZE: {
            if (pApp && wParam != SIZE_MINIMIZED) {
                LayoutMainControls(pApp, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        }
        case WM_SYSCOMMAND: {
            if (pApp && (wParam & 0xFFF0) == SC_MINIMIZE && pApp->settings.bMinimizeToTray) {
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
            break;
        }
        case WM_CLOSE: {
            if (pApp && pApp->settings.bMinimizeToTray && !pApp->bForceQuit) {
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pApp) {
                RECT rcClient;
                RECT rcBar;
                LONG lBarH = MainScale(pApp->uDpi, 56);
                HPEN hPen;
                HPEN hOldPen;

                GetClientRect(hwnd, &rcClient);
                rcBar = rcClient;
                rcBar.bottom = lBarH;
                FillRect(hdc, &rcBar, pApp->hBrToolbar);

                hPen = CreatePen(PS_SOLID, 1, pApp->theme.crBorderStrong);
                hOldPen = (HPEN)SelectObject(hdc, hPen);
                MoveToEx(hdc, 0, lBarH - 1, NULL);
                LineTo(hdc, rcClient.right, lBarH - 1);

                /* Draw themed border around visible search EDIT */
                {
                    HWND hSearch = GetDlgItem(hwnd, IDC_TB_SEARCH);
                    if (hSearch && IsWindowVisible(hSearch)) {
                        RECT rcEd;
                        POINT pt1, pt2;
                        HBRUSH hOldBr;
                        GetWindowRect(hSearch, &rcEd);
                        pt1.x = rcEd.left; pt1.y = rcEd.top;
                        pt2.x = rcEd.right; pt2.y = rcEd.bottom;
                        ScreenToClient(hwnd, &pt1);
                        ScreenToClient(hwnd, &pt2);
                        hOldBr = (HBRUSH)SelectObject(hdc, pApp->hBrSearch);
                        RoundRect(hdc, pt1.x - MainScale(pApp->uDpi, 4), pt1.y - MainScale(pApp->uDpi, 3),
                                  pt2.x + MainScale(pApp->uDpi, 4), pt2.y + MainScale(pApp->uDpi, 3),
                                  MainScale(pApp->uDpi, 5), MainScale(pApp->uDpi, 5));
                        SelectObject(hdc, hOldBr);
                    }
                }
                SelectObject(hdc, hOldPen);
                DeleteObject(hPen);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            if (pApp) {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, pApp->theme.crTextPrimary);
                SetBkColor(hdc, pApp->theme.crBgHeader);
                return (LRESULT)pApp->hBrToolbar;
            }
            break;
        }
        case WM_CTLCOLOREDIT: {
            if (pApp) {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, pApp->theme.crTextPrimary);
                SetBkColor(hdc, pApp->theme.crBgSurface);
                return (LRESULT)pApp->hBrSearch;
            }
            break;
        }
        case WM_DRAWITEM: {
            if (pApp && wParam >= IDC_TB_PREV && wParam <= IDC_TB_SETTINGS) {
                DrawToolbarButton(pApp, (const DRAWITEMSTRUCT*)lParam);
                return TRUE;
            }
            break;
        }
        case WM_SETTINGCHANGE: {
            if (pApp && lParam != 0 && wcscmp((const WCHAR*)lParam, L"ImmersiveColorSet") == 0) {
                ApplyThemeEverywhere(pApp);
            }
            return 0;
        }
        case WM_DPICHANGED: {
            if (pApp) {
                RECT* pSuggested = (RECT*)lParam;
                pApp->uDpi = HIWORD(wParam);
                RebuildFontsAndBrushes(pApp);
                SendMessageW(GetDlgItem(hwnd, IDC_TB_TITLE), WM_SETFONT, (WPARAM)pApp->hFontTitle, FALSE);
                SendMessageW(GetDlgItem(hwnd, IDC_TB_SEARCH), WM_SETFONT, (WPARAM)pApp->hFontUi, FALSE);
                CalendarCanvas_SetDpi(pApp->hwndCanvas, pApp->uDpi);
                SetWindowPos(hwnd, NULL, pSuggested->left, pSuggested->top,
                             pSuggested->right - pSuggested->left,
                             pSuggested->bottom - pSuggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }
        case WM_TIMER: {
            if (pApp && wParam == TIMER_ID_NOTIFY) {
                Notify_CheckDueEvents(hwnd, &pApp->events, &pApp->settings, &pApp->firedNotifs);
                return 0;
            }
            break;
        }
        case WM_APP_TRAYICON: {
            if (!pApp) break;
            if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONUP) {
                RestoreMainWindow(pApp);
            } else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                ShowTrayContextMenu(pApp);
            }
            return 0;
        }
        case WM_APP_SHOW_MAIN: {
            if (pApp) {
                RestoreMainWindow(pApp);
            }
            return 0;
        }
        case WM_APP_CANVAS_CREATE: {
            if (pApp) {
                OpenNewEventDialog(pApp, (const SYSTEMTIME*)lParam);
            }
            return 0;
        }
        case WM_APP_CANVAS_EDIT: {
            if (pApp) {
                OpenEditEventDialog(pApp, (DWORD)wParam);
            }
            return 0;
        }
        case WM_APP_CANVAS_NAVIGATE: {
            if (pApp) {
                pApp->settings.dwViewMode = (DWORD)wParam;
                Settings_Save(&pApp->settings);
                CalendarCanvas_SetView(pApp->hwndCanvas, pApp->settings.dwViewMode, (const SYSTEMTIME*)lParam);
                UpdateToolbarTitle(pApp);
            }
            return 0;
        }
        case WM_COMMAND: {
            WORD wId = LOWORD(wParam);
            WORD wNotif = HIWORD(wParam);
            if (!pApp) break;

            if (wId == IDC_TB_SEARCH && wNotif == EN_CHANGE) {
                WCHAR szFilter[128];
                GetWindowTextW(GetDlgItem(hwnd, IDC_TB_SEARCH), szFilter, 128);
                CalendarCanvas_SetFilter(pApp->hwndCanvas, szFilter);
                return 0;
            }
            switch (wId) {
                case IDC_TB_PREV:
                    CalendarCanvas_NavigateStep(pApp->hwndCanvas, -1);
                    UpdateToolbarTitle(pApp);
                    return 0;
                case IDC_TB_TODAY:
                case IDM_TRAY_TODAY:
                    CalendarCanvas_NavigateStep(pApp->hwndCanvas, 0);
                    UpdateToolbarTitle(pApp);
                    if (wId == IDM_TRAY_TODAY) RestoreMainWindow(pApp);
                    return 0;
                case IDC_TB_NEXT:
                    CalendarCanvas_NavigateStep(pApp->hwndCanvas, 1);
                    UpdateToolbarTitle(pApp);
                    return 0;
                case IDC_TB_VIEW_DAY:
                case IDC_TB_VIEW_WEEK:
                case IDC_TB_VIEW_MONTH:
                case IDC_TB_VIEW_YEAR: {
                    DWORD dwMode = (DWORD)(wId - IDC_TB_VIEW_DAY);
                    pApp->settings.dwViewMode = dwMode;
                    Settings_Save(&pApp->settings);
                    CalendarCanvas_SetView(pApp->hwndCanvas, dwMode, NULL);
                    UpdateToolbarTitle(pApp);
                    return 0;
                }
                case IDC_TB_NEW_EVENT:
                case IDM_TRAY_NEW:
                    if (wId == IDM_TRAY_NEW) RestoreMainWindow(pApp);
                    OpenNewEventDialog(pApp, NULL);
                    return 0;
                case IDC_TB_SETTINGS:
                case IDM_TRAY_SETTINGS:
                    if (wId == IDM_TRAY_SETTINGS) RestoreMainWindow(pApp);
                    OpenSettingsDialog(pApp);
                    return 0;
                case IDM_TRAY_OPEN:
                    RestoreMainWindow(pApp);
                    return 0;
                case IDM_TRAY_QUIT:
                    pApp->bForceQuit = TRUE;
                    DestroyWindow(hwnd);
                    return 0;
                default:
                    break;
            }
            break;
        }
        case WM_DESTROY: {
            if (pApp) {
                KillTimer(hwnd, TIMER_ID_NOTIFY);
                Notify_RemoveTray(hwnd);
                if (pApp->hFontUi) DeleteObject(pApp->hFontUi);
                if (pApp->hFontTitle) DeleteObject(pApp->hFontTitle);
                if (pApp->hFontBold) DeleteObject(pApp->hFontBold);
                if (pApp->hBrToolbar) DeleteObject(pApp->hBrToolbar);
                if (pApp->hBrSearch) DeleteObject(pApp->hBrSearch);
                if (pApp->hIconLarge) DestroyIcon(pApp->hIconLarge);
                if (pApp->hIconSmall) DestroyIcon(pApp->hIconSmall);
            }
            PostQuitMessage(0);
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    HANDLE hMutex;
    INITCOMMONCONTROLSEX icc;
    MAIN_APP_STATE app;
    WNDCLASSEXW wc;
    MSG msg;
    int nScreenW, nScreenH, nWinW, nWinH;

    (void)hPrevInstance;
    (void)lpCmdLine;

    EnableAppUserModelId();
    EnablePerMonitorDpi();

    /* Enforce single instance and activate existing window if already running */
    hMutex = CreateMutexW(NULL, TRUE, AGENDA_MUTEX_NAME);
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hwndExisting = FindWindowW(AGENDA_WND_CLASS, NULL);
        if (hwndExisting) {
            PostMessageW(hwndExisting, WM_APP_SHOW_MAIN, 0, 0);
        }
        CloseHandle(hMutex);
        return 0;
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    Theme_InitProcessDarkMode(Theme_IsSystemDarkMode());

    ZeroMemory(&icc, sizeof(icc));
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_DATE_CLASSES | ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    ZeroMemory(&app, sizeof(app));
    app.hInst = hInstance;
    EventList_Init(&app.events);
    FiredList_Init(&app.firedNotifs);
    Settings_InitDefaults(&app.settings);
    Settings_Load(&app.settings);
    Theme_SetDefaultFont(app.settings.szFontName, app.settings.dwFontSizePt);
    Storage_LoadEvents(&app.settings, &app.events);

    if (!CalendarCanvas_Register(hInstance) || !Dialogs_Register(hInstance) || !ThemedCtrls_Register(hInstance)) {
        CoUninitialize();
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = AGENDA_WND_CLASS;

    if (!RegisterClassExW(&wc)) {
        CoUninitialize();
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    nScreenW = GetSystemMetrics(SM_CXSCREEN);
    nScreenH = GetSystemMetrics(SM_CYSCREEN);
    nWinW = (nScreenW * 78) / 100;
    nWinH = (nScreenH * 80) / 100;
    if (nWinW < 960) nWinW = 960;
    if (nWinH < 640) nWinH = 640;

    app.hwndMain = CreateWindowExW(
        0,
        AGENDA_WND_CLASS,
        AGENDA_APP_NAME,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        (nScreenW - nWinW) / 2,
        (nScreenH - nWinH) / 2,
        nWinW,
        nWinH,
        NULL,
        NULL,
        hInstance,
        &app
    );

    if (!app.hwndMain) {
        CoUninitialize();
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    if (app.settings.bStartMinimized) {
        ShowWindow(app.hwndMain, SW_HIDE);
    } else {
        ShowWindow(app.hwndMain, nCmdShow);
        UpdateWindow(app.hwndMain);
    }

    /* Run initial notification check right at startup */
    Notify_CheckDueEvents(app.hwndMain, &app.events, &app.settings, &app.firedNotifs);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EventList_Free(&app.events);
    FiredList_Free(&app.firedNotifs);
    CoUninitialize();
    if (hMutex) CloseHandle(hMutex);
    return (int)msg.wParam;
}
