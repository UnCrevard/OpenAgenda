#include "themed_ctrls.h"
#include "date_utils.h"
#include "theme.h"

#define AGENDA_POPUP_CAL_CLASS   L"AgendaWin32PopupCalClass"
#define AGENDA_POPUP_LIST_CLASS  L"AgendaWin32PopupListClass"

static LONG TcScale(UINT uDpi, LONG lPx)
{
    return MulDiv(lPx, (int)uDpi, 96);
}

static void DrawChevronDown(HDC hdc, int cx, int cy, int size, COLORREF cr)
{
    HPEN hPen = CreatePen(PS_SOLID, 2, cr);
    HPEN hOld = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, cx - size, cy - size / 2, NULL);
    LineTo(hdc, cx, cy + size / 2);
    LineTo(hdc, cx + size, cy - size / 2);
    SelectObject(hdc, hOld);
    DeleteObject(hPen);
}

static void DrawThemedBoxFrame(HDC hdc, const RECT* pRc, COLORREF crBg, COLORREF crBorder, int nRadius, int nBorderWidth)
{
    HBRUSH hBr = CreateSolidBrush(crBg);
    HPEN hPen = CreatePen(PS_SOLID, nBorderWidth, crBorder);
    HBRUSH hOB = (HBRUSH)SelectObject(hdc, hBr);
    HPEN hOP = (HPEN)SelectObject(hdc, hPen);
    RoundRect(hdc, pRc->left, pRc->top, pRc->right, pRc->bottom, nRadius, nRadius);
    SelectObject(hdc, hOB);
    SelectObject(hdc, hOP);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

/* Keeps popup inside the current monitor work area */
static void ClampPopupToMonitor(HWND hwndAnchor, int nPopW, int nPopH, int* pxOut, int* pyOut)
{
    RECT rcAnchor;
    HMONITOR hMon;
    MONITORINFO mi;
    int x, y;

    GetWindowRect(hwndAnchor, &rcAnchor);
    x = rcAnchor.left;
    y = rcAnchor.bottom + 2;

    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(MONITORINFO);
    hMon = MonitorFromWindow(hwndAnchor, MONITOR_DEFAULTTONEAREST);
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        if (x + nPopW > mi.rcWork.right) {
            x = mi.rcWork.right - nPopW;
        }
        if (x < mi.rcWork.left) {
            x = mi.rcWork.left;
        }
        if (y + nPopH > mi.rcWork.bottom) {
            y = rcAnchor.top - nPopH - 2;
        }
        if (y < mi.rcWork.top) {
            y = mi.rcWork.top;
        }
    }
    *pxOut = x;
    *pyOut = y;
}

/* ========================================================================= */
/* 1. CUSTOM THEMED DATE PICKER + POPUP MONTH CALENDAR                       */
/* ========================================================================= */

typedef struct _DATE_PICKER_STATE {
    HWND             hwnd;
    HWND             hwndParent;
    HINSTANCE        hInst;
    const APP_THEME* pTheme;
    HFONT            hFont;
    UINT             uDpi;
    int              nId;
    SYSTEMTIME       stDate;
    BOOL             bPopupOpen;
    BOOL             bHover;
} DATE_PICKER_STATE;

typedef enum _POPUP_CAL_MODE {
    CAL_MODE_DAYS   = 0,
    CAL_MODE_MONTHS = 1,
    CAL_MODE_YEARS  = 2
} POPUP_CAL_MODE;

typedef struct _POPUP_CAL_STATE {
    HWND               hwnd;
    DATE_PICKER_STATE* pOwner;
    POPUP_CAL_MODE     mode;
    SYSTEMTIME         stViewMonth;
    LONG               lYearPageStart;
    LONG               lHoverDay;   /* 1..42 or 0 */
    LONG               lHoverGrid;  /* 0..15 or -1 */
    BOOL               bHoverPrev;
    BOOL               bHoverNext;
    BOOL               bHoverMonthBtn;
    BOOL               bHoverYearBtn;
    BOOL               bHoverToday;
    RECT               rcPrev;
    RECT               rcNext;
    RECT               rcMonthBtn;
    RECT               rcYearBtn;
    RECT               rcTodayBtn;
    RECT               rcDayCells[42];
    RECT               rcGridCells[16];
    SYSTEMTIME         stCellDates[42];
    WCHAR              szYearTyped[5];
    DWORD              dwYearTypedLen;
} POPUP_CAL_STATE;

static void OpenDatePopup(DATE_PICKER_STATE* pS);

static void PopupCal_ApplyYearSelection(POPUP_CAL_STATE* pP, WORD wNewYear)
{
    DATE_PICKER_STATE* pO = pP->pOwner;
    DWORD dwMaxD;
    if (wNewYear < 1601) wNewYear = 1601;
    if (wNewYear > 9999) wNewYear = 9999;

    pP->stViewMonth.wYear = wNewYear;
    pO->stDate.wYear = wNewYear;
    dwMaxD = Date_DaysInMonth(pO->stDate.wYear, pO->stDate.wMonth);
    if (pO->stDate.wDay > dwMaxD) {
        pO->stDate.wDay = (WORD)dwMaxD;
    }
    Date_NormalizeTime(&pO->stDate);
    InvalidateRect(pO->hwnd, NULL, FALSE);
    SendMessageW(pO->hwndParent, WM_COMMAND,
                 MAKEWPARAM((WORD)pO->nId, TCN_DATE_CHANGED), (LPARAM)pO->hwnd);
}

static LRESULT CALLBACK PopupCalProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    POPUP_CAL_STATE* pP = (POPUP_CAL_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCs = (CREATESTRUCTW*)lParam;
            pP = (POPUP_CAL_STATE*)pCs->lpCreateParams;
            pP->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pP);
            return 0;
        }
        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE && pP) {
                if (pP->pOwner) {
                    pP->pOwner->bPopupOpen = FALSE;
                    InvalidateRect(pP->pOwner->hwnd, NULL, FALSE);
                }
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                if (pP && pP->mode != CAL_MODE_DAYS) {
                    pP->mode = CAL_MODE_DAYS;
                    pP->lHoverGrid = -1;
                    pP->dwYearTypedLen = 0;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                } else {
                    HWND hwndBack = (pP && pP->pOwner) ? pP->pOwner->hwnd : NULL;
                    DestroyWindow(hwnd);
                    if (hwndBack) SetFocus(hwndBack);
                    return 0;
                }
            }
            if (pP && pP->pOwner && pP->mode == CAL_MODE_YEARS && wParam == VK_BACK && pP->dwYearTypedLen > 0) {
                pP->dwYearTypedLen--;
                pP->szYearTyped[pP->dwYearTypedLen] = L'\0';
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        }
        case WM_CHAR: {
            if (pP && pP->pOwner && pP->mode == CAL_MODE_YEARS && wParam >= L'0' && wParam <= L'9') {
                if (pP->dwYearTypedLen >= 4) pP->dwYearTypedLen = 0;
                pP->szYearTyped[pP->dwYearTypedLen++] = (WCHAR)wParam;
                pP->szYearTyped[pP->dwYearTypedLen] = L'\0';

                if (pP->dwYearTypedLen == 4) {
                    LONG lVal = _wtol(pP->szYearTyped);
                    pP->dwYearTypedLen = 0;
                    if (lVal >= 1601 && lVal <= 9999) {
                        PopupCal_ApplyYearSelection(pP, (WORD)lVal);
                        pP->lYearPageStart = lVal - (lVal % 16);
                        if (pP->lYearPageStart < 1601) pP->lYearPageStart = 1601;
                        pP->mode = CAL_MODE_DAYS;
                        pP->lHoverGrid = -1;
                    }
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (pP) {
                short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                int nDir = (zDelta > 0) ? -1 : 1;
                if (pP->mode == CAL_MODE_DAYS) {
                    Date_AddMonths(&pP->stViewMonth, nDir);
                } else if (pP->mode == CAL_MODE_MONTHS) {
                    LONG lY = (LONG)pP->stViewMonth.wYear + nDir;
                    if (lY < 1601) lY = 1601;
                    if (lY > 9999) lY = 9999;
                    pP->stViewMonth.wYear = (WORD)lY;
                } else {
                    pP->lYearPageStart += nDir * 4;
                    if (pP->lYearPageStart < 1601) pP->lYearPageStart = 1601;
                    if (pP->lYearPageStart > 9984) pP->lYearPageStart = 9984;
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        }
        case WM_MOUSEMOVE: {
            if (pP) {
                POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
                LONG lNewHoverDay = 0;
                LONG lNewHoverGrid = -1;
                BOOL bP = PtInRect(&pP->rcPrev, pt);
                BOOL bN = PtInRect(&pP->rcNext, pt);
                BOOL bM = (pP->mode == CAL_MODE_DAYS) && PtInRect(&pP->rcMonthBtn, pt);
                BOOL bY = (pP->mode != CAL_MODE_YEARS) && PtInRect(&pP->rcYearBtn, pt);
                BOOL bT = PtInRect(&pP->rcTodayBtn, pt);
                int i;

                if (pP->mode == CAL_MODE_DAYS) {
                    for (i = 0; i < 42; ++i) {
                        if (PtInRect(&pP->rcDayCells[i], pt)) {
                            lNewHoverDay = (LONG)(i + 1);
                            break;
                        }
                    }
                } else {
                    int nGridCount = (pP->mode == CAL_MODE_MONTHS) ? 12 : 16;
                    for (i = 0; i < nGridCount; ++i) {
                        if (PtInRect(&pP->rcGridCells[i], pt)) {
                            lNewHoverGrid = (LONG)i;
                            break;
                        }
                    }
                }
                if (bP != pP->bHoverPrev || bN != pP->bHoverNext ||
                    bM != pP->bHoverMonthBtn || bY != pP->bHoverYearBtn ||
                    bT != pP->bHoverToday || lNewHoverDay != pP->lHoverDay ||
                    lNewHoverGrid != pP->lHoverGrid) {
                    pP->bHoverPrev = bP;
                    pP->bHoverNext = bN;
                    pP->bHoverMonthBtn = bM;
                    pP->bHoverYearBtn = bY;
                    pP->bHoverToday = bT;
                    pP->lHoverDay = lNewHoverDay;
                    pP->lHoverGrid = lNewHoverGrid;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (pP && pP->pOwner) {
                POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
                DATE_PICKER_STATE* pO = pP->pOwner;
                int i;

                if (PtInRect(&pP->rcPrev, pt)) {
                    if (pP->mode == CAL_MODE_DAYS) {
                        Date_AddMonths(&pP->stViewMonth, -1);
                    } else if (pP->mode == CAL_MODE_MONTHS) {
                        if (pP->stViewMonth.wYear > 1601) pP->stViewMonth.wYear--;
                    } else {
                        pP->lYearPageStart -= 16;
                        if (pP->lYearPageStart < 1601) pP->lYearPageStart = 1601;
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (PtInRect(&pP->rcNext, pt)) {
                    if (pP->mode == CAL_MODE_DAYS) {
                        Date_AddMonths(&pP->stViewMonth, 1);
                    } else if (pP->mode == CAL_MODE_MONTHS) {
                        if (pP->stViewMonth.wYear < 9999) pP->stViewMonth.wYear++;
                    } else {
                        pP->lYearPageStart += 16;
                        if (pP->lYearPageStart > 9984) pP->lYearPageStart = 9984;
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (pP->mode == CAL_MODE_DAYS && PtInRect(&pP->rcMonthBtn, pt)) {
                    pP->mode = CAL_MODE_MONTHS;
                    pP->lHoverGrid = -1;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (pP->mode != CAL_MODE_YEARS && PtInRect(&pP->rcYearBtn, pt)) {
                    pP->mode = CAL_MODE_YEARS;
                    pP->lYearPageStart = (LONG)pP->stViewMonth.wYear - ((LONG)pP->stViewMonth.wYear % 16);
                    if (pP->lYearPageStart < 1601) pP->lYearPageStart = 1601;
                    pP->lHoverGrid = -1;
                    pP->dwYearTypedLen = 0;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (PtInRect(&pP->rcTodayBtn, pt)) {
                    SYSTEMTIME stNow;
                    GetLocalTime(&stNow);
                    if (pP->mode == CAL_MODE_YEARS) {
                        PopupCal_ApplyYearSelection(pP, stNow.wYear);
                        pP->mode = CAL_MODE_DAYS;
                        pP->lHoverGrid = -1;
                        InvalidateRect(hwnd, NULL, FALSE);
                        return 0;
                    }
                    pO->stDate.wYear = stNow.wYear;
                    pO->stDate.wMonth = stNow.wMonth;
                    pO->stDate.wDay = stNow.wDay;
                    Date_NormalizeTime(&pO->stDate);
                    pO->bPopupOpen = FALSE;
                    InvalidateRect(pO->hwnd, NULL, FALSE);
                    SendMessageW(pO->hwndParent, WM_COMMAND,
                                 MAKEWPARAM((WORD)pO->nId, TCN_DATE_CHANGED), (LPARAM)pO->hwnd);
                    DestroyWindow(hwnd);
                    SetFocus(pO->hwnd);
                    return 0;
                }

                if (pP->mode == CAL_MODE_DAYS) {
                    for (i = 0; i < 42; ++i) {
                        if (PtInRect(&pP->rcDayCells[i], pt)) {
                            pO->stDate.wYear = pP->stCellDates[i].wYear;
                            pO->stDate.wMonth = pP->stCellDates[i].wMonth;
                            pO->stDate.wDay = pP->stCellDates[i].wDay;
                            Date_NormalizeTime(&pO->stDate);
                            pO->bPopupOpen = FALSE;
                            InvalidateRect(pO->hwnd, NULL, FALSE);
                            SendMessageW(pO->hwndParent, WM_COMMAND,
                                         MAKEWPARAM((WORD)pO->nId, TCN_DATE_CHANGED), (LPARAM)pO->hwnd);
                            DestroyWindow(hwnd);
                            SetFocus(pO->hwnd);
                            return 0;
                        }
                    }
                } else if (pP->mode == CAL_MODE_MONTHS) {
                    for (i = 0; i < 12; ++i) {
                        if (PtInRect(&pP->rcGridCells[i], pt)) {
                            DWORD dwMaxD;
                            pP->stViewMonth.wMonth = (WORD)(i + 1);
                            pO->stDate.wYear = pP->stViewMonth.wYear;
                            pO->stDate.wMonth = (WORD)(i + 1);
                            dwMaxD = Date_DaysInMonth(pO->stDate.wYear, pO->stDate.wMonth);
                            if (pO->stDate.wDay > dwMaxD) pO->stDate.wDay = (WORD)dwMaxD;
                            Date_NormalizeTime(&pO->stDate);
                            InvalidateRect(pO->hwnd, NULL, FALSE);
                            SendMessageW(pO->hwndParent, WM_COMMAND,
                                         MAKEWPARAM((WORD)pO->nId, TCN_DATE_CHANGED), (LPARAM)pO->hwnd);
                            pP->mode = CAL_MODE_DAYS;
                            pP->lHoverGrid = -1;
                            InvalidateRect(hwnd, NULL, FALSE);
                            return 0;
                        }
                    }
                } else if (pP->mode == CAL_MODE_YEARS) {
                    for (i = 0; i < 16; ++i) {
                        if (PtInRect(&pP->rcGridCells[i], pt)) {
                            LONG lPicked = pP->lYearPageStart + (LONG)i;
                            if (lPicked >= 1601 && lPicked <= 9999) {
                                PopupCal_ApplyYearSelection(pP, (WORD)lPicked);
                                pP->mode = CAL_MODE_DAYS;
                                pP->lHoverGrid = -1;
                                pP->dwYearTypedLen = 0;
                                InvalidateRect(hwnd, NULL, FALSE);
                            }
                            return 0;
                        }
                    }
                }
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pP && pP->pOwner && pP->pOwner->pTheme) {
                const APP_THEME* pT = pP->pOwner->pTheme;
                UINT d = pP->pOwner->uDpi;
                RECT rc;
                int nW, nH;
                HDC hdcMem;
                HBITMAP hbm, hOldBmp;
                HFONT hOldFont;
                SYSTEMTIME stNow;
                int r, c;
                LONG lPad = TcScale(d, 10);
                LONG lHeadH = TcScale(d, 36);
                LONG lDowH = TcScale(d, 22);
                LONG lFootH = TcScale(d, 32);

                GetClientRect(hwnd, &rc);
                nW = rc.right - rc.left;
                nH = rc.bottom - rc.top;
                hdcMem = CreateCompatibleDC(hdc);
                hbm = CreateCompatibleBitmap(hdc, nW, nH);
                hOldBmp = (HBITMAP)SelectObject(hdcMem, hbm);

                {
                    HBRUSH hBr = CreateSolidBrush(pT->crBgSurface);
                    HPEN hPen = CreatePen(PS_SOLID, 1, pT->crBorderStrong);
                    HBRUSH hOB = (HBRUSH)SelectObject(hdcMem, hBr);
                    HPEN hOP = (HPEN)SelectObject(hdcMem, hPen);
                    Rectangle(hdcMem, 0, 0, nW, nH);
                    SelectObject(hdcMem, hOB);
                    SelectObject(hdcMem, hOP);
                    DeleteObject(hBr);
                    DeleteObject(hPen);
                }

                GetLocalTime(&stNow);
                SetBkMode(hdcMem, TRANSPARENT);
                hOldFont = (HFONT)SelectObject(hdcMem, pP->pOwner->hFont);

                /* Header Prev/Next buttons */
                pP->rcPrev.left = lPad;
                pP->rcPrev.top = TcScale(d, 6);
                pP->rcPrev.right = lPad + TcScale(d, 28);
                pP->rcPrev.bottom = lHeadH - TcScale(d, 4);

                pP->rcNext.left = nW - lPad - TcScale(d, 28);
                pP->rcNext.top = TcScale(d, 6);
                pP->rcNext.right = nW - lPad;
                pP->rcNext.bottom = lHeadH - TcScale(d, 4);

                if (pP->bHoverPrev) DrawThemedBoxFrame(hdcMem, &pP->rcPrev, pT->crBgHover, pT->crBorderSubtle, TcScale(d, 4), 1);
                if (pP->bHoverNext) DrawThemedBoxFrame(hdcMem, &pP->rcNext, pT->crBgHover, pT->crBorderSubtle, TcScale(d, 4), 1);

                SetTextColor(hdcMem, pT->crTextPrimary);
                DrawTextW(hdcMem, L"<", -1, &pP->rcPrev, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                DrawTextW(hdcMem, L">", -1, &pP->rcNext, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

                if (pP->mode == CAL_MODE_DAYS) {
                    WCHAR szMonth[32];
                    WCHAR szYear[16];
                    LONG lCenterL = pP->rcPrev.right + TcScale(d, 6);
                    LONG lCenterR = pP->rcNext.left - TcScale(d, 6);
                    LONG lYearW = TcScale(d, 70);
                    LONG lCellW = (nW - lPad * 2) / 7;
                    LONG lCellH = (nH - lHeadH - lDowH - lFootH - TcScale(d, 6)) / 6;
                    SYSTEMTIME stFirst = pP->stViewMonth;
                    SYSTEMTIME stGridStart;

                    pP->rcMonthBtn.left = lCenterL;
                    pP->rcMonthBtn.top = pP->rcPrev.top;
                    pP->rcMonthBtn.right = lCenterR - lYearW - TcScale(d, 4);
                    pP->rcMonthBtn.bottom = pP->rcPrev.bottom;

                    pP->rcYearBtn.left = pP->rcMonthBtn.right + TcScale(d, 4);
                    pP->rcYearBtn.top = pP->rcPrev.top;
                    pP->rcYearBtn.right = lCenterR;
                    pP->rcYearBtn.bottom = pP->rcPrev.bottom;

                    DrawThemedBoxFrame(hdcMem, &pP->rcMonthBtn,
                                       pP->bHoverMonthBtn ? pT->crBgHover : pT->crBgHeader,
                                       pP->bHoverMonthBtn ? pT->crAccent : pT->crBorderSubtle,
                                       TcScale(d, 4), 1);
                    DrawThemedBoxFrame(hdcMem, &pP->rcYearBtn,
                                       pP->bHoverYearBtn ? pT->crBgHover : pT->crBgHeader,
                                       pP->bHoverYearBtn ? pT->crAccent : pT->crBorderSubtle,
                                       TcScale(d, 4), 1);

                    Date_GetMonthName(pP->stViewMonth.wMonth, szMonth, 32);
                    _snwprintf(szYear, 16, L"%u", (UINT)pP->stViewMonth.wYear);
                    szYear[15] = L'\0';

                    {
                        RECT rcMT = pP->rcMonthBtn;
                        RECT rcYT = pP->rcYearBtn;
                        rcMT.right -= TcScale(d, 14);
                        rcMT.left += TcScale(d, 4);
                        rcYT.right -= TcScale(d, 14);
                        rcYT.left += TcScale(d, 4);
                        SetTextColor(hdcMem, pT->crTextPrimary);
                        DrawTextW(hdcMem, szMonth, -1, &rcMT, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                        DrawTextW(hdcMem, szYear, -1, &rcYT, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                    }
                    DrawChevronDown(hdcMem, pP->rcMonthBtn.right - TcScale(d, 9),
                                    (pP->rcMonthBtn.top + pP->rcMonthBtn.bottom) / 2,
                                    TcScale(d, 3), pT->crTextSecondary);
                    DrawChevronDown(hdcMem, pP->rcYearBtn.right - TcScale(d, 9),
                                    (pP->rcYearBtn.top + pP->rcYearBtn.bottom) / 2,
                                    TcScale(d, 3), pT->crTextSecondary);

                    /* Weekday initials */
                    SetTextColor(hdcMem, pT->crTextMuted);
                    for (c = 0; c < 7; ++c) {
                        WCHAR szWd[16];
                        RECT rcDow = {lPad + c * lCellW, lHeadH, lPad + (c + 1) * lCellW, lHeadH + lDowH};
                        Date_GetWeekdayShort((DWORD)c, szWd, 16);
                        szWd[2] = L'\0';
                        DrawTextW(hdcMem, szWd, -1, &rcDow, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                    }

                    stFirst.wDay = 1;
                    Date_NormalizeTime(&stFirst);
                    Date_GetWeekStartMonday(&stFirst, &stGridStart);

                    /* 42 Day cells */
                    for (r = 0; r < 6; ++r) {
                        for (c = 0; c < 7; ++c) {
                            int idx = r * 7 + c;
                            SYSTEMTIME stCell = stGridStart;
                            RECT rcCell = {
                                lPad + c * lCellW,
                                lHeadH + lDowH + r * lCellH,
                                lPad + (c + 1) * lCellW,
                                lHeadH + lDowH + (r + 1) * lCellH
                            };
                            BOOL bInMonth;
                            BOOL bSelected;
                            BOOL bToday;
                            WCHAR szDay[8];

                            Date_AddDays(&stCell, idx);
                            pP->rcDayCells[idx] = rcCell;
                            pP->stCellDates[idx] = stCell;

                            bInMonth = (stCell.wMonth == pP->stViewMonth.wMonth);
                            bSelected = (Date_CompareDate(&stCell, &pP->pOwner->stDate) == 0);
                            bToday = (Date_CompareDate(&stCell, &stNow) == 0);

                            if (bSelected) {
                                RECT rcPill = rcCell;
                                InflateRect(&rcPill, -2, -2);
                                DrawThemedBoxFrame(hdcMem, &rcPill, pT->crAccent, pT->crAccent, TcScale(d, 6), 1);
                                SetTextColor(hdcMem, pT->crAccentText);
                            } else if (pP->lHoverDay == idx + 1) {
                                RECT rcPill = rcCell;
                                InflateRect(&rcPill, -2, -2);
                                DrawThemedBoxFrame(hdcMem, &rcPill, pT->crBgHover, pT->crBorderSubtle, TcScale(d, 6), 1);
                                SetTextColor(hdcMem, bInMonth ? pT->crTextPrimary : pT->crTextMuted);
                            } else if (bToday) {
                                RECT rcPill = rcCell;
                                InflateRect(&rcPill, -2, -2);
                                DrawThemedBoxFrame(hdcMem, &rcPill, pT->crBgToday, pT->crAccent, TcScale(d, 6), 1);
                                SetTextColor(hdcMem, pT->crTextPrimary);
                            } else {
                                SetTextColor(hdcMem, bInMonth ? pT->crTextPrimary : pT->crTextMuted);
                            }

                            _snwprintf(szDay, 8, L"%u", (UINT)stCell.wDay);
                            DrawTextW(hdcMem, szDay, -1, &rcCell, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                        }
                    }

                    pP->rcTodayBtn.left = lPad;
                    pP->rcTodayBtn.top = nH - lFootH;
                    pP->rcTodayBtn.right = nW - lPad;
                    pP->rcTodayBtn.bottom = nH - TcScale(d, 6);
                    DrawThemedBoxFrame(hdcMem, &pP->rcTodayBtn,
                                       pP->bHoverToday ? pT->crBgHover : pT->crBgHeader,
                                       pT->crBorderSubtle, TcScale(d, 4), 1);
                    SetTextColor(hdcMem, pT->crAccent);
                    DrawTextW(hdcMem, L"Aujourd'hui", -1, &pP->rcTodayBtn, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                } else if (pP->mode == CAL_MODE_MONTHS) {
                    static const WCHAR* szMonthsShort[12] = {
                        L"Jan", L"F\u00E9v", L"Mar", L"Avr",
                        L"Mai", L"Juin", L"Juil", L"Ao\u00FBt",
                        L"Sep", L"Oct", L"Nov", L"D\u00E9c"
                    };
                    WCHAR szYear[16];
                    LONG lGridW = (nW - lPad * 2) / 3;
                    LONG lGridH = (nH - lHeadH - lFootH - TcScale(d, 10)) / 4;

                    SetRectEmpty(&pP->rcMonthBtn);
                    pP->rcYearBtn.left = pP->rcPrev.right + TcScale(d, 12);
                    pP->rcYearBtn.top = pP->rcPrev.top;
                    pP->rcYearBtn.right = pP->rcNext.left - TcScale(d, 12);
                    pP->rcYearBtn.bottom = pP->rcPrev.bottom;

                    DrawThemedBoxFrame(hdcMem, &pP->rcYearBtn,
                                       pP->bHoverYearBtn ? pT->crBgHover : pT->crBgHeader,
                                       pP->bHoverYearBtn ? pT->crAccent : pT->crBorderSubtle,
                                       TcScale(d, 4), 1);
                    _snwprintf(szYear, 16, L"%u", (UINT)pP->stViewMonth.wYear);
                    szYear[15] = L'\0';
                    SetTextColor(hdcMem, pT->crTextPrimary);
                    DrawTextW(hdcMem, szYear, -1, &pP->rcYearBtn, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                    DrawChevronDown(hdcMem, pP->rcYearBtn.right - TcScale(d, 12),
                                    (pP->rcYearBtn.top + pP->rcYearBtn.bottom) / 2,
                                    TcScale(d, 3), pT->crTextSecondary);

                    for (r = 0; r < 4; ++r) {
                        for (c = 0; c < 3; ++c) {
                            int idx = r * 3 + c;
                            RECT rcCell = {
                                lPad + c * lGridW,
                                lHeadH + TcScale(d, 4) + r * lGridH,
                                lPad + (c + 1) * lGridW,
                                lHeadH + TcScale(d, 4) + (r + 1) * lGridH
                            };
                            RECT rcPill = rcCell;
                            BOOL bSelected = ((WORD)(idx + 1) == pP->pOwner->stDate.wMonth &&
                                              pP->stViewMonth.wYear == pP->pOwner->stDate.wYear);
                            BOOL bNowMonth = ((WORD)(idx + 1) == stNow.wMonth &&
                                              pP->stViewMonth.wYear == stNow.wYear);

                            pP->rcGridCells[idx] = rcCell;
                            InflateRect(&rcPill, -3, -3);
                            if (bSelected) {
                                DrawThemedBoxFrame(hdcMem, &rcPill, pT->crAccent, pT->crAccent, TcScale(d, 6), 1);
                                SetTextColor(hdcMem, pT->crAccentText);
                            } else if (pP->lHoverGrid == idx) {
                                DrawThemedBoxFrame(hdcMem, &rcPill, pT->crBgHover, pT->crBorderSubtle, TcScale(d, 6), 1);
                                SetTextColor(hdcMem, pT->crTextPrimary);
                            } else if (bNowMonth) {
                                DrawThemedBoxFrame(hdcMem, &rcPill, pT->crBgToday, pT->crAccent, TcScale(d, 6), 1);
                                SetTextColor(hdcMem, pT->crTextPrimary);
                            } else {
                                DrawThemedBoxFrame(hdcMem, &rcPill, pT->crBgHeader, pT->crBorderSubtle, TcScale(d, 6), 1);
                                SetTextColor(hdcMem, pT->crTextPrimary);
                            }
                            DrawTextW(hdcMem, szMonthsShort[idx], -1, &rcCell, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                        }
                    }

                    pP->rcTodayBtn.left = lPad;
                    pP->rcTodayBtn.top = nH - lFootH;
                    pP->rcTodayBtn.right = nW - lPad;
                    pP->rcTodayBtn.bottom = nH - TcScale(d, 6);
                    DrawThemedBoxFrame(hdcMem, &pP->rcTodayBtn,
                                       pP->bHoverToday ? pT->crBgHover : pT->crBgHeader,
                                       pT->crBorderSubtle, TcScale(d, 4), 1);
                    SetTextColor(hdcMem, pT->crAccent);
                    DrawTextW(hdcMem, L"Aujourd'hui", -1, &pP->rcTodayBtn, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                } else {
                    /* CAL_MODE_YEARS: 4x4 grid of 16 years */
                    WCHAR szRange[48];
                    WCHAR szFoot[48];
                    LONG lGridW = (nW - lPad * 2) / 4;
                    LONG lGridH = (nH - lHeadH - lFootH - TcScale(d, 10)) / 4;
                    LONG lPageEnd = pP->lYearPageStart + 15;
                    if (lPageEnd > 9999) lPageEnd = 9999;

                    SetRectEmpty(&pP->rcMonthBtn);
                    SetRectEmpty(&pP->rcYearBtn);

                    if (pP->dwYearTypedLen > 0) {
                        _snwprintf(szRange, 48, L"Saisie : %s_", pP->szYearTyped);
                    } else {
                        _snwprintf(szRange, 48, L"%ld \u2013 %ld", (long)pP->lYearPageStart, (long)lPageEnd);
                    }
                    szRange[47] = L'\0';
                    {
                        RECT rcHdr = {pP->rcPrev.right + 4, pP->rcPrev.top, pP->rcNext.left - 4, pP->rcPrev.bottom};
                        SetTextColor(hdcMem, pT->crTextPrimary);
                        DrawTextW(hdcMem, szRange, -1, &rcHdr, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                    }

                    for (r = 0; r < 4; ++r) {
                        for (c = 0; c < 4; ++c) {
                            int idx = r * 4 + c;
                            LONG lYearVal = pP->lYearPageStart + (LONG)idx;
                            RECT rcCell = {
                                lPad + c * lGridW,
                                lHeadH + TcScale(d, 4) + r * lGridH,
                                lPad + (c + 1) * lGridW,
                                lHeadH + TcScale(d, 4) + (r + 1) * lGridH
                            };
                            RECT rcPill = rcCell;
                            WCHAR szY[12];

                            if (lYearVal > 9999) {
                                SetRectEmpty(&pP->rcGridCells[idx]);
                                continue;
                            }
                            pP->rcGridCells[idx] = rcCell;
                            InflateRect(&rcPill, -3, -3);

                            if ((WORD)lYearVal == pP->pOwner->stDate.wYear) {
                                DrawThemedBoxFrame(hdcMem, &rcPill, pT->crAccent, pT->crAccent, TcScale(d, 6), 1);
                                SetTextColor(hdcMem, pT->crAccentText);
                            } else if (pP->lHoverGrid == idx) {
                                DrawThemedBoxFrame(hdcMem, &rcPill, pT->crBgHover, pT->crBorderSubtle, TcScale(d, 6), 1);
                                SetTextColor(hdcMem, pT->crTextPrimary);
                            } else if ((WORD)lYearVal == stNow.wYear) {
                                DrawThemedBoxFrame(hdcMem, &rcPill, pT->crBgToday, pT->crAccent, TcScale(d, 6), 1);
                                SetTextColor(hdcMem, pT->crTextPrimary);
                            } else {
                                DrawThemedBoxFrame(hdcMem, &rcPill, pT->crBgHeader, pT->crBorderSubtle, TcScale(d, 6), 1);
                                SetTextColor(hdcMem, pT->crTextPrimary);
                            }

                            _snwprintf(szY, 12, L"%ld", (long)lYearVal);
                            DrawTextW(hdcMem, szY, -1, &rcCell, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                        }
                    }

                    pP->rcTodayBtn.left = lPad;
                    pP->rcTodayBtn.top = nH - lFootH;
                    pP->rcTodayBtn.right = nW - lPad;
                    pP->rcTodayBtn.bottom = nH - TcScale(d, 6);
                    DrawThemedBoxFrame(hdcMem, &pP->rcTodayBtn,
                                       pP->bHoverToday ? pT->crBgHover : pT->crBgHeader,
                                       pT->crBorderSubtle, TcScale(d, 4), 1);
                    _snwprintf(szFoot, 48, L"Ann\u00E9e actuelle (%u)", (UINT)stNow.wYear);
                    szFoot[47] = L'\0';
                    SetTextColor(hdcMem, pT->crAccent);
                    DrawTextW(hdcMem, szFoot, -1, &pP->rcTodayBtn, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                }

                SelectObject(hdcMem, hOldFont);
                BitBlt(hdc, 0, 0, nW, nH, hdcMem, 0, 0, SRCCOPY);
                SelectObject(hdcMem, hOldBmp);
                DeleteObject(hbm);
                DeleteDC(hdcMem);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (pP) {
                if (pP->pOwner) pP->pOwner->bPopupOpen = FALSE;
                free(pP);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

static void OpenDatePopup(DATE_PICKER_STATE* pS)
{
    POPUP_CAL_STATE* pP;
    int nW = TcScale(pS->uDpi, 286);
    int nH = TcScale(pS->uDpi, 284);
    int x, y;
    HWND hwndPop;

    if (pS->bPopupOpen) return;
    pP = (POPUP_CAL_STATE*)calloc(1, sizeof(POPUP_CAL_STATE));
    if (!pP) return;

    pP->pOwner = pS;
    pP->mode = CAL_MODE_DAYS;
    pP->stViewMonth = pS->stDate;
    pP->stViewMonth.wDay = 1;
    pP->lYearPageStart = (LONG)pS->stDate.wYear - ((LONG)pS->stDate.wYear % 16);
    if (pP->lYearPageStart < 1601) pP->lYearPageStart = 1601;
    pP->lHoverGrid = -1;

    ClampPopupToMonitor(pS->hwnd, nW, nH, &x, &y);
    pS->bPopupOpen = TRUE;
    InvalidateRect(pS->hwnd, NULL, FALSE);

    hwndPop = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        AGENDA_POPUP_CAL_CLASS,
        L"",
        WS_POPUP,
        x, y, nW, nH,
        pS->hwndParent, NULL, pS->hInst, pP
    );
    if (hwndPop) {
        ShowWindow(hwndPop, SW_SHOW);
        SetFocus(hwndPop);
    } else {
        pS->bPopupOpen = FALSE;
        free(pP);
    }
}

static LRESULT CALLBACK DatePickerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    DATE_PICKER_STATE* pS = (DATE_PICKER_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCs = (CREATESTRUCTW*)lParam;
            pS = (DATE_PICKER_STATE*)pCs->lpCreateParams;
            pS->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pS);
            return 0;
        }
        case WM_ENABLE:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            if (pS && IsWindowEnabled(hwnd)) {
                OpenDatePopup(pS);
            }
            return 0;

        case WM_KEYDOWN:
            if (pS && IsWindowEnabled(hwnd)) {
                if (wParam == VK_SPACE || wParam == VK_RETURN || wParam == VK_DOWN) {
                    OpenDatePopup(pS);
                    return 0;
                }
                if (wParam == VK_LEFT) {
                    Date_AddDays(&pS->stDate, -1);
                    InvalidateRect(hwnd, NULL, FALSE);
                    SendMessageW(pS->hwndParent, WM_COMMAND, MAKEWPARAM((WORD)pS->nId, TCN_DATE_CHANGED), (LPARAM)hwnd);
                    return 0;
                }
                if (wParam == VK_RIGHT) {
                    Date_AddDays(&pS->stDate, 1);
                    InvalidateRect(hwnd, NULL, FALSE);
                    SendMessageW(pS->hwndParent, WM_COMMAND, MAKEWPARAM((WORD)pS->nId, TCN_DATE_CHANGED), (LPARAM)hwnd);
                    return 0;
                }
            }
            break;

        case WM_GETDLGCODE:
            return DLGC_WANTARROWS;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pS && pS->pTheme) {
                const APP_THEME* pT = pS->pTheme;
                UINT d = pS->uDpi;
                RECT rc;
                BOOL bEnabled = IsWindowEnabled(hwnd);
                BOOL bFocused = (GetFocus() == hwnd) || pS->bPopupOpen;
                COLORREF crBg = bEnabled ? pT->crBgSurface : pT->crBgGridAlt;
                COLORREF crBorder = bFocused ? pT->crAccent : (bEnabled ? pT->crBorderStrong : pT->crBorderSubtle);
                COLORREF crText = bEnabled ? pT->crTextPrimary : pT->crTextMuted;
                WCHAR szDow[16];
                WCHAR szDisp[64];
                RECT rcText;
                HFONT hOldF;
                DWORD dwMonIdx;

                GetClientRect(hwnd, &rc);
                {
                    HBRUSH hBrWin = CreateSolidBrush(pT->crBgWindow);
                    FillRect(hdc, &rc, hBrWin);
                    DeleteObject(hBrWin);
                }

                DrawThemedBoxFrame(hdc, &rc, crBg, crBorder, TcScale(d, 5), bFocused ? 2 : 1);

                /* Small calendar icon on left */
                {
                    LONG lIx = rc.left + TcScale(d, 9);
                    LONG lIw = TcScale(d, 13);
                    LONG lIy = rc.top + (rc.bottom - rc.top - lIw) / 2;
                    RECT rcIcon = {lIx, lIy, lIx + lIw, lIy + lIw};
                    RECT rcTop = {lIx, lIy, lIx + lIw, lIy + TcScale(d, 4)};
                    HBRUSH hBrTop = CreateSolidBrush(bEnabled ? pT->crAccent : pT->crTextMuted);
                    HPEN hPenIc = CreatePen(PS_SOLID, 1, crText);
                    HBRUSH hOB = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    HPEN hOP = (HPEN)SelectObject(hdc, hPenIc);
                    Rectangle(hdc, rcIcon.left, rcIcon.top, rcIcon.right, rcIcon.bottom);
                    FillRect(hdc, &rcTop, hBrTop);
                    SelectObject(hdc, hOB);
                    SelectObject(hdc, hOP);
                    DeleteObject(hPenIc);
                    DeleteObject(hBrTop);
                }

                dwMonIdx = Date_MondayBasedDayOfWeek(&pS->stDate);
                Date_GetWeekdayShort(dwMonIdx, szDow, 16);
                _snwprintf(szDisp, 64, L"%s %02u/%02u/%04u",
                           szDow, (UINT)pS->stDate.wDay, (UINT)pS->stDate.wMonth, (UINT)pS->stDate.wYear);
                szDisp[63] = L'\0';

                rcText.left = rc.left + TcScale(d, 28);
                rcText.top = rc.top;
                rcText.right = rc.right - TcScale(d, 22);
                rcText.bottom = rc.bottom;
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, crText);
                hOldF = (HFONT)SelectObject(hdc, pS->hFont);
                DrawTextW(hdc, szDisp, -1, &rcText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                SelectObject(hdc, hOldF);

                DrawChevronDown(hdc, rc.right - TcScale(d, 12), (rc.top + rc.bottom) / 2,
                                TcScale(d, 4), bEnabled ? pT->crTextSecondary : pT->crTextMuted);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (pS) {
                free(pS);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

/* ========================================================================= */
/* 2. SHARED POPUP LIST (USED BY BOTH TIME PICKER & COMBOBOX)                */
/* ========================================================================= */

typedef struct _POPUP_LIST_ITEM {
    WCHAR    szText[160];
    COLORREF crSwatch;
    BOOL     bHasSwatch;
} POPUP_LIST_ITEM;

typedef void (*PFN_POPUP_LIST_PICK)(void* pCtx, LONG lIndex);

typedef struct _POPUP_LIST_STATE {
    HWND               hwnd;
    HWND               hwndAnchor;
    const APP_THEME*   pTheme;
    HFONT              hFont;
    UINT               uDpi;
    POPUP_LIST_ITEM*   pItems;
    DWORD              dwCount;
    LONG               lSelected;
    LONG               lHover;
    LONG               lScrollIdx;
    LONG               lVisibleRows;
    LONG               lRowH;
    PFN_POPUP_LIST_PICK pfnOnPick;
    void*              pPickCtx;
} POPUP_LIST_STATE;

static void PopupList_UpdateScrollBar(POPUP_LIST_STATE* pP)
{
    SCROLLINFO si;
    LONG lMax = (LONG)pP->dwCount - pP->lVisibleRows;
    if (lMax < 0) lMax = 0;

    if (pP->lScrollIdx < 0) pP->lScrollIdx = 0;
    if (pP->lScrollIdx > lMax) pP->lScrollIdx = lMax;

    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_ALL;
    si.nMin = 0;
    si.nMax = (pP->dwCount > 0) ? (int)(pP->dwCount - 1) : 0;
    si.nPage = (UINT)pP->lVisibleRows;
    si.nPos = pP->lScrollIdx;
    SetScrollInfo(pP->hwnd, SB_VERT, &si, TRUE);
    ShowScrollBar(pP->hwnd, SB_VERT, (LONG)pP->dwCount > pP->lVisibleRows);
}

static LRESULT CALLBACK PopupListProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    POPUP_LIST_STATE* pP = (POPUP_LIST_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCs = (CREATESTRUCTW*)lParam;
            pP = (POPUP_LIST_STATE*)pCs->lpCreateParams;
            pP->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pP);
            if (pP->pTheme) {
                Theme_ApplyToControl(hwnd, pP->pTheme->bDarkMode, FALSE);
            }
            PopupList_UpdateScrollBar(pP);
            return 0;
        }
        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE && pP) {
                if (pP->pfnOnPick) {
                    PFN_POPUP_LIST_PICK pfn = pP->pfnOnPick;
                    void* pCtx = pP->pPickCtx;
                    pP->pfnOnPick = NULL;
                    pfn(pCtx, -1); /* Cancelled */
                }
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_VSCROLL: {
            if (pP) {
                SCROLLINFO si;
                LONG lNewPos;
                si.cbSize = sizeof(SCROLLINFO);
                si.fMask = SIF_ALL;
                GetScrollInfo(hwnd, SB_VERT, &si);
                lNewPos = si.nPos;
                switch (LOWORD(wParam)) {
                    case SB_LINEUP:     lNewPos -= 1; break;
                    case SB_LINEDOWN:   lNewPos += 1; break;
                    case SB_PAGEUP:     lNewPos -= pP->lVisibleRows; break;
                    case SB_PAGEDOWN:   lNewPos += pP->lVisibleRows; break;
                    case SB_THUMBTRACK: lNewPos = si.nTrackPos; break;
                    default: break;
                }
                pP->lScrollIdx = lNewPos;
                PopupList_UpdateScrollBar(pP);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            if (pP) {
                short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                pP->lScrollIdx -= (zDelta / WHEEL_DELTA) * 3;
                PopupList_UpdateScrollBar(pP);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (pP) {
                int y = (short)HIWORD(lParam);
                LONG lRow = (y - 2) / pP->lRowH;
                LONG lIdx = pP->lScrollIdx + lRow;
                if (lIdx >= 0 && (DWORD)lIdx < pP->dwCount && lIdx != pP->lHover) {
                    pP->lHover = lIdx;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (pP) {
                int y = (short)HIWORD(lParam);
                LONG lRow = (y - 2) / pP->lRowH;
                LONG lIdx = pP->lScrollIdx + lRow;
                HWND hwndAnchor = pP->hwndAnchor;
                if (lIdx >= 0 && (DWORD)lIdx < pP->dwCount && pP->pfnOnPick) {
                    PFN_POPUP_LIST_PICK pfn = pP->pfnOnPick;
                    void* pCtx = pP->pPickCtx;
                    pP->pfnOnPick = NULL; /* Prevent WA_INACTIVE double-notify */
                    pfn(pCtx, lIdx);
                    DestroyWindow(hwnd);
                    if (hwndAnchor) SetFocus(hwndAnchor);
                }
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (pP) {
                if (wParam == VK_ESCAPE) {
                    HWND hwndAnchor = pP->hwndAnchor;
                    PFN_POPUP_LIST_PICK pfn = pP->pfnOnPick;
                    void* pCtx = pP->pPickCtx;
                    pP->pfnOnPick = NULL;
                    if (pfn) pfn(pCtx, -1);
                    DestroyWindow(hwnd);
                    if (hwndAnchor) SetFocus(hwndAnchor);
                    return 0;
                }
                if (wParam == VK_UP && pP->lHover > 0) {
                    pP->lHover--;
                    if (pP->lHover < pP->lScrollIdx) {
                        pP->lScrollIdx = pP->lHover;
                        PopupList_UpdateScrollBar(pP);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (wParam == VK_DOWN && (DWORD)(pP->lHover + 1) < pP->dwCount) {
                    pP->lHover++;
                    if (pP->lHover >= pP->lScrollIdx + pP->lVisibleRows) {
                        pP->lScrollIdx = pP->lHover - pP->lVisibleRows + 1;
                        PopupList_UpdateScrollBar(pP);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (wParam == VK_RETURN && pP->lHover >= 0 && (DWORD)pP->lHover < pP->dwCount) {
                    HWND hwndAnchor = pP->hwndAnchor;
                    PFN_POPUP_LIST_PICK pfn = pP->pfnOnPick;
                    void* pCtx = pP->pPickCtx;
                    pP->pfnOnPick = NULL;
                    if (pfn) pfn(pCtx, pP->lHover);
                    DestroyWindow(hwnd);
                    if (hwndAnchor) SetFocus(hwndAnchor);
                    return 0;
                }
            }
            break;
        }
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pP && pP->pTheme) {
                const APP_THEME* pT = pP->pTheme;
                UINT d = pP->uDpi;
                RECT rc;
                int nW, nH;
                HDC hdcMem;
                HBITMAP hbm, hOldBmp;
                HFONT hOldF;
                LONG r;

                GetClientRect(hwnd, &rc);
                nW = rc.right - rc.left;
                nH = rc.bottom - rc.top;
                hdcMem = CreateCompatibleDC(hdc);
                hbm = CreateCompatibleBitmap(hdc, nW, nH);
                hOldBmp = (HBITMAP)SelectObject(hdcMem, hbm);

                {
                    HBRUSH hBr = CreateSolidBrush(pT->crBgSurface);
                    HPEN hPen = CreatePen(PS_SOLID, 1, pT->crBorderStrong);
                    HBRUSH hOB = (HBRUSH)SelectObject(hdcMem, hBr);
                    HPEN hOP = (HPEN)SelectObject(hdcMem, hPen);
                    Rectangle(hdcMem, 0, 0, nW, nH);
                    SelectObject(hdcMem, hOB);
                    SelectObject(hdcMem, hOP);
                    DeleteObject(hBr);
                    DeleteObject(hPen);
                }

                SetBkMode(hdcMem, TRANSPARENT);
                hOldF = (HFONT)SelectObject(hdcMem, pP->hFont);

                for (r = 0; r < pP->lVisibleRows; ++r) {
                    LONG idx = pP->lScrollIdx + r;
                    RECT rcRow = {2, 2 + r * pP->lRowH, nW - 2, 2 + (r + 1) * pP->lRowH};
                    const POPUP_LIST_ITEM* pIt;

                    if (idx < 0 || (DWORD)idx >= pP->dwCount) break;
                    pIt = &pP->pItems[idx];

                    if (idx == pP->lSelected) {
                        HBRUSH hBrS = CreateSolidBrush(pT->crAccent);
                        FillRect(hdcMem, &rcRow, hBrS);
                        DeleteObject(hBrS);
                        SetTextColor(hdcMem, pT->crAccentText);
                    } else if (idx == pP->lHover) {
                        HBRUSH hBrH = CreateSolidBrush(pT->crBgHover);
                        FillRect(hdcMem, &rcRow, hBrH);
                        DeleteObject(hBrH);
                        SetTextColor(hdcMem, pT->crTextPrimary);
                    } else {
                        SetTextColor(hdcMem, pT->crTextPrimary);
                    }

                    if (pIt->bHasSwatch) {
                        LONG lSw = TcScale(d, 12);
                        LONG lSy = rcRow.top + (pP->lRowH - lSw) / 2;
                        RECT rcSw = {rcRow.left + TcScale(d, 8), lSy, rcRow.left + TcScale(d, 8) + lSw, lSy + lSw};
                        HBRUSH hBrSw = CreateSolidBrush(pIt->crSwatch);
                        FillRect(hdcMem, &rcSw, hBrSw);
                        DeleteObject(hBrSw);
                        rcRow.left += TcScale(d, 26);
                    } else {
                        rcRow.left += TcScale(d, 8);
                    }

                    rcRow.right -= TcScale(d, 6);
                    DrawTextW(hdcMem, pIt->szText, -1, &rcRow, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                }

                SelectObject(hdcMem, hOldF);
                BitBlt(hdc, 0, 0, nW, nH, hdcMem, 0, 0, SRCCOPY);
                SelectObject(hdcMem, hOldBmp);
                DeleteObject(hbm);
                DeleteDC(hdcMem);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (pP) {
                if (pP->pItems) free(pP->pItems);
                free(pP);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

/* ========================================================================= */
/* 2B. CUSTOM THEMED YEAR PICKER (DROPDOWN + WHEEL + 4-DIGIT ENTRY)          */
/* ========================================================================= */

#define YEAR_PICKER_LIST_MIN 1900
#define YEAR_PICKER_LIST_MAX 2100

typedef struct _YEAR_PICKER_STATE {
    HWND             hwnd;
    HWND             hwndParent;
    HINSTANCE        hInst;
    const APP_THEME* pTheme;
    HFONT            hFont;
    UINT             uDpi;
    int              nId;
    WORD             wYear;
    BOOL             bPopupOpen;
    WCHAR            szDigitBuf[5];
    DWORD            dwDigitLen;
} YEAR_PICKER_STATE;

static void OnYearItemPicked(void* pCtx, LONG lIndex)
{
    YEAR_PICKER_STATE* pS = (YEAR_PICKER_STATE*)pCtx;
    if (!pS) return;
    pS->bPopupOpen = FALSE;
    if (lIndex >= 0 && lIndex <= (YEAR_PICKER_LIST_MAX - YEAR_PICKER_LIST_MIN)) {
        pS->wYear = (WORD)(YEAR_PICKER_LIST_MIN + lIndex);
        pS->dwDigitLen = 0;
        SendMessageW(pS->hwndParent, WM_COMMAND, MAKEWPARAM((WORD)pS->nId, TCN_YEAR_CHANGED), (LPARAM)pS->hwnd);
    }
    InvalidateRect(pS->hwnd, NULL, FALSE);
}

static void OpenYearPopup(YEAR_PICKER_STATE* pS)
{
    POPUP_LIST_STATE* pP;
    RECT rcCtrl;
    LONG lRowH = TcScale(pS->uDpi, 26);
    LONG lVisRows = 10;
    DWORD dwCount = (DWORD)(YEAR_PICKER_LIST_MAX - YEAR_PICKER_LIST_MIN + 1);
    LONG lSelIdx;
    int nW, nH, x, y;
    HWND hwndPop;
    DWORD i;

    if (pS->bPopupOpen) return;
    pP = (POPUP_LIST_STATE*)calloc(1, sizeof(POPUP_LIST_STATE));
    if (!pP) return;

    pP->pItems = (POPUP_LIST_ITEM*)calloc(dwCount, sizeof(POPUP_LIST_ITEM));
    if (!pP->pItems) {
        free(pP);
        return;
    }

    for (i = 0; i < dwCount; ++i) {
        _snwprintf(pP->pItems[i].szText, 160, L"%u", (UINT)(YEAR_PICKER_LIST_MIN + i));
    }

    if (pS->wYear >= YEAR_PICKER_LIST_MIN && pS->wYear <= YEAR_PICKER_LIST_MAX) {
        lSelIdx = (LONG)(pS->wYear - YEAR_PICKER_LIST_MIN);
    } else if (pS->wYear < YEAR_PICKER_LIST_MIN) {
        lSelIdx = 0;
    } else {
        lSelIdx = (LONG)(dwCount - 1);
    }

    pP->hwndAnchor = pS->hwnd;
    pP->pTheme = pS->pTheme;
    pP->hFont = pS->hFont;
    pP->uDpi = pS->uDpi;
    pP->dwCount = dwCount;
    pP->lSelected = (pS->wYear >= YEAR_PICKER_LIST_MIN && pS->wYear <= YEAR_PICKER_LIST_MAX) ? lSelIdx : -1;
    pP->lHover = lSelIdx;
    pP->lRowH = lRowH;
    pP->lVisibleRows = lVisRows;
    pP->lScrollIdx = (lSelIdx > lVisRows / 2) ? (lSelIdx - lVisRows / 2) : 0;
    pP->pfnOnPick = OnYearItemPicked;
    pP->pPickCtx = pS;

    GetWindowRect(pS->hwnd, &rcCtrl);
    nW = rcCtrl.right - rcCtrl.left;
    if (nW < TcScale(pS->uDpi, 90)) nW = TcScale(pS->uDpi, 90);
    nH = lVisRows * lRowH + 4;

    ClampPopupToMonitor(pS->hwnd, nW, nH, &x, &y);
    pS->bPopupOpen = TRUE;
    InvalidateRect(pS->hwnd, NULL, FALSE);

    hwndPop = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        AGENDA_POPUP_LIST_CLASS,
        L"",
        WS_POPUP | WS_VSCROLL,
        x, y, nW, nH,
        pS->hwndParent, NULL, pS->hInst, pP
    );
    if (hwndPop) {
        ShowWindow(hwndPop, SW_SHOW);
        SetFocus(hwndPop);
    } else {
        pS->bPopupOpen = FALSE;
        free(pP->pItems);
        free(pP);
    }
}

static void StepYearValue(YEAR_PICKER_STATE* pS, LONG lDelta)
{
    LONG lNew = (LONG)pS->wYear + lDelta;
    if (lNew < 1601) lNew = 1601;
    if (lNew > 9999) lNew = 9999;
    pS->wYear = (WORD)lNew;
    pS->dwDigitLen = 0;
    InvalidateRect(pS->hwnd, NULL, FALSE);
    SendMessageW(pS->hwndParent, WM_COMMAND, MAKEWPARAM((WORD)pS->nId, TCN_YEAR_CHANGED), (LPARAM)pS->hwnd);
}

static LRESULT CALLBACK YearPickerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    YEAR_PICKER_STATE* pS = (YEAR_PICKER_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCs = (CREATESTRUCTW*)lParam;
            pS = (YEAR_PICKER_STATE*)pCs->lpCreateParams;
            pS->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pS);
            return 0;
        }
        case WM_ENABLE:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            if (pS) pS->dwDigitLen = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            if (pS && IsWindowEnabled(hwnd)) {
                OpenYearPopup(pS);
            }
            return 0;

        case WM_MOUSEWHEEL:
            if (pS && IsWindowEnabled(hwnd)) {
                short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                StepYearValue(pS, (zDelta > 0) ? -1 : 1);
                return 0;
            }
            break;

        case WM_KEYDOWN:
            if (pS && IsWindowEnabled(hwnd)) {
                if (wParam == VK_SPACE || wParam == VK_RETURN) {
                    OpenYearPopup(pS);
                    return 0;
                }
                if (wParam == VK_UP) {
                    StepYearValue(pS, -1);
                    return 0;
                }
                if (wParam == VK_DOWN) {
                    StepYearValue(pS, 1);
                    return 0;
                }
                if (wParam == VK_BACK && pS->dwDigitLen > 0) {
                    pS->dwDigitLen--;
                    pS->szDigitBuf[pS->dwDigitLen] = L'\0';
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            break;

        case WM_CHAR:
            if (pS && IsWindowEnabled(hwnd) && wParam >= L'0' && wParam <= L'9') {
                if (pS->dwDigitLen >= 4) pS->dwDigitLen = 0;
                pS->szDigitBuf[pS->dwDigitLen++] = (WCHAR)wParam;
                pS->szDigitBuf[pS->dwDigitLen] = L'\0';

                if (pS->dwDigitLen == 4) {
                    LONG lParsed = _wtol(pS->szDigitBuf);
                    pS->dwDigitLen = 0;
                    if (lParsed >= 1601 && lParsed <= 9999) {
                        pS->wYear = (WORD)lParsed;
                        SendMessageW(pS->hwndParent, WM_COMMAND,
                                     MAKEWPARAM((WORD)pS->nId, TCN_YEAR_CHANGED), (LPARAM)hwnd);
                    }
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;

        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pS && pS->pTheme) {
                const APP_THEME* pT = pS->pTheme;
                UINT d = pS->uDpi;
                RECT rc;
                BOOL bEnabled = IsWindowEnabled(hwnd);
                BOOL bFocused = (GetFocus() == hwnd) || pS->bPopupOpen;
                COLORREF crBg = bEnabled ? pT->crBgSurface : pT->crBgGridAlt;
                COLORREF crBorder = bFocused ? pT->crAccent : (bEnabled ? pT->crBorderStrong : pT->crBorderSubtle);
                COLORREF crText = bEnabled ? pT->crTextPrimary : pT->crTextMuted;
                WCHAR szDisp[16];
                RECT rcText;
                HFONT hOldF;

                GetClientRect(hwnd, &rc);
                {
                    HBRUSH hBrWin = CreateSolidBrush(pT->crBgWindow);
                    FillRect(hdc, &rc, hBrWin);
                    DeleteObject(hBrWin);
                }

                DrawThemedBoxFrame(hdc, &rc, crBg, crBorder, TcScale(d, 5), bFocused ? 2 : 1);

                if (pS->dwDigitLen > 0) {
                    _snwprintf(szDisp, 16, L"%s_", pS->szDigitBuf);
                } else {
                    _snwprintf(szDisp, 16, L"%04u", (UINT)pS->wYear);
                }
                szDisp[15] = L'\0';

                rcText.left = rc.left + TcScale(d, 10);
                rcText.top = rc.top;
                rcText.right = rc.right - TcScale(d, 20);
                rcText.bottom = rc.bottom;
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, crText);
                hOldF = (HFONT)SelectObject(hdc, pS->hFont);
                DrawTextW(hdc, szDisp, -1, &rcText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                SelectObject(hdc, hOldF);

                DrawChevronDown(hdc, rc.right - TcScale(d, 11), (rc.top + rc.bottom) / 2,
                                TcScale(d, 4), bEnabled ? pT->crTextSecondary : pT->crTextMuted);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (pS) {
                free(pS);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

/* ========================================================================= */
/* 3. CUSTOM THEMED TIME PICKER (15-MIN SLOTS + WHEEL + DIGIT ENTRY)         */
/* ========================================================================= */

typedef struct _TIME_PICKER_STATE {
    HWND             hwnd;
    HWND             hwndParent;
    HINSTANCE        hInst;
    const APP_THEME* pTheme;
    HFONT            hFont;
    UINT             uDpi;
    int              nId;
    WORD             wHour;
    WORD             wMinute;
    BOOL             bPopupOpen;
    WCHAR            szDigitBuf[5];
    DWORD            dwDigitLen;
} TIME_PICKER_STATE;

static void OnTimeSlotPicked(void* pCtx, LONG lIndex)
{
    TIME_PICKER_STATE* pS = (TIME_PICKER_STATE*)pCtx;
    if (!pS) return;
    pS->bPopupOpen = FALSE;
    if (lIndex >= 0 && lIndex < 96) {
        pS->wHour = (WORD)(lIndex / 4);
        pS->wMinute = (WORD)((lIndex % 4) * 15);
        pS->dwDigitLen = 0;
        SendMessageW(pS->hwndParent, WM_COMMAND, MAKEWPARAM((WORD)pS->nId, TCN_TIME_CHANGED), (LPARAM)pS->hwnd);
    }
    InvalidateRect(pS->hwnd, NULL, FALSE);
}

static void OpenTimePopup(TIME_PICKER_STATE* pS)
{
    POPUP_LIST_STATE* pP;
    RECT rcCtrl;
    LONG lRowH = TcScale(pS->uDpi, 26);
    LONG lVisRows = 9;
    int nW, nH, x, y;
    HWND hwndPop;
    DWORD i;
    LONG lCurSlot;

    if (pS->bPopupOpen) return;
    pP = (POPUP_LIST_STATE*)calloc(1, sizeof(POPUP_LIST_STATE));
    if (!pP) return;

    pP->pItems = (POPUP_LIST_ITEM*)calloc(96, sizeof(POPUP_LIST_ITEM));
    if (!pP->pItems) {
        free(pP);
        return;
    }

    for (i = 0; i < 96; ++i) {
        _snwprintf(pP->pItems[i].szText, 160, L"%02u:%02u", (UINT)(i / 4), (UINT)((i % 4) * 15));
    }

    lCurSlot = (LONG)(pS->wHour * 4 + (pS->wMinute / 15));
    pP->hwndAnchor = pS->hwnd;
    pP->pTheme = pS->pTheme;
    pP->hFont = pS->hFont;
    pP->uDpi = pS->uDpi;
    pP->dwCount = 96;
    pP->lSelected = lCurSlot;
    pP->lHover = lCurSlot;
    pP->lRowH = lRowH;
    pP->lVisibleRows = lVisRows;
    pP->lScrollIdx = lCurSlot - 3;
    pP->pfnOnPick = OnTimeSlotPicked;
    pP->pPickCtx = pS;

    GetWindowRect(pS->hwnd, &rcCtrl);
    nW = rcCtrl.right - rcCtrl.left;
    if (nW < TcScale(pS->uDpi, 118)) nW = TcScale(pS->uDpi, 118);
    nH = lVisRows * lRowH + 4;

    ClampPopupToMonitor(pS->hwnd, nW, nH, &x, &y);
    pS->bPopupOpen = TRUE;
    InvalidateRect(pS->hwnd, NULL, FALSE);

    hwndPop = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        AGENDA_POPUP_LIST_CLASS,
        L"",
        WS_POPUP | WS_VSCROLL,
        x, y, nW, nH,
        pS->hwndParent, NULL, pS->hInst, pP
    );
    if (hwndPop) {
        ShowWindow(hwndPop, SW_SHOW);
        SetFocus(hwndPop);
    } else {
        pS->bPopupOpen = FALSE;
        free(pP->pItems);
        free(pP);
    }
}

static void StepTimeMinutes(TIME_PICKER_STATE* pS, LONG lDeltaMinutes)
{
    LONG lTotal = (LONG)pS->wHour * 60 + (LONG)pS->wMinute + lDeltaMinutes;
    while (lTotal < 0) lTotal += 1440;
    lTotal %= 1440;
    pS->wHour = (WORD)(lTotal / 60);
    pS->wMinute = (WORD)(lTotal % 60);
    pS->dwDigitLen = 0;
    InvalidateRect(pS->hwnd, NULL, FALSE);
    SendMessageW(pS->hwndParent, WM_COMMAND, MAKEWPARAM((WORD)pS->nId, TCN_TIME_CHANGED), (LPARAM)pS->hwnd);
}

static LRESULT CALLBACK TimePickerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    TIME_PICKER_STATE* pS = (TIME_PICKER_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCs = (CREATESTRUCTW*)lParam;
            pS = (TIME_PICKER_STATE*)pCs->lpCreateParams;
            pS->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pS);
            return 0;
        }
        case WM_ENABLE:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_SETFOCUS:
            if (pS) pS->dwDigitLen = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_KILLFOCUS:
            if (pS) pS->dwDigitLen = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            if (pS && IsWindowEnabled(hwnd)) {
                OpenTimePopup(pS);
            }
            return 0;

        case WM_MOUSEWHEEL:
            if (pS && IsWindowEnabled(hwnd)) {
                short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                StepTimeMinutes(pS, (zDelta > 0) ? -15 : 15);
                return 0;
            }
            break;

        case WM_KEYDOWN:
            if (pS && IsWindowEnabled(hwnd)) {
                if (wParam == VK_SPACE || wParam == VK_RETURN) {
                    OpenTimePopup(pS);
                    return 0;
                }
                if (wParam == VK_UP) {
                    StepTimeMinutes(pS, -15);
                    return 0;
                }
                if (wParam == VK_DOWN) {
                    StepTimeMinutes(pS, 15);
                    return 0;
                }
            }
            break;

        case WM_CHAR:
            if (pS && IsWindowEnabled(hwnd) && wParam >= L'0' && wParam <= L'9') {
                WCHAR ch = (WCHAR)wParam;
                if (pS->dwDigitLen >= 4) pS->dwDigitLen = 0;
                pS->szDigitBuf[pS->dwDigitLen++] = ch;
                pS->szDigitBuf[pS->dwDigitLen] = L'\0';

                if (pS->dwDigitLen == 2) {
                    UINT hh = (UINT)((pS->szDigitBuf[0] - L'0') * 10 + (pS->szDigitBuf[1] - L'0'));
                    if (hh < 24) {
                        pS->wHour = (WORD)hh;
                        pS->wMinute = 0;
                    } else {
                        pS->dwDigitLen = 0;
                    }
                } else if (pS->dwDigitLen == 4) {
                    UINT hh = (UINT)((pS->szDigitBuf[0] - L'0') * 10 + (pS->szDigitBuf[1] - L'0'));
                    UINT mm = (UINT)((pS->szDigitBuf[2] - L'0') * 10 + (pS->szDigitBuf[3] - L'0'));
                    if (hh < 24 && mm < 60) {
                        pS->wHour = (WORD)hh;
                        pS->wMinute = (WORD)mm;
                    }
                    pS->dwDigitLen = 0;
                }
                InvalidateRect(hwnd, NULL, FALSE);
                SendMessageW(pS->hwndParent, WM_COMMAND, MAKEWPARAM((WORD)pS->nId, TCN_TIME_CHANGED), (LPARAM)hwnd);
                return 0;
            }
            break;

        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pS && pS->pTheme) {
                const APP_THEME* pT = pS->pTheme;
                UINT d = pS->uDpi;
                RECT rc;
                BOOL bEnabled = IsWindowEnabled(hwnd);
                BOOL bFocused = (GetFocus() == hwnd) || pS->bPopupOpen;
                COLORREF crBg = bEnabled ? pT->crBgSurface : pT->crBgGridAlt;
                COLORREF crBorder = bFocused ? pT->crAccent : (bEnabled ? pT->crBorderStrong : pT->crBorderSubtle);
                COLORREF crText = bEnabled ? pT->crTextPrimary : pT->crTextMuted;
                WCHAR szTime[16];
                RECT rcText;
                HFONT hOldF;

                GetClientRect(hwnd, &rc);
                {
                    HBRUSH hBrWin = CreateSolidBrush(pT->crBgWindow);
                    FillRect(hdc, &rc, hBrWin);
                    DeleteObject(hBrWin);
                }

                DrawThemedBoxFrame(hdc, &rc, crBg, crBorder, TcScale(d, 5), bFocused ? 2 : 1);

                /* Small clock circle on left */
                {
                    LONG lIx = rc.left + TcScale(d, 9);
                    LONG lIw = TcScale(d, 13);
                    LONG lIy = rc.top + (rc.bottom - rc.top - lIw) / 2;
                    LONG lCx = lIx + lIw / 2;
                    LONG lCy = lIy + lIw / 2;
                    HPEN hPenClk = CreatePen(PS_SOLID, 1, crText);
                    HBRUSH hOB = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    HPEN hOP = (HPEN)SelectObject(hdc, hPenClk);
                    Ellipse(hdc, lIx, lIy, lIx + lIw, lIy + lIw);
                    MoveToEx(hdc, lCx, lCy - TcScale(d, 3), NULL);
                    LineTo(hdc, lCx, lCy);
                    LineTo(hdc, lCx + TcScale(d, 3), lCy);
                    SelectObject(hdc, hOB);
                    SelectObject(hdc, hOP);
                    DeleteObject(hPenClk);
                }

                _snwprintf(szTime, 16, L"%02u:%02u", (UINT)pS->wHour, (UINT)pS->wMinute);
                szTime[15] = L'\0';

                rcText.left = rc.left + TcScale(d, 28);
                rcText.top = rc.top;
                rcText.right = rc.right - TcScale(d, 22);
                rcText.bottom = rc.bottom;
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, crText);
                hOldF = (HFONT)SelectObject(hdc, pS->hFont);
                DrawTextW(hdc, szTime, -1, &rcText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                SelectObject(hdc, hOldF);

                DrawChevronDown(hdc, rc.right - TcScale(d, 12), (rc.top + rc.bottom) / 2,
                                TcScale(d, 4), bEnabled ? pT->crTextSecondary : pT->crTextMuted);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (pS) {
                free(pS);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

/* ========================================================================= */
/* 4. CUSTOM THEMED COMBOBOX (100% DARK/LIGHT CONSISTENT)                    */
/* ========================================================================= */

typedef struct _THEMED_COMBO_STATE {
    HWND             hwnd;
    HWND             hwndParent;
    HINSTANCE        hInst;
    const APP_THEME* pTheme;
    HFONT            hFont;
    UINT             uDpi;
    int              nId;
    POPUP_LIST_ITEM* pItems;
    DWORD            dwCount;
    DWORD            dwCapacity;
    LONG             lSelected;
    BOOL             bPopupOpen;
} THEMED_COMBO_STATE;

static void OnComboItemPicked(void* pCtx, LONG lIndex)
{
    THEMED_COMBO_STATE* pS = (THEMED_COMBO_STATE*)pCtx;
    if (!pS) return;
    pS->bPopupOpen = FALSE;
    if (lIndex >= 0 && (DWORD)lIndex < pS->dwCount) {
        pS->lSelected = lIndex;
        SendMessageW(pS->hwndParent, WM_COMMAND, MAKEWPARAM((WORD)pS->nId, CBN_SELCHANGE), (LPARAM)pS->hwnd);
    }
    InvalidateRect(pS->hwnd, NULL, FALSE);
}

static void OpenComboPopup(THEMED_COMBO_STATE* pS)
{
    POPUP_LIST_STATE* pP;
    RECT rcCtrl;
    LONG lRowH = TcScale(pS->uDpi, 26);
    LONG lVisRows;
    int nW, nH, x, y;
    HWND hwndPop;

    if (pS->bPopupOpen || pS->dwCount == 0) return;
    pP = (POPUP_LIST_STATE*)calloc(1, sizeof(POPUP_LIST_STATE));
    if (!pP) return;

    pP->pItems = (POPUP_LIST_ITEM*)calloc(pS->dwCount, sizeof(POPUP_LIST_ITEM));
    if (!pP->pItems) {
        free(pP);
        return;
    }
    memcpy(pP->pItems, pS->pItems, pS->dwCount * sizeof(POPUP_LIST_ITEM));

    lVisRows = (pS->dwCount > 10) ? 10 : (LONG)pS->dwCount;
    pP->hwndAnchor = pS->hwnd;
    pP->pTheme = pS->pTheme;
    pP->hFont = pS->hFont;
    pP->uDpi = pS->uDpi;
    pP->dwCount = pS->dwCount;
    pP->lSelected = pS->lSelected;
    pP->lHover = (pS->lSelected >= 0) ? pS->lSelected : 0;
    pP->lRowH = lRowH;
    pP->lVisibleRows = lVisRows;
    pP->lScrollIdx = (pS->lSelected > lVisRows / 2 && (LONG)pS->dwCount > lVisRows)
                     ? (pS->lSelected - lVisRows / 2) : 0;
    pP->pfnOnPick = OnComboItemPicked;
    pP->pPickCtx = pS;

    GetWindowRect(pS->hwnd, &rcCtrl);
    nW = rcCtrl.right - rcCtrl.left;
    nH = lVisRows * lRowH + 4;

    ClampPopupToMonitor(pS->hwnd, nW, nH, &x, &y);
    pS->bPopupOpen = TRUE;
    InvalidateRect(pS->hwnd, NULL, FALSE);

    hwndPop = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        AGENDA_POPUP_LIST_CLASS,
        L"",
        WS_POPUP | ((LONG)pS->dwCount > lVisRows ? WS_VSCROLL : 0),
        x, y, nW, nH,
        pS->hwndParent, NULL, pS->hInst, pP
    );
    if (hwndPop) {
        ShowWindow(hwndPop, SW_SHOW);
        SetFocus(hwndPop);
    } else {
        pS->bPopupOpen = FALSE;
        free(pP->pItems);
        free(pP);
    }
}

static LRESULT CALLBACK ThemedComboProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    THEMED_COMBO_STATE* pS = (THEMED_COMBO_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCs = (CREATESTRUCTW*)lParam;
            pS = (THEMED_COMBO_STATE*)pCs->lpCreateParams;
            pS->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pS);
            return 0;
        }
        case WM_ENABLE:
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            if (pS && IsWindowEnabled(hwnd)) {
                OpenComboPopup(pS);
            }
            return 0;

        case WM_KEYDOWN:
            if (pS && IsWindowEnabled(hwnd)) {
                if (wParam == VK_SPACE || wParam == VK_RETURN) {
                    OpenComboPopup(pS);
                    return 0;
                }
                if (wParam == VK_UP && pS->lSelected > 0) {
                    pS->lSelected--;
                    InvalidateRect(hwnd, NULL, FALSE);
                    SendMessageW(pS->hwndParent, WM_COMMAND, MAKEWPARAM((WORD)pS->nId, CBN_SELCHANGE), (LPARAM)hwnd);
                    return 0;
                }
                if (wParam == VK_DOWN && (DWORD)(pS->lSelected + 1) < pS->dwCount) {
                    pS->lSelected++;
                    InvalidateRect(hwnd, NULL, FALSE);
                    SendMessageW(pS->hwndParent, WM_COMMAND, MAKEWPARAM((WORD)pS->nId, CBN_SELCHANGE), (LPARAM)hwnd);
                    return 0;
                }
            }
            break;

        case WM_GETDLGCODE:
            return DLGC_WANTARROWS;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pS && pS->pTheme) {
                const APP_THEME* pT = pS->pTheme;
                UINT d = pS->uDpi;
                RECT rc;
                BOOL bEnabled = IsWindowEnabled(hwnd);
                BOOL bFocused = (GetFocus() == hwnd) || pS->bPopupOpen;
                COLORREF crBg = bEnabled ? pT->crBgSurface : pT->crBgGridAlt;
                COLORREF crBorder = bFocused ? pT->crAccent : (bEnabled ? pT->crBorderStrong : pT->crBorderSubtle);
                COLORREF crText = bEnabled ? pT->crTextPrimary : pT->crTextMuted;
                RECT rcText;
                HFONT hOldF;

                GetClientRect(hwnd, &rc);
                {
                    HBRUSH hBrWin = CreateSolidBrush(pT->crBgWindow);
                    FillRect(hdc, &rc, hBrWin);
                    DeleteObject(hBrWin);
                }

                DrawThemedBoxFrame(hdc, &rc, crBg, crBorder, TcScale(d, 5), bFocused ? 2 : 1);

                rcText.left = rc.left + TcScale(d, 8);
                rcText.top = rc.top;
                rcText.right = rc.right - TcScale(d, 22);
                rcText.bottom = rc.bottom;

                if (pS->lSelected >= 0 && (DWORD)pS->lSelected < pS->dwCount) {
                    const POPUP_LIST_ITEM* pIt = &pS->pItems[pS->lSelected];
                    if (pIt->bHasSwatch) {
                        LONG lSw = TcScale(d, 12);
                        LONG lSy = rc.top + (rc.bottom - rc.top - lSw) / 2;
                        RECT rcSw = {rcText.left, lSy, rcText.left + lSw, lSy + lSw};
                        HBRUSH hBrSw = CreateSolidBrush(pIt->crSwatch);
                        FillRect(hdc, &rcSw, hBrSw);
                        DeleteObject(hBrSw);
                        rcText.left += TcScale(d, 18);
                    }
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, crText);
                    hOldF = (HFONT)SelectObject(hdc, pS->hFont);
                    DrawTextW(hdc, pIt->szText, -1, &rcText, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                    SelectObject(hdc, hOldF);
                }

                DrawChevronDown(hdc, rc.right - TcScale(d, 12), (rc.top + rc.bottom) / 2,
                                TcScale(d, 4), bEnabled ? pT->crTextSecondary : pT->crTextMuted);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (pS) {
                if (pS->pItems) free(pS->pItems);
                free(pS);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

/* ========================================================================= */
/* 5. 100% THEMED MODAL MESSAGEBOX                                           */
/* ========================================================================= */

#define IDC_MB_YES 9101
#define IDC_MB_NO  9102

typedef struct _THEMED_MB_STATE {
    HWND             hwnd;
    HWND             hwndOwner;
    const APP_THEME* pTheme;
    const WCHAR*     pszMessage;
    BOOL             bYesNo;
    UINT             uDpi;
    HFONT            hFont;
    HFONT            hFontBold;
    HBRUSH           hBrBg;
    INT              nResult;
    BOOL             bClosed;
} THEMED_MB_STATE;

static LRESULT CALLBACK ThemedMsgBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    THEMED_MB_STATE* pS = (THEMED_MB_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCs = (CREATESTRUCTW*)lParam;
            RECT rcClient;
            int nW, nH;
            UINT d;

            pS = (THEMED_MB_STATE*)pCs->lpCreateParams;
            pS->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pS);

            d = pS->uDpi;
            pS->hBrBg = CreateSolidBrush(pS->pTheme->crBgWindow);
            Theme_ApplyToWindow(hwnd, pS->pTheme->bDarkMode);

            GetClientRect(hwnd, &rcClient);
            nW = rcClient.right - rcClient.left;
            nH = rcClient.bottom - rcClient.top;

            {
                HWND hLbl = CreateWindowExW(
                    0, L"STATIC", pS->pszMessage,
                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                    TcScale(d, 20), TcScale(d, 20), nW - TcScale(d, 40), nH - TcScale(d, 80),
                    hwnd, NULL, pCs->hInstance, NULL
                );
                SendMessageW(hLbl, WM_SETFONT, (WPARAM)pS->hFont, FALSE);
            }

            if (pS->bYesNo) {
                HWND hYes = CreateWindowExW(
                    0, L"BUTTON", L"Oui",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                    nW - TcScale(d, 204), nH - TcScale(d, 46), TcScale(d, 88), TcScale(d, 30),
                    hwnd, (HMENU)(INT_PTR)IDC_MB_YES, pCs->hInstance, NULL
                );
                HWND hNo = CreateWindowExW(
                    0, L"BUTTON", L"Non",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                    nW - TcScale(d, 108), nH - TcScale(d, 46), TcScale(d, 88), TcScale(d, 30),
                    hwnd, (HMENU)(INT_PTR)IDC_MB_NO, pCs->hInstance, NULL
                );
                SendMessageW(hYes, WM_SETFONT, (WPARAM)pS->hFontBold, FALSE);
                SendMessageW(hNo, WM_SETFONT, (WPARAM)pS->hFont, FALSE);
                SetFocus(hNo);
            } else {
                HWND hOk = CreateWindowExW(
                    0, L"BUTTON", L"OK",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                    nW - TcScale(d, 112), nH - TcScale(d, 46), TcScale(d, 92), TcScale(d, 30),
                    hwnd, (HMENU)(INT_PTR)IDC_MB_YES, pCs->hInstance, NULL
                );
                SendMessageW(hOk, WM_SETFONT, (WPARAM)pS->hFontBold, FALSE);
                SetFocus(hOk);
            }
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
        case WM_CTLCOLORSTATIC:
            if (pS && pS->pTheme) {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, pS->pTheme->crTextPrimary);
                SetBkColor(hdc, pS->pTheme->crBgWindow);
                return (LRESULT)pS->hBrBg;
            }
            break;
        case WM_DRAWITEM:
            if (pS && pS->pTheme) {
                const DRAWITEMSTRUCT* pDis = (const DRAWITEMSTRUCT*)lParam;
                const APP_THEME* pT = pS->pTheme;
                BOOL bPrimary = (pDis->CtlID == IDC_MB_YES);
                BOOL bPressed = (pDis->itemState & ODS_SELECTED) != 0;
                BOOL bFocused = (pDis->itemState & ODS_FOCUS) != 0;
                COLORREF crBg = bPrimary ? pT->crAccent : (bPressed ? pT->crBgSelected : pT->crBgSurface);
                COLORREF crBd = bPrimary ? pT->crAccent : (bFocused ? pT->crAccent : pT->crBorderStrong);
                COLORREF crTx = bPrimary ? pT->crAccentText : pT->crTextPrimary;
                WCHAR szText[32];
                HFONT hOldF;
                RECT rcItem = pDis->rcItem;
                HBRUSH hBrW = CreateSolidBrush(pT->crBgWindow);
                FillRect(pDis->hDC, &rcItem, hBrW);
                DeleteObject(hBrW);
                DrawThemedBoxFrame(pDis->hDC, &rcItem, crBg, crBd, TcScale(pS->uDpi, 6), bFocused ? 2 : 1);
                GetWindowTextW(pDis->hwndItem, szText, 32);
                SetBkMode(pDis->hDC, TRANSPARENT);
                SetTextColor(pDis->hDC, crTx);
                hOldF = (HFONT)SelectObject(pDis->hDC, bPrimary ? pS->hFontBold : pS->hFont);
                DrawTextW(pDis->hDC, szText, -1, &rcItem, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                SelectObject(pDis->hDC, hOldF);
                return TRUE;
            }
            break;
        case WM_COMMAND:
            if (pS) {
                WORD wId = LOWORD(wParam);
                if (wId == IDC_MB_YES) {
                    pS->nResult = pS->bYesNo ? IDYES : IDOK;
                    pS->bClosed = TRUE;
                    EnableWindow(pS->hwndOwner, TRUE);
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (wId == IDC_MB_NO) {
                    pS->nResult = IDNO;
                    pS->bClosed = TRUE;
                    EnableWindow(pS->hwndOwner, TRUE);
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            break;
        case WM_CLOSE:
            if (pS) {
                pS->nResult = pS->bYesNo ? IDNO : IDOK;
                pS->bClosed = TRUE;
                EnableWindow(pS->hwndOwner, TRUE);
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (pS) {
                if (pS->hBrBg) DeleteObject(pS->hBrBg);
                pS->bClosed = TRUE;
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

/* ========================================================================= */
/* PUBLIC REGISTRATION & CONTROL API                                         */
/* ========================================================================= */

BOOL ThemedCtrls_Register(HINSTANCE hInst)
{
    WNDCLASSEXW wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);

    wc.lpfnWndProc = DatePickerProc;
    wc.lpszClassName = AGENDA_THEMED_DATE_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    wc.lpfnWndProc = PopupCalProc;
    wc.lpszClassName = AGENDA_POPUP_CAL_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    wc.lpfnWndProc = YearPickerProc;
    wc.lpszClassName = AGENDA_THEMED_YEAR_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    wc.lpfnWndProc = TimePickerProc;
    wc.lpszClassName = AGENDA_THEMED_TIME_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    wc.lpfnWndProc = ThemedComboProc;
    wc.lpszClassName = AGENDA_THEMED_COMBO_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    wc.lpfnWndProc = PopupListProc;
    wc.lpszClassName = AGENDA_POPUP_LIST_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    wc.lpfnWndProc = ThemedMsgBoxProc;
    wc.lpszClassName = AGENDA_THEMED_MSGBOX_CLASS;
    return (RegisterClassExW(&wc) != 0);
}

HWND ThemedDatePicker_Create(
    HWND hwndParent, HINSTANCE hInst, const APP_THEME* pTheme,
    HFONT hFont, UINT uDpi, int x, int y, int w, int h, int nId,
    const SYSTEMTIME* pstInitial
)
{
    DATE_PICKER_STATE* pS = (DATE_PICKER_STATE*)calloc(1, sizeof(DATE_PICKER_STATE));
    if (!pS) return NULL;
    pS->hwndParent = hwndParent;
    pS->hInst = hInst;
    pS->pTheme = pTheme;
    pS->hFont = hFont;
    pS->uDpi = uDpi;
    pS->nId = nId;
    if (pstInitial) {
        pS->stDate = *pstInitial;
    } else {
        GetLocalTime(&pS->stDate);
    }
    Date_NormalizeTime(&pS->stDate);

    return CreateWindowExW(
        0, AGENDA_THEMED_DATE_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, w, h, hwndParent, (HMENU)(INT_PTR)nId, hInst, pS
    );
}

void ThemedDatePicker_GetDate(HWND hwnd, SYSTEMTIME* pstOut)
{
    DATE_PICKER_STATE* pS = (DATE_PICKER_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (pS && pstOut) {
        *pstOut = pS->stDate;
    }
}

void ThemedDatePicker_SetDate(HWND hwnd, const SYSTEMTIME* pst)
{
    DATE_PICKER_STATE* pS = (DATE_PICKER_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (pS && pst) {
        pS->stDate = *pst;
        Date_NormalizeTime(&pS->stDate);
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

HWND ThemedYearPicker_Create(
    HWND hwndParent, HINSTANCE hInst, const APP_THEME* pTheme,
    HFONT hFont, UINT uDpi, int x, int y, int w, int h, int nId,
    WORD wYear
)
{
    YEAR_PICKER_STATE* pS = (YEAR_PICKER_STATE*)calloc(1, sizeof(YEAR_PICKER_STATE));
    if (!pS) return NULL;
    pS->hwndParent = hwndParent;
    pS->hInst = hInst;
    pS->pTheme = pTheme;
    pS->hFont = hFont;
    pS->uDpi = uDpi;
    pS->nId = nId;
    pS->wYear = (wYear >= 1601 && wYear <= 9999) ? wYear : 2026;

    return CreateWindowExW(
        0, AGENDA_THEMED_YEAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, w, h, hwndParent, (HMENU)(INT_PTR)nId, hInst, pS
    );
}

WORD ThemedYearPicker_GetYear(HWND hwnd)
{
    YEAR_PICKER_STATE* pS = (YEAR_PICKER_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    return pS ? pS->wYear : 2026;
}

void ThemedYearPicker_SetYear(HWND hwnd, WORD wYear)
{
    YEAR_PICKER_STATE* pS = (YEAR_PICKER_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (pS && wYear >= 1601 && wYear <= 9999) {
        pS->wYear = wYear;
        pS->dwDigitLen = 0;
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

HWND ThemedTimePicker_Create(
    HWND hwndParent, HINSTANCE hInst, const APP_THEME* pTheme,
    HFONT hFont, UINT uDpi, int x, int y, int w, int h, int nId,
    WORD wHour, WORD wMinute
)
{
    TIME_PICKER_STATE* pS = (TIME_PICKER_STATE*)calloc(1, sizeof(TIME_PICKER_STATE));
    if (!pS) return NULL;
    pS->hwndParent = hwndParent;
    pS->hInst = hInst;
    pS->pTheme = pTheme;
    pS->hFont = hFont;
    pS->uDpi = uDpi;
    pS->nId = nId;
    pS->wHour = (wHour < 24) ? wHour : 9;
    pS->wMinute = (wMinute < 60) ? wMinute : 0;

    return CreateWindowExW(
        0, AGENDA_THEMED_TIME_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, w, h, hwndParent, (HMENU)(INT_PTR)nId, hInst, pS
    );
}

void ThemedTimePicker_GetTime(HWND hwnd, WORD* pwHour, WORD* pwMinute)
{
    TIME_PICKER_STATE* pS = (TIME_PICKER_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (pS) {
        if (pwHour) *pwHour = pS->wHour;
        if (pwMinute) *pwMinute = pS->wMinute;
    }
}

void ThemedTimePicker_SetTime(HWND hwnd, WORD wHour, WORD wMinute)
{
    TIME_PICKER_STATE* pS = (TIME_PICKER_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (pS) {
        pS->wHour = (wHour < 24) ? wHour : 0;
        pS->wMinute = (wMinute < 60) ? wMinute : 0;
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

HWND ThemedCombo_Create(
    HWND hwndParent, HINSTANCE hInst, const APP_THEME* pTheme,
    HFONT hFont, UINT uDpi, int x, int y, int w, int h, int nId
)
{
    THEMED_COMBO_STATE* pS = (THEMED_COMBO_STATE*)calloc(1, sizeof(THEMED_COMBO_STATE));
    if (!pS) return NULL;
    pS->hwndParent = hwndParent;
    pS->hInst = hInst;
    pS->pTheme = pTheme;
    pS->hFont = hFont;
    pS->uDpi = uDpi;
    pS->nId = nId;
    pS->lSelected = -1;

    return CreateWindowExW(
        0, AGENDA_THEMED_COMBO_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, w, h, hwndParent, (HMENU)(INT_PTR)nId, hInst, pS
    );
}

void ThemedCombo_AddItem(HWND hwnd, const WCHAR* pszText, COLORREF crSwatch, BOOL bHasSwatch)
{
    THEMED_COMBO_STATE* pS = (THEMED_COMBO_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!pS || !pszText) return;

    if (pS->dwCount >= pS->dwCapacity) {
        DWORD dwNewCap = (pS->dwCapacity == 0) ? 8 : pS->dwCapacity * 2;
        POPUP_LIST_ITEM* pNew = (POPUP_LIST_ITEM*)realloc(pS->pItems, dwNewCap * sizeof(POPUP_LIST_ITEM));
        if (!pNew) return;
        pS->pItems = pNew;
        pS->dwCapacity = dwNewCap;
    }

    wcsncpy(pS->pItems[pS->dwCount].szText, pszText, 159);
    pS->pItems[pS->dwCount].szText[159] = L'\0';
    pS->pItems[pS->dwCount].crSwatch = crSwatch;
    pS->pItems[pS->dwCount].bHasSwatch = bHasSwatch;
    pS->dwCount++;
    if (pS->lSelected < 0) {
        pS->lSelected = 0;
    }
}

void ThemedCombo_SetCurSel(HWND hwnd, LONG lIndex)
{
    THEMED_COMBO_STATE* pS = (THEMED_COMBO_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!pS) return;
    if (lIndex >= 0 && (DWORD)lIndex < pS->dwCount) {
        pS->lSelected = lIndex;
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

LONG ThemedCombo_GetCurSel(HWND hwnd)
{
    THEMED_COMBO_STATE* pS = (THEMED_COMBO_STATE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    return pS ? pS->lSelected : -1;
}

INT Themed_MessageBox(
    HWND hwndOwner, HINSTANCE hInst, const APP_THEME* pTheme,
    const WCHAR* pszTitle, const WCHAR* pszMessage, BOOL bYesNo
)
{
    THEMED_MB_STATE state;
    HDC hdc = GetDC(hwndOwner);
    UINT uDpi = hdc ? (UINT)GetDeviceCaps(hdc, LOGPIXELSY) : 96;
    RECT rcOwner, rcWnd;
    DWORD dwStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    int nW, nH, x, y;
    HWND hwndDlg;
    MSG msg;

    if (hdc) ReleaseDC(hwndOwner, hdc);
    if (uDpi == 0) uDpi = 96;

    ZeroMemory(&state, sizeof(state));
    state.hwndOwner = hwndOwner;
    state.pTheme = pTheme;
    state.pszMessage = pszMessage;
    state.bYesNo = bYesNo;
    state.uDpi = uDpi;
    state.nResult = bYesNo ? IDNO : IDOK;

    state.hFont = Theme_CreateAppFont(uDpi, 0, FW_NORMAL, FALSE);
    state.hFontBold = Theme_CreateAppFont(uDpi, 0, FW_BOLD, FALSE);

    rcWnd.left = 0; rcWnd.top = 0;
    rcWnd.right = TcScale(uDpi, 400);
    rcWnd.bottom = TcScale(uDpi, 155);
    AdjustWindowRectEx(&rcWnd, dwStyle, FALSE, 0);
    nW = rcWnd.right - rcWnd.left;
    nH = rcWnd.bottom - rcWnd.top;

    GetWindowRect(hwndOwner, &rcOwner);
    x = rcOwner.left + ((rcOwner.right - rcOwner.left) - nW) / 2;
    y = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - nH) / 2;

    hwndDlg = CreateWindowExW(
        0, AGENDA_THEMED_MSGBOX_CLASS, pszTitle, dwStyle,
        x, y, nW, nH, hwndOwner, NULL, hInst, &state
    );
    if (!hwndDlg) {
        DeleteObject(state.hFont);
        DeleteObject(state.hFontBold);
        return state.nResult;
    }

    EnableWindow(hwndOwner, FALSE);
    ShowWindow(hwndDlg, SW_SHOW);
    UpdateWindow(hwndDlg);

    while (!state.bClosed && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            PostMessageW(hwndDlg, WM_CLOSE, 0, 0);
            continue;
        }
        if (!IsDialogMessageW(hwndDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(hwndOwner, TRUE);
    SetForegroundWindow(hwndOwner);
    DeleteObject(state.hFont);
    DeleteObject(state.hFontBold);
    return state.nResult;
}
