#include "calendar_view.h"
#include "date_utils.h"
#include "recurrence.h"
#include "theme.h"

static LONG ScaleDpi(UINT uDpi, LONG lPx)
{
    return MulDiv(lPx, (int)uDpi, 96);
}

static HFONT CreateUiFont(UINT uDpi, LONG lPt, int nWeight, BOOL bItalic)
{
    return Theme_CreateAppFont(uDpi, lPt - 9, nWeight, bItalic);
}

static void Canvas_ClearHits(CALENDAR_CANVAS* pC)
{
    pC->dwHitCount = 0;
    pC->lHoverIndex = -1;
}

static void Canvas_AddHit(CALENDAR_CANVAS* pC, const RECT* pRc, HIT_KIND kind, DWORD dwEventId, const SYSTEMTIME* pst)
{
    if (pC->dwHitCount >= pC->dwHitCap) {
        DWORD dwNewCap = (pC->dwHitCap == 0) ? 128 : pC->dwHitCap * 2;
        HIT_ITEM* pNew = (HIT_ITEM*)realloc(pC->pHits, dwNewCap * sizeof(HIT_ITEM));
        if (!pNew) {
            return;
        }
        pC->pHits = pNew;
        pC->dwHitCap = dwNewCap;
    }
    pC->pHits[pC->dwHitCount].rc = *pRc;
    pC->pHits[pC->dwHitCount].kind = kind;
    pC->pHits[pC->dwHitCount].dwEventId = dwEventId;
    if (pst) {
        pC->pHits[pC->dwHitCount].stDate = *pst;
    } else {
        ZeroMemory(&pC->pHits[pC->dwHitCount].stDate, sizeof(SYSTEMTIME));
    }
    pC->dwHitCount++;
}

static LONG Canvas_HitTest(const CALENDAR_CANVAS* pC, POINT pt)
{
    LONG i;
    /* Search backwards so top-most elements (events, +N links) take priority over cells */
    for (i = (LONG)pC->dwHitCount - 1; i >= 0; --i) {
        if (PtInRect(&pC->pHits[i].rc, pt)) {
            return i;
        }
    }
    return -1;
}

static void FillSolidRect(HDC hdc, const RECT* pRc, COLORREF cr)
{
    HBRUSH hBr = CreateSolidBrush(cr);
    FillRect(hdc, pRc, hBr);
    DeleteObject(hBr);
}

static void DrawLineColor(HDC hdc, int x1, int y1, int x2, int y2, COLORREF cr, int nWidth)
{
    HPEN hPen = CreatePen(PS_SOLID, nWidth, cr);
    HPEN hOld = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, hOld);
    DeleteObject(hPen);
}

static void DrawRoundPill(HDC hdc, const RECT* pRc, COLORREF crFill, COLORREF crBorder, int nRadius)
{
    HBRUSH hBr = CreateSolidBrush(crFill);
    HPEN hPen = CreatePen(PS_SOLID, 1, crBorder);
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    RoundRect(hdc, pRc->left, pRc->top, pRc->right, pRc->bottom, nRadius, nRadius);
    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

static void UpdateScrollBar(CALENDAR_CANVAS* pC, int nClientH)
{
    SCROLLINFO si;
    BOOL bShowScroll = (pC->dwViewMode == VIEW_DAY || pC->dwViewMode == VIEW_WEEK);

    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_ALL;

    if (!bShowScroll) {
        pC->lScrollY = 0;
        pC->lMaxScrollY = 0;
        si.nMin = 0;
        si.nMax = 0;
        si.nPage = 1;
        si.nPos = 0;
        SetScrollInfo(pC->hwnd, SB_VERT, &si, TRUE);
        ShowScrollBar(pC->hwnd, SB_VERT, FALSE);
        return;
    }

    {
        LONG lHeaderH = ScaleDpi(pC->uDpi, 58);
        LONG lHourH = ScaleDpi(pC->uDpi, 56);
        LONG lContentH = lHourH * 24;
        LONG lViewportH = nClientH - lHeaderH;
        if (lViewportH < 100) lViewportH = 100;

        pC->lMaxScrollY = (lContentH > lViewportH) ? (lContentH - lViewportH) : 0;
        if (pC->lScrollY < 0) pC->lScrollY = 0;
        if (pC->lScrollY > pC->lMaxScrollY) pC->lScrollY = pC->lMaxScrollY;

        ShowScrollBar(pC->hwnd, SB_VERT, pC->lMaxScrollY > 0);
        si.nMin = 0;
        si.nMax = lContentH - 1;
        si.nPage = (UINT)lViewportH;
        si.nPos = pC->lScrollY;
        SetScrollInfo(pC->hwnd, SB_VERT, &si, TRUE);
    }
}

/* Helper to check if an occurrence intersects a specific calendar date */
static BOOL OccurrenceOnDate(const EVENT_OCCURRENCE* pOcc, const SYSTEMTIME* pstDay)
{
    SYSTEMTIME stDayStart = *pstDay;
    SYSTEMTIME stDayEnd = *pstDay;
    ULONGLONG ullStart;
    ULONGLONG ullEnd;

    stDayStart.wHour = 0; stDayStart.wMinute = 0; stDayStart.wSecond = 0;
    stDayEnd.wHour = 23; stDayEnd.wMinute = 59; stDayEnd.wSecond = 59;
    ullStart = Date_ToUInt64(&stDayStart);
    ullEnd = Date_ToUInt64(&stDayEnd);

    return (pOcc->ullEndFt > ullStart && pOcc->ullStartFt <= ullEnd);
}

/* Paint Day or Week hourly timeline view */
static void PaintTimeGridView(CALENDAR_CANVAS* pC, HDC hdc, int nW, int nH, BOOL bWeekMode)
{
    const APP_THEME* pT = pC->pTheme;
    UINT uDpi = pC->uDpi;
    LONG lGutterW = ScaleDpi(uDpi, 64);
    LONG lHeaderH = ScaleDpi(uDpi, 58);
    LONG lHourH = ScaleDpi(uDpi, 56);
    DWORD dwCols = bWeekMode ? 7 : 1;
    LONG lGridW = nW - lGutterW;
    LONG lColW = (dwCols > 0) ? (lGridW / (LONG)dwCols) : lGridW;
    SYSTEMTIME stFirstDay;
    SYSTEMTIME stLastDay;
    SYSTEMTIME stNow;
    EVENT_OCCURRENCE* pOccs = NULL;
    DWORD dwOccCount = 0;
    HFONT hFontHeader = CreateUiFont(uDpi, 9, FW_SEMIBOLD, FALSE);
    HFONT hFontDayNum = CreateUiFont(uDpi, 13, FW_BOLD, FALSE);
    HFONT hFontSmall  = CreateUiFont(uDpi, 8, FW_NORMAL, FALSE);
    HFONT hFontEvent  = CreateUiFont(uDpi, 9, FW_SEMIBOLD, FALSE);
    DWORD c, h;
    RECT rcAll = {0, 0, nW, nH};

    GetLocalTime(&stNow);
    FillSolidRect(hdc, &rcAll, pT->crBgSurface);

    if (bWeekMode) {
        Date_GetWeekStartMonday(&pC->stAnchor, &stFirstDay);
    } else {
        stFirstDay = pC->stAnchor;
        stFirstDay.wHour = 0; stFirstDay.wMinute = 0; stFirstDay.wSecond = 0;
    }

    stLastDay = stFirstDay;
    Date_AddDays(&stLastDay, (LONG)(dwCols - 1));
    stLastDay.wHour = 23; stLastDay.wMinute = 59; stLastDay.wSecond = 59;

    Recurrence_ExpandRange(pC->pEvents, &stFirstDay, &stLastDay, pC->szFilter, &pOccs, &dwOccCount);

    /* Clip and draw scrollable hourly body first */
    {
        HRGN hClip = CreateRectRgn(0, lHeaderH, nW, nH);
        SelectClipRgn(hdc, hClip);

        for (c = 0; c < dwCols; ++c) {
            SYSTEMTIME stColDay = stFirstDay;
            LONG lColX = lGutterW + (LONG)c * lColW;
            LONG lColRight = (c + 1 == dwCols) ? nW : (lColX + lColW);
            BOOL bIsToday;

            Date_AddDays(&stColDay, (LONG)c);
            bIsToday = (Date_CompareDate(&stColDay, &stNow) == 0);

            if (bIsToday) {
                RECT rcTodayCol = {lColX, lHeaderH, lColRight, nH};
                FillSolidRect(hdc, &rcTodayCol, pT->crBgToday);
            }

            /* Hour cells and hit slots */
            for (h = 0; h < 24; ++h) {
                LONG yTop = lHeaderH + (LONG)h * lHourH - pC->lScrollY;
                LONG yBot = yTop + lHourH;
                RECT rcSlot;
                SYSTEMTIME stSlot = stColDay;

                if (yBot < lHeaderH || yTop > nH) {
                    continue;
                }

                stSlot.wHour = (WORD)h;
                stSlot.wMinute = 0;
                stSlot.wSecond = 0;

                rcSlot.left = lColX;
                rcSlot.top = (yTop < lHeaderH) ? lHeaderH : yTop;
                rcSlot.right = lColRight;
                rcSlot.bottom = yBot;
                Canvas_AddHit(pC, &rcSlot, HIT_TIME_SLOT, 0, &stSlot);

                /* Hour border */
                DrawLineColor(hdc, lColX, yTop, lColRight, yTop, pT->crBorderSubtle, 1);
            }

            /* Vertical column separator */
            DrawLineColor(hdc, lColX, lHeaderH, lColX, nH, pT->crBorderSubtle, 1);
        }

        /* Hour labels in left gutter */
        SelectObject(hdc, hFontSmall);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, pT->crTextMuted);
        for (h = 0; h < 24; ++h) {
            LONG yTop = lHeaderH + (LONG)h * lHourH - pC->lScrollY;
            WCHAR szHour[16];
            RECT rcLbl = {ScaleDpi(uDpi, 6), yTop - ScaleDpi(uDpi, 7), lGutterW - ScaleDpi(uDpi, 8), yTop + ScaleDpi(uDpi, 14)};
            if (yTop + 20 < lHeaderH || yTop - 10 > nH) continue;
            if (h > 0) {
                _snwprintf(szHour, 16, L"%02u:00", (UINT)h);
                DrawTextW(hdc, szHour, -1, &rcLbl, DT_RIGHT | DT_SINGLELINE | DT_TOP);
            }
        }

        /* Render events per day column with simple overlap lane layout */
        for (c = 0; c < dwCols; ++c) {
            SYSTEMTIME stColDay = stFirstDay;
            LONG lColX = lGutterW + (LONG)c * lColW;
            LONG lColRight = (c + 1 == dwCols) ? nW : (lColX + lColW);
            DWORD dwDayIndices[128];
            DWORD dwDayCount = 0;
            DWORD i, j;

            Date_AddDays(&stColDay, (LONG)c);

            for (i = 0; i < dwOccCount && dwDayCount < 128; ++i) {
                if (OccurrenceOnDate(&pOccs[i], &stColDay)) {
                    dwDayIndices[dwDayCount++] = i;
                }
            }

            for (i = 0; i < dwDayCount; ++i) {
                const EVENT_OCCURRENCE* pOcc = &pOccs[dwDayIndices[i]];
                const CALENDAR_EVENT* pEv = &pC->pEvents->pItems[pOcc->dwEventIndex];
                LONG lStartMin;
                LONG lEndMin;
                LONG lOverlapTotal = 1;
                LONG lLane = 0;
                LONG yTop, yBot, lUsableW, lEvtW, xLeft;
                RECT rcEvt;
                COLORREF crAccent = pT->crEventColors[pEv->dwColorIndex % EVENT_COLOR_COUNT];
                COLORREF crTint   = pT->crEventBgTint[pEv->dwColorIndex % EVENT_COLOR_COUNT];
                WCHAR szLabel[256];

                if (Date_CompareDate(&pOcc->stStart, &stColDay) < 0) {
                    lStartMin = 0;
                } else {
                    lStartMin = (LONG)pOcc->stStart.wHour * 60 + (LONG)pOcc->stStart.wMinute;
                }
                if (Date_CompareDate(&pOcc->stEnd, &stColDay) > 0) {
                    lEndMin = 24 * 60;
                } else {
                    lEndMin = (LONG)pOcc->stEnd.wHour * 60 + (LONG)pOcc->stEnd.wMinute;
                    if (lEndMin <= lStartMin) lEndMin = lStartMin + 30;
                }
                if (pEv->bAllDay) {
                    lStartMin = 0;
                    lEndMin = 60;
                }

                /* Count concurrent overlaps on the same day */
                for (j = 0; j < dwDayCount; ++j) {
                    if (i == j) continue;
                    const EVENT_OCCURRENCE* pOther = &pOccs[dwDayIndices[j]];
                    if (pOther->ullStartFt < pOcc->ullEndFt && pOther->ullEndFt > pOcc->ullStartFt) {
                        lOverlapTotal++;
                        if (j < i) lLane++;
                    }
                }
                if (lOverlapTotal > 4) lOverlapTotal = 4;
                lLane = lLane % lOverlapTotal;

                yTop = lHeaderH + MulDiv(lStartMin, lHourH, 60) - pC->lScrollY;
                yBot = lHeaderH + MulDiv(lEndMin, lHourH, 60) - pC->lScrollY;
                if (yBot - yTop < ScaleDpi(uDpi, 20)) {
                    yBot = yTop + ScaleDpi(uDpi, 20);
                }

                lUsableW = (lColRight - lColX) - ScaleDpi(uDpi, 6);
                lEvtW = (lOverlapTotal > 0) ? (lUsableW / lOverlapTotal) : lUsableW;
                xLeft = lColX + ScaleDpi(uDpi, 3) + lLane * lEvtW;

                rcEvt.left = xLeft;
                rcEvt.top = yTop + 1;
                rcEvt.right = xLeft + lEvtW - ScaleDpi(uDpi, 2);
                rcEvt.bottom = yBot - 1;

                if (rcEvt.bottom < lHeaderH || rcEvt.top > nH) {
                    continue;
                }

                DrawRoundPill(hdc, &rcEvt, crTint, crAccent, ScaleDpi(uDpi, 6));

                /* Left accent stripe */
                {
                    RECT rcBar = {rcEvt.left, rcEvt.top + 2, rcEvt.left + ScaleDpi(uDpi, 4), rcEvt.bottom - 2};
                    FillSolidRect(hdc, &rcBar, crAccent);
                }

                Canvas_AddHit(pC, &rcEvt, HIT_EVENT, pEv->dwId, &pOcc->stStart);

                {
                    WCHAR szDispTitle[256];
                    Recurrence_FormatDisplayTitle(pEv, &pOcc->stStart, szDispTitle, 256);
                    if (pEv->bAllDay) {
                        _snwprintf(szLabel, 256, L"Journ\u00E9e: %s", szDispTitle);
                    } else {
                        _snwprintf(szLabel, 256, L"%02u:%02u %s",
                                   (UINT)pOcc->stStart.wHour, (UINT)pOcc->stStart.wMinute, szDispTitle);
                    }
                    szLabel[255] = L'\0';
                }

                {
                    RECT rcTxt = {
                        rcEvt.left + ScaleDpi(uDpi, 7),
                        rcEvt.top + ScaleDpi(uDpi, 3),
                        rcEvt.right - ScaleDpi(uDpi, 4),
                        rcEvt.bottom - ScaleDpi(uDpi, 2)
                    };
                    SelectObject(hdc, hFontEvent);
                    SetTextColor(hdc, pT->crTextPrimary);
                    DrawTextW(hdc, szLabel, -1, &rcTxt, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
                }
            }

            /* Current time red indicator line on Today */
            if (Date_CompareDate(&stColDay, &stNow) == 0) {
                LONG lNowMins = (LONG)stNow.wHour * 60 + (LONG)stNow.wMinute;
                LONG yNow = lHeaderH + MulDiv(lNowMins, lHourH, 60) - pC->lScrollY;
                if (yNow >= lHeaderH && yNow <= nH) {
                    HBRUSH hDotBr = CreateSolidBrush(pT->crCurrentTime);
                    HPEN hDotPen = CreatePen(PS_SOLID, 1, pT->crCurrentTime);
                    HBRUSH hOldB = (HBRUSH)SelectObject(hdc, hDotBr);
                    HPEN hOldP = (HPEN)SelectObject(hdc, hDotPen);
                    int rDot = ScaleDpi(uDpi, 4);
                    Ellipse(hdc, lColX - rDot, yNow - rDot, lColX + rDot, yNow + rDot);
                    SelectObject(hdc, hOldB);
                    SelectObject(hdc, hOldP);
                    DeleteObject(hDotBr);
                    DeleteObject(hDotPen);
                    DrawLineColor(hdc, lColX, yNow, lColRight, yNow, pT->crCurrentTime, 2);
                }
            }
        }

        SelectClipRgn(hdc, NULL);
        DeleteObject(hClip);
    }

    /* Sticky header bar above the hourly grid */
    {
        RECT rcHead = {0, 0, nW, lHeaderH};
        FillSolidRect(hdc, &rcHead, pT->crBgHeader);
        DrawLineColor(hdc, 0, lHeaderH - 1, nW, lHeaderH - 1, pT->crBorderStrong, 1);

        for (c = 0; c < dwCols; ++c) {
            SYSTEMTIME stColDay = stFirstDay;
            LONG lColX = lGutterW + (LONG)c * lColW;
            LONG lColRight = (c + 1 == dwCols) ? nW : (lColX + lColW);
            WCHAR szDow[32];
            WCHAR szDayNum[16];
            BOOL bIsToday;
            RECT rcDow;
            RECT rcNum;

            Date_AddDays(&stColDay, (LONG)c);
            bIsToday = (Date_CompareDate(&stColDay, &stNow) == 0);

            DrawLineColor(hdc, lColX, 0, lColX, lHeaderH, pT->crBorderSubtle, 1);

            if (bWeekMode) {
                Date_GetWeekdayShort(c, szDow, 32);
            } else {
                Date_GetWeekdayFull(Date_DayOfWeek(stColDay.wYear, stColDay.wMonth, stColDay.wDay), szDow, 32);
            }

            rcDow.left = lColX;
            rcDow.top = ScaleDpi(uDpi, 6);
            rcDow.right = lColRight;
            rcDow.bottom = ScaleDpi(uDpi, 24);

            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, hFontHeader);
            SetTextColor(hdc, bIsToday ? pT->crAccent : pT->crTextSecondary);
            DrawTextW(hdc, szDow, -1, &rcDow, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

            _snwprintf(szDayNum, 16, L"%u", (UINT)stColDay.wDay);
            rcNum.left = (lColX + lColRight) / 2 - ScaleDpi(uDpi, 15);
            rcNum.right = (lColX + lColRight) / 2 + ScaleDpi(uDpi, 15);
            rcNum.top = ScaleDpi(uDpi, 25);
            rcNum.bottom = ScaleDpi(uDpi, 53);

            if (bIsToday) {
                HBRUSH hBr = CreateSolidBrush(pT->crAccent);
                HPEN hPn = CreatePen(PS_SOLID, 1, pT->crAccent);
                HBRUSH hOB = (HBRUSH)SelectObject(hdc, hBr);
                HPEN hOP = (HPEN)SelectObject(hdc, hPn);
                Ellipse(hdc, rcNum.left, rcNum.top, rcNum.right, rcNum.bottom);
                SelectObject(hdc, hOB);
                SelectObject(hdc, hOP);
                DeleteObject(hBr);
                DeleteObject(hPn);
                SetTextColor(hdc, pT->crAccentText);
            } else {
                SetTextColor(hdc, pT->crTextPrimary);
            }
            SelectObject(hdc, hFontDayNum);
            DrawTextW(hdc, szDayNum, -1, &rcNum, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }
    }

    if (pOccs) free(pOccs);
    DeleteObject(hFontHeader);
    DeleteObject(hFontDayNum);
    DeleteObject(hFontSmall);
    DeleteObject(hFontEvent);
}

/* Paint Month 6x7 grid view */
static void PaintMonthView(CALENDAR_CANVAS* pC, HDC hdc, int nW, int nH)
{
    const APP_THEME* pT = pC->pTheme;
    UINT uDpi = pC->uDpi;
    LONG lHeaderH = ScaleDpi(uDpi, 32);
    LONG lBodyH = nH - lHeaderH;
    LONG lColW = nW / 7;
    LONG lRowH = (lBodyH > 60) ? (lBodyH / 6) : 20;
    SYSTEMTIME stMonthFirst = pC->stAnchor;
    SYSTEMTIME stGridStart;
    SYSTEMTIME stGridEnd;
    SYSTEMTIME stNow;
    EVENT_OCCURRENCE* pOccs = NULL;
    DWORD dwOccCount = 0;
    HFONT hFontHeader = CreateUiFont(uDpi, 9, FW_SEMIBOLD, FALSE);
    HFONT hFontDayNum = CreateUiFont(uDpi, 9, FW_BOLD, FALSE);
    HFONT hFontEvent  = CreateUiFont(uDpi, 8, FW_SEMIBOLD, FALSE);
    DWORD r, c;
    RECT rcAll = {0, 0, nW, nH};

    GetLocalTime(&stNow);
    FillSolidRect(hdc, &rcAll, pT->crBgSurface);

    stMonthFirst.wDay = 1;
    stMonthFirst.wHour = 0; stMonthFirst.wMinute = 0; stMonthFirst.wSecond = 0;
    Date_NormalizeTime(&stMonthFirst);

    Date_GetWeekStartMonday(&stMonthFirst, &stGridStart);
    stGridEnd = stGridStart;
    Date_AddDays(&stGridEnd, 41);
    stGridEnd.wHour = 23; stGridEnd.wMinute = 59; stGridEnd.wSecond = 59;

    Recurrence_ExpandRange(pC->pEvents, &stGridStart, &stGridEnd, pC->szFilter, &pOccs, &dwOccCount);

    /* Weekday header strip */
    {
        RECT rcHead = {0, 0, nW, lHeaderH};
        FillSolidRect(hdc, &rcHead, pT->crBgHeader);
        DrawLineColor(hdc, 0, lHeaderH - 1, nW, lHeaderH - 1, pT->crBorderStrong, 1);

        SelectObject(hdc, hFontHeader);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, pT->crTextSecondary);

        for (c = 0; c < 7; ++c) {
            WCHAR szDow[16];
            RECT rcCol = {(LONG)c * lColW, 0, (c == 6) ? nW : ((LONG)(c + 1) * lColW), lHeaderH};
            Date_GetWeekdayShort(c, szDow, 16);
            DrawTextW(hdc, szDow, -1, &rcCol, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }
    }

    /* 6x7 day cells */
    for (r = 0; r < 6; ++r) {
        for (c = 0; c < 7; ++c) {
            SYSTEMTIME stCell = stGridStart;
            LONG xL = (LONG)c * lColW;
            LONG yT = lHeaderH + (LONG)r * lRowH;
            LONG xR = (c == 6) ? nW : (xL + lColW);
            LONG yB = (r == 5) ? nH : (yT + lRowH);
            RECT rcCell = {xL, yT, xR, yB};
            BOOL bInMonth;
            BOOL bIsToday;
            BOOL bIsSelected;
            LONG lPillH = ScaleDpi(uDpi, 19);
            LONG lPillGap = ScaleDpi(uDpi, 3);
            LONG lDayHeaderH = ScaleDpi(uDpi, 26);
            LONG lMaxPills;
            LONG lPlaced = 0;
            DWORD dwDayTotal = 0;
            DWORD i;

            Date_AddDays(&stCell, (LONG)(r * 7 + c));
            bInMonth = (stCell.wMonth == stMonthFirst.wMonth);
            bIsToday = (Date_CompareDate(&stCell, &stNow) == 0);
            bIsSelected = (Date_CompareDate(&stCell, &pC->stSelected) == 0);

            if (bIsToday) {
                FillSolidRect(hdc, &rcCell, pT->crBgToday);
            } else if (bIsSelected) {
                FillSolidRect(hdc, &rcCell, pT->crBgSelected);
            } else if (!bInMonth) {
                FillSolidRect(hdc, &rcCell, pT->crBgGridAlt);
            }

            DrawLineColor(hdc, xR - 1, yT, xR - 1, yB, pT->crBorderSubtle, 1);
            DrawLineColor(hdc, xL, yB - 1, xR, yB - 1, pT->crBorderSubtle, 1);

            Canvas_AddHit(pC, &rcCell, HIT_DAY_CELL, 0, &stCell);

            /* Day number badge */
            {
                WCHAR szNum[16];
                RECT rcBadge = {xL + ScaleDpi(uDpi, 6), yT + ScaleDpi(uDpi, 4),
                                xL + ScaleDpi(uDpi, 28), yT + ScaleDpi(uDpi, 24)};
                _snwprintf(szNum, 16, L"%u", (UINT)stCell.wDay);
                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, hFontDayNum);
                if (bIsToday) {
                    HBRUSH hBr = CreateSolidBrush(pT->crAccent);
                    HPEN hPn = CreatePen(PS_SOLID, 1, pT->crAccent);
                    HBRUSH hOB = (HBRUSH)SelectObject(hdc, hBr);
                    HPEN hOP = (HPEN)SelectObject(hdc, hPn);
                    Ellipse(hdc, rcBadge.left, rcBadge.top, rcBadge.right, rcBadge.bottom);
                    SelectObject(hdc, hOB);
                    SelectObject(hdc, hOP);
                    DeleteObject(hBr);
                    DeleteObject(hPn);
                    SetTextColor(hdc, pT->crAccentText);
                } else {
                    SetTextColor(hdc, bInMonth ? pT->crTextPrimary : pT->crTextMuted);
                }
                DrawTextW(hdc, szNum, -1, &rcBadge, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            }

            /* Event pills inside cell */
            lMaxPills = (yB - yT - lDayHeaderH - ScaleDpi(uDpi, 4)) / (lPillH + lPillGap);
            if (lMaxPills < 0) lMaxPills = 0;

            for (i = 0; i < dwOccCount; ++i) {
                if (OccurrenceOnDate(&pOccs[i], &stCell)) {
                    dwDayTotal++;
                }
            }

            for (i = 0; i < dwOccCount; ++i) {
                if (!OccurrenceOnDate(&pOccs[i], &stCell)) continue;
                if (lMaxPills == 0) break;

                /* Reserve last line for "+N autre(s)" if overflow exists */
                if (dwDayTotal > (DWORD)lMaxPills && lPlaced == lMaxPills - 1) {
                    RECT rcMore = {
                        xL + ScaleDpi(uDpi, 5),
                        yT + lDayHeaderH + lPlaced * (lPillH + lPillGap),
                        xR - ScaleDpi(uDpi, 5),
                        yT + lDayHeaderH + lPlaced * (lPillH + lPillGap) + lPillH
                    };
                    WCHAR szMore[48];
                    DWORD dwRemain = dwDayTotal - (DWORD)lPlaced;
                    _snwprintf(szMore, 48, L"+%u autre%s", (UINT)dwRemain, dwRemain > 1 ? L"s" : L"");
                    SelectObject(hdc, hFontEvent);
                    SetTextColor(hdc, pT->crAccent);
                    DrawTextW(hdc, szMore, -1, &rcMore, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                    Canvas_AddHit(pC, &rcMore, HIT_MORE_LINK, 0, &stCell);
                    break;
                }

                {
                    const CALENDAR_EVENT* pEv = &pC->pEvents->pItems[pOccs[i].dwEventIndex];
                    COLORREF crAccent = pT->crEventColors[pEv->dwColorIndex % EVENT_COLOR_COUNT];
                    COLORREF crTint   = pT->crEventBgTint[pEv->dwColorIndex % EVENT_COLOR_COUNT];
                    RECT rcPill = {
                        xL + ScaleDpi(uDpi, 4),
                        yT + lDayHeaderH + lPlaced * (lPillH + lPillGap),
                        xR - ScaleDpi(uDpi, 5),
                        yT + lDayHeaderH + lPlaced * (lPillH + lPillGap) + lPillH
                    };
                    RECT rcTxt;
                    WCHAR szLabel[160];

                    DrawRoundPill(hdc, &rcPill, crTint, crAccent, ScaleDpi(uDpi, 5));
                    Canvas_AddHit(pC, &rcPill, HIT_EVENT, pEv->dwId, &pOccs[i].stStart);

                    {
                        WCHAR szDispTitle[160];
                        Recurrence_FormatDisplayTitle(pEv, &pOccs[i].stStart, szDispTitle, 160);
                        if (pEv->bAllDay) {
                            wcsncpy(szLabel, szDispTitle, 159);
                        } else {
                            _snwprintf(szLabel, 160, L"%02u:%02u %s",
                                       (UINT)pOccs[i].stStart.wHour, (UINT)pOccs[i].stStart.wMinute, szDispTitle);
                        }
                        szLabel[159] = L'\0';
                    }

                    rcTxt.left = rcPill.left + ScaleDpi(uDpi, 5);
                    rcTxt.top = rcPill.top;
                    rcTxt.right = rcPill.right - ScaleDpi(uDpi, 4);
                    rcTxt.bottom = rcPill.bottom;
                    SelectObject(hdc, hFontEvent);
                    SetTextColor(hdc, pT->crTextPrimary);
                    DrawTextW(hdc, szLabel, -1, &rcTxt, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

                    lPlaced++;
                    if (lPlaced >= lMaxPills) break;
                }
            }
        }
    }

    if (pOccs) free(pOccs);
    DeleteObject(hFontHeader);
    DeleteObject(hFontDayNum);
    DeleteObject(hFontEvent);
}

typedef struct _YEAR_DAY_INFO {
    DWORD dwCount;
    DWORD dwFirstEventId;
    DWORD dwColorIndex;
} YEAR_DAY_INFO;

/* Popup menu allowing user to pick an event to edit when a Year-view day has multiple events */
static void ShowYearDayEventsMenu(CALENDAR_CANVAS* pC, const SYSTEMTIME* pstDay, POINT ptClient)
{
    SYSTEMTIME stStart = *pstDay;
    SYSTEMTIME stEnd = *pstDay;
    EVENT_OCCURRENCE* pOccs = NULL;
    DWORD dwCount = 0;
    DWORD i;
    HMENU hMenu;
    POINT ptScreen = ptClient;
    UINT uCmd;

    stStart.wHour = 0; stStart.wMinute = 0; stStart.wSecond = 0;
    stEnd.wHour = 23; stEnd.wMinute = 59; stEnd.wSecond = 59;
    Recurrence_ExpandRange(pC->pEvents, &stStart, &stEnd, pC->szFilter, &pOccs, &dwCount);

    if (dwCount == 1) {
        DWORD dwId = pOccs[0].dwEventId;
        free(pOccs);
        SendMessageW(pC->hwndParent, WM_APP_CANVAS_EDIT, (WPARAM)dwId, 0);
        return;
    }

    hMenu = CreatePopupMenu();
    if (!hMenu) {
        if (pOccs) free(pOccs);
        return;
    }

    for (i = 0; i < dwCount && i < 32; ++i) {
        const CALENDAR_EVENT* pEv = &pC->pEvents->pItems[pOccs[i].dwEventIndex];
        WCHAR szDispTitle[128];
        WCHAR szMenuText[192];

        Recurrence_FormatDisplayTitle(pEv, &pOccs[i].stStart, szDispTitle, 128);
        if (pEv->bAllDay) {
            _snwprintf(szMenuText, 192, L"Journ\u00E9e \u2014 %s", szDispTitle);
        } else {
            _snwprintf(szMenuText, 192, L"%02u:%02u \u2014 %s",
                       (UINT)pOccs[i].stStart.wHour, (UINT)pOccs[i].stStart.wMinute, szDispTitle);
        }
        szMenuText[191] = L'\0';
        AppendMenuW(hMenu, MF_STRING, (UINT_PTR)(1000 + i), szMenuText);
    }

    if (dwCount > 0) {
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    }
    AppendMenuW(hMenu, MF_STRING, 5001, L"Ouvrir cette journ\u00E9e");
    AppendMenuW(hMenu, MF_STRING, 5002, L"+ Nouvel \u00E9v\u00E9nement ce jour...");

    ClientToScreen(pC->hwnd, &ptScreen);
    uCmd = (UINT)TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                                ptScreen.x, ptScreen.y, 0, pC->hwnd, NULL);
    DestroyMenu(hMenu);

    if (uCmd >= 1000 && (DWORD)(uCmd - 1000) < dwCount) {
        DWORD dwEditId = pOccs[uCmd - 1000].dwEventId;
        free(pOccs);
        SendMessageW(pC->hwndParent, WM_APP_CANVAS_EDIT, (WPARAM)dwEditId, 0);
        return;
    }
    if (pOccs) {
        free(pOccs);
    }
    if (uCmd == 5001) {
        SYSTEMTIME stNav = *pstDay;
        SendMessageW(pC->hwndParent, WM_APP_CANVAS_NAVIGATE, VIEW_DAY, (LPARAM)&stNav);
    } else if (uCmd == 5002) {
        SYSTEMTIME stNew = *pstDay;
        stNew.wHour = 9;
        stNew.wMinute = 0;
        stNew.wSecond = 0;
        SendMessageW(pC->hwndParent, WM_APP_CANVAS_CREATE, 0, (LPARAM)&stNew);
    }
}

/* Paint Year 4x3 mini-month overview */
static void PaintYearView(CALENDAR_CANVAS* pC, HDC hdc, int nW, int nH)
{
    const APP_THEME* pT = pC->pTheme;
    UINT uDpi = pC->uDpi;
    LONG lPad = ScaleDpi(uDpi, 14);
    LONG lBoxW = (nW - lPad * 5) / 4;
    LONG lBoxH = (nH - lPad * 4) / 3;
    SYSTEMTIME stYearStart;
    SYSTEMTIME stYearEnd;
    SYSTEMTIME stNow;
    EVENT_OCCURRENCE* pOccs = NULL;
    DWORD dwOccCount = 0;
    YEAR_DAY_INFO dayInfo[12][32];
    HFONT hFontMonth = CreateUiFont(uDpi, 10, FW_BOLD, FALSE);
    HFONT hFontDow   = CreateUiFont(uDpi, 8, FW_SEMIBOLD, FALSE);
    HFONT hFontDay   = CreateUiFont(uDpi, 8, FW_NORMAL, FALSE);
    DWORD m, i;
    RECT rcAll = {0, 0, nW, nH};

    GetLocalTime(&stNow);
    FillSolidRect(hdc, &rcAll, pT->crBgWindow);
    ZeroMemory(dayInfo, sizeof(dayInfo));

    ZeroMemory(&stYearStart, sizeof(stYearStart));
    stYearStart.wYear = pC->stAnchor.wYear;
    stYearStart.wMonth = 1;
    stYearStart.wDay = 1;

    ZeroMemory(&stYearEnd, sizeof(stYearEnd));
    stYearEnd.wYear = pC->stAnchor.wYear;
    stYearEnd.wMonth = 12;
    stYearEnd.wDay = 31;
    stYearEnd.wHour = 23; stYearEnd.wMinute = 59; stYearEnd.wSecond = 59;

    Recurrence_ExpandRange(pC->pEvents, &stYearStart, &stYearEnd, pC->szFilter, &pOccs, &dwOccCount);

    /* Track event counts, first event ID, and color for every day in the year */
    for (i = 0; i < dwOccCount; ++i) {
        SYSTEMTIME stIter = pOccs[i].stStart;
        SYSTEMTIME stLastDay = pOccs[i].stEnd;
        const CALENDAR_EVENT* pEv = &pC->pEvents->pItems[pOccs[i].dwEventIndex];
        stIter.wHour = 0; stIter.wMinute = 0; stIter.wSecond = 0;
        if (stLastDay.wHour == 0 && stLastDay.wMinute == 0 && stLastDay.wSecond == 0 &&
            Date_CompareDate(&stLastDay, &stIter) > 0) {
            Date_AddDays(&stLastDay, -1);
        }
        while (Date_CompareDate(&stIter, &stLastDay) <= 0) {
            if (stIter.wYear == pC->stAnchor.wYear && stIter.wMonth >= 1 && stIter.wMonth <= 12 &&
                stIter.wDay >= 1 && stIter.wDay <= 31) {
                YEAR_DAY_INFO* pInfo = &dayInfo[stIter.wMonth - 1][stIter.wDay];
                if (pInfo->dwCount == 0) {
                    pInfo->dwFirstEventId = pOccs[i].dwEventId;
                    pInfo->dwColorIndex = pEv->dwColorIndex;
                }
                pInfo->dwCount++;
            }
            if (stIter.wYear > pC->stAnchor.wYear) break;
            Date_AddDays(&stIter, 1);
        }
    }

    if (lBoxW < 100) lBoxW = 100;
    if (lBoxH < 100) lBoxH = 100;

    for (m = 0; m < 12; ++m) {
        DWORD dwRow = m / 4;
        DWORD dwCol = m % 4;
        LONG x0 = lPad + (LONG)dwCol * (lBoxW + lPad);
        LONG y0 = lPad + (LONG)dwRow * (lBoxH + lPad);
        RECT rcCard = {x0, y0, x0 + lBoxW, y0 + lBoxH};
        RECT rcTitle = {x0 + ScaleDpi(uDpi, 10), y0 + ScaleDpi(uDpi, 6),
                        x0 + lBoxW - ScaleDpi(uDpi, 10), y0 + ScaleDpi(uDpi, 26)};
        WCHAR szMonth[32];
        SYSTEMTIME stMonth = {0};
        DWORD dwFirstMonIdx;
        DWORD dwDays;
        LONG lGridTop = y0 + ScaleDpi(uDpi, 28);
        LONG lCellW = (lBoxW - ScaleDpi(uDpi, 16)) / 7;
        LONG lCellH = (lBoxH - ScaleDpi(uDpi, 34)) / 7;
        DWORD d, c;

        stMonth.wYear = pC->stAnchor.wYear;
        stMonth.wMonth = (WORD)(m + 1);
        stMonth.wDay = 1;
        Date_NormalizeTime(&stMonth);

        DrawRoundPill(hdc, &rcCard, pT->crBgSurface, pT->crBorderSubtle, ScaleDpi(uDpi, 8));

        Date_GetMonthName(m + 1, szMonth, 32);
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, hFontMonth);
        SetTextColor(hdc, (stNow.wYear == stMonth.wYear && stNow.wMonth == stMonth.wMonth) ? pT->crAccent : pT->crTextPrimary);
        DrawTextW(hdc, szMonth, -1, &rcTitle, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        Canvas_AddHit(pC, &rcTitle, HIT_YEAR_MONTH, 0, &stMonth);

        /* Initials L M M J V S D */
        SelectObject(hdc, hFontDow);
        SetTextColor(hdc, pT->crTextMuted);
        for (c = 0; c < 7; ++c) {
            WCHAR szShort[8];
            WCHAR szInitial[2];
            RECT rcDow = {
                x0 + ScaleDpi(uDpi, 8) + (LONG)c * lCellW,
                lGridTop,
                x0 + ScaleDpi(uDpi, 8) + (LONG)(c + 1) * lCellW,
                lGridTop + lCellH
            };
            Date_GetWeekdayShort(c, szShort, 8);
            szInitial[0] = szShort[0];
            szInitial[1] = L'\0';
            DrawTextW(hdc, szInitial, -1, &rcDow, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }

        dwFirstMonIdx = Date_MondayBasedDayOfWeek(&stMonth);
        dwDays = Date_DaysInMonth(stMonth.wYear, stMonth.wMonth);

        SelectObject(hdc, hFontDay);
        for (d = 1; d <= dwDays; ++d) {
            DWORD dwSlot = dwFirstMonIdx + (d - 1);
            DWORD r = dwSlot / 7;
            DWORD col = dwSlot % 7;
            LONG cx = x0 + ScaleDpi(uDpi, 8) + (LONG)col * lCellW;
            LONG cy = lGridTop + lCellH + (LONG)r * lCellH;
            RECT rcDay = {cx, cy, cx + lCellW, cy + lCellH};
            SYSTEMTIME stDay = stMonth;
            BOOL bToday;
            const YEAR_DAY_INFO* pInfo = &dayInfo[m][d];
            WCHAR szD[8];

            stDay.wDay = (WORD)d;
            Date_NormalizeTime(&stDay);
            bToday = (Date_CompareDate(&stDay, &stNow) == 0);

            if (bToday) {
                LONG lSide = (lCellW < lCellH ? lCellW : lCellH) - 2;
                LONG lMidX = (rcDay.left + rcDay.right) / 2;
                LONG lMidY = (rcDay.top + rcDay.bottom) / 2;
                HBRUSH hBr = CreateSolidBrush(pT->crAccent);
                HPEN hPn = CreatePen(PS_SOLID, (pInfo->dwCount > 0) ? 2 : 1,
                                     (pInfo->dwCount > 0) ? pT->crEventColors[pInfo->dwColorIndex % EVENT_COLOR_COUNT] : pT->crAccent);
                HBRUSH hOB = (HBRUSH)SelectObject(hdc, hBr);
                HPEN hOP = (HPEN)SelectObject(hdc, hPn);
                Ellipse(hdc, lMidX - lSide / 2, lMidY - lSide / 2, lMidX + lSide / 2, lMidY + lSide / 2);
                SelectObject(hdc, hOB);
                SelectObject(hdc, hOP);
                DeleteObject(hBr);
                DeleteObject(hPn);
                SetTextColor(hdc, pT->crAccentText);
            } else if (pInfo->dwCount > 0) {
                LONG lSide = (lCellW < lCellH ? lCellW : lCellH) - 2;
                LONG lMidX = (rcDay.left + rcDay.right) / 2;
                LONG lMidY = (rcDay.top + rcDay.bottom) / 2;
                DWORD dwColIdx = pInfo->dwColorIndex % EVENT_COLOR_COUNT;
                HBRUSH hBr = CreateSolidBrush(pT->crEventBgTint[dwColIdx]);
                HPEN hPn = CreatePen(PS_SOLID, (pInfo->dwCount > 1) ? 2 : 1, pT->crEventColors[dwColIdx]);
                HBRUSH hOB = (HBRUSH)SelectObject(hdc, hBr);
                HPEN hOP = (HPEN)SelectObject(hdc, hPn);
                Ellipse(hdc, lMidX - lSide / 2, lMidY - lSide / 2, lMidX + lSide / 2, lMidY + lSide / 2);
                SelectObject(hdc, hOB);
                SelectObject(hdc, hOP);
                DeleteObject(hBr);
                DeleteObject(hPn);
                SetTextColor(hdc, pT->crTextPrimary);
            } else {
                SetTextColor(hdc, pT->crTextPrimary);
            }

            _snwprintf(szD, 8, L"%u", (UINT)d);
            DrawTextW(hdc, szD, -1, &rcDay, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

            if (pInfo->dwCount > 0) {
                Canvas_AddHit(pC, &rcDay, HIT_YEAR_EVENT_DAY,
                              (pInfo->dwCount == 1) ? pInfo->dwFirstEventId : 0, &stDay);
            } else {
                Canvas_AddHit(pC, &rcDay, HIT_DAY_CELL, 0, &stDay);
            }
        }
    }

    if (pOccs) free(pOccs);
    DeleteObject(hFontMonth);
    DeleteObject(hFontDow);
    DeleteObject(hFontDay);
}

static LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALENDAR_CANVAS* pC = (CALENDAR_CANVAS*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCs = (CREATESTRUCTW*)lParam;
            pC = (CALENDAR_CANVAS*)pCs->lpCreateParams;
            pC->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pC);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1; /* Handled cleanly via double-buffering in WM_PAINT */

        case WM_SIZE: {
            if (pC) {
                UpdateScrollBar(pC, HIWORD(lParam));
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_VSCROLL: {
            if (pC && (pC->dwViewMode == VIEW_DAY || pC->dwViewMode == VIEW_WEEK)) {
                SCROLLINFO si;
                LONG lNewPos;
                si.cbSize = sizeof(SCROLLINFO);
                si.fMask = SIF_ALL;
                GetScrollInfo(hwnd, SB_VERT, &si);
                lNewPos = si.nPos;

                switch (LOWORD(wParam)) {
                    case SB_LINEUP:        lNewPos -= ScaleDpi(pC->uDpi, 28); break;
                    case SB_LINEDOWN:      lNewPos += ScaleDpi(pC->uDpi, 28); break;
                    case SB_PAGEUP:        lNewPos -= (LONG)si.nPage; break;
                    case SB_PAGEDOWN:      lNewPos += (LONG)si.nPage; break;
                    case SB_THUMBTRACK:    lNewPos = si.nTrackPos; break;
                    default: break;
                }
                if (lNewPos < 0) lNewPos = 0;
                if (lNewPos > pC->lMaxScrollY) lNewPos = pC->lMaxScrollY;
                pC->lScrollY = lNewPos;
                SetScrollPos(hwnd, SB_VERT, pC->lScrollY, TRUE);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            if (pC && (pC->dwViewMode == VIEW_DAY || pC->dwViewMode == VIEW_WEEK)) {
                short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                LONG lStep = ScaleDpi(pC->uDpi, 48);
                pC->lScrollY -= (zDelta / WHEEL_DELTA) * lStep;
                if (pC->lScrollY < 0) pC->lScrollY = 0;
                if (pC->lScrollY > pC->lMaxScrollY) pC->lScrollY = pC->lMaxScrollY;
                SetScrollPos(hwnd, SB_VERT, pC->lScrollY, TRUE);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        }
        case WM_SETCURSOR: {
            if (pC && LOWORD(lParam) == HTCLIENT) {
                POINT pt;
                LONG idx;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                idx = Canvas_HitTest(pC, pt);
                if (idx >= 0) {
                    HIT_KIND k = pC->pHits[idx].kind;
                    if (k == HIT_EVENT || k == HIT_YEAR_EVENT_DAY || k == HIT_YEAR_MONTH || k == HIT_MORE_LINK) {
                        SetCursor(LoadCursorW(NULL, IDC_HAND));
                        return TRUE;
                    }
                }
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            if (pC) {
                POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
                LONG idx = Canvas_HitTest(pC, pt);
                SetFocus(hwnd);
                if (idx >= 0) {
                    const HIT_ITEM* pH = &pC->pHits[idx];
                    if (pH->kind == HIT_DAY_CELL || pH->kind == HIT_TIME_SLOT) {
                        pC->stSelected = pH->stDate;
                        InvalidateRect(hwnd, NULL, FALSE);
                    } else if (pH->kind == HIT_EVENT) {
                        pC->dwSelectedEventId = pH->dwEventId;
                        InvalidateRect(hwnd, NULL, FALSE);
                    } else if (pH->kind == HIT_YEAR_EVENT_DAY) {
                        SYSTEMTIME stPickedDay = pH->stDate;
                        DWORD dwEvId = pH->dwEventId;
                        pC->stSelected = stPickedDay;
                        InvalidateRect(hwnd, NULL, FALSE);
                        if (dwEvId != 0) {
                            SendMessageW(pC->hwndParent, WM_APP_CANVAS_EDIT, (WPARAM)dwEvId, 0);
                        } else {
                            ShowYearDayEventsMenu(pC, &stPickedDay, pt);
                        }
                    } else if (pH->kind == HIT_MORE_LINK) {
                        SYSTEMTIME stTarget = pH->stDate;
                        SendMessageW(pC->hwndParent, WM_APP_CANVAS_NAVIGATE, VIEW_DAY, (LPARAM)&stTarget);
                    } else if (pH->kind == HIT_YEAR_MONTH) {
                        SYSTEMTIME stTarget = pH->stDate;
                        SendMessageW(pC->hwndParent, WM_APP_CANVAS_NAVIGATE, VIEW_MONTH, (LPARAM)&stTarget);
                    }
                }
            }
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            if (pC) {
                POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
                LONG idx = Canvas_HitTest(pC, pt);
                if (idx >= 0) {
                    const HIT_ITEM* pH = &pC->pHits[idx];
                    if (pH->kind == HIT_EVENT) {
                        SendMessageW(pC->hwndParent, WM_APP_CANVAS_EDIT, (WPARAM)pH->dwEventId, 0);
                    } else if (pH->kind == HIT_YEAR_EVENT_DAY) {
                        SYSTEMTIME stPickedDay = pH->stDate;
                        if (pH->dwEventId != 0) {
                            SendMessageW(pC->hwndParent, WM_APP_CANVAS_EDIT, (WPARAM)pH->dwEventId, 0);
                        } else {
                            ShowYearDayEventsMenu(pC, &stPickedDay, pt);
                        }
                    } else if (pH->kind == HIT_DAY_CELL || pH->kind == HIT_TIME_SLOT) {
                        SYSTEMTIME stTarget = pH->stDate;
                        if (pH->kind == HIT_DAY_CELL) {
                            stTarget.wHour = 9;
                            stTarget.wMinute = 0;
                        }
                        SendMessageW(pC->hwndParent, WM_APP_CANVAS_CREATE, 0, (LPARAM)&stTarget);
                    }
                }
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            {
                int nW = rcClient.right - rcClient.left;
                int nH = rcClient.bottom - rcClient.top;
                if (pC && nW > 0 && nH > 0) {
                    HDC hdcMem = CreateCompatibleDC(hdc);
                    HBITMAP hbm = CreateCompatibleBitmap(hdc, nW, nH);
                    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hbm);

                    Canvas_ClearHits(pC);
                    if (pC->dwViewMode == VIEW_DAY) {
                        PaintTimeGridView(pC, hdcMem, nW, nH, FALSE);
                    } else if (pC->dwViewMode == VIEW_WEEK) {
                        PaintTimeGridView(pC, hdcMem, nW, nH, TRUE);
                    } else if (pC->dwViewMode == VIEW_MONTH) {
                        PaintMonthView(pC, hdcMem, nW, nH);
                    } else {
                        PaintYearView(pC, hdcMem, nW, nH);
                    }

                    BitBlt(hdc, 0, 0, nW, nH, hdcMem, 0, 0, SRCCOPY);
                    SelectObject(hdcMem, hOldBmp);
                    DeleteObject(hbm);
                    DeleteDC(hdcMem);
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY: {
            if (pC) {
                if (pC->pHits) {
                    free(pC->pHits);
                }
                free(pC);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

BOOL CalendarCanvas_Register(HINSTANCE hInst)
{
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = CanvasWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = AGENDA_CANVAS_CLASS;
    return (RegisterClassExW(&wc) != 0);
}

HWND CalendarCanvas_Create(
    HINSTANCE hInst,
    HWND hwndParent,
    const EVENT_LIST* pEvents,
    const APP_THEME* pTheme
)
{
    CALENDAR_CANVAS* pC = (CALENDAR_CANVAS*)calloc(1, sizeof(CALENDAR_CANVAS));
    if (!pC) {
        return NULL;
    }
    pC->hwndParent = hwndParent;
    pC->dwViewMode = VIEW_MONTH;
    GetLocalTime(&pC->stAnchor);
    pC->stSelected = pC->stAnchor;
    pC->pEvents = pEvents;
    pC->pTheme = pTheme;
    pC->uDpi = 96;
    pC->lScrollY = ScaleDpi(96, 56 * 7); /* Scroll to 07:00 initially */
    pC->lHoverIndex = -1;

    return CreateWindowExW(
        0,
        AGENDA_CANVAS_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL,
        0, 0, 100, 100,
        hwndParent,
        NULL,
        hInst,
        pC
    );
}

void CalendarCanvas_SetView(HWND hwndCanvas, DWORD dwViewMode, const SYSTEMTIME* pstAnchor)
{
    CALENDAR_CANVAS* pC = (CALENDAR_CANVAS*)GetWindowLongPtrW(hwndCanvas, GWLP_USERDATA);
    RECT rc;
    if (!pC) return;

    pC->dwViewMode = dwViewMode;
    if (pstAnchor) {
        pC->stAnchor = *pstAnchor;
        pC->stSelected = *pstAnchor;
    }
    if ((dwViewMode == VIEW_DAY || dwViewMode == VIEW_WEEK) && pC->lScrollY == 0) {
        pC->lScrollY = ScaleDpi(pC->uDpi, 56 * 7);
    }
    GetClientRect(hwndCanvas, &rc);
    UpdateScrollBar(pC, rc.bottom - rc.top);
    InvalidateRect(hwndCanvas, NULL, FALSE);
}

void CalendarCanvas_SetFilter(HWND hwndCanvas, const WCHAR* pszFilter)
{
    CALENDAR_CANVAS* pC = (CALENDAR_CANVAS*)GetWindowLongPtrW(hwndCanvas, GWLP_USERDATA);
    if (!pC) return;
    if (pszFilter) {
        wcsncpy(pC->szFilter, pszFilter, 127);
        pC->szFilter[127] = L'\0';
    } else {
        pC->szFilter[0] = L'\0';
    }
    InvalidateRect(hwndCanvas, NULL, FALSE);
}

void CalendarCanvas_Refresh(HWND hwndCanvas)
{
    if (hwndCanvas) {
        InvalidateRect(hwndCanvas, NULL, FALSE);
    }
}

void CalendarCanvas_SetDpi(HWND hwndCanvas, UINT uDpi)
{
    CALENDAR_CANVAS* pC = (CALENDAR_CANVAS*)GetWindowLongPtrW(hwndCanvas, GWLP_USERDATA);
    RECT rc;
    if (!pC) return;
    pC->uDpi = uDpi > 0 ? uDpi : 96;
    GetClientRect(hwndCanvas, &rc);
    UpdateScrollBar(pC, rc.bottom - rc.top);
    InvalidateRect(hwndCanvas, NULL, FALSE);
}

void CalendarCanvas_GetAnchorDate(HWND hwndCanvas, SYSTEMTIME* pstOut)
{
    CALENDAR_CANVAS* pC = (CALENDAR_CANVAS*)GetWindowLongPtrW(hwndCanvas, GWLP_USERDATA);
    if (pC && pstOut) {
        *pstOut = pC->stAnchor;
    }
}

void CalendarCanvas_NavigateStep(HWND hwndCanvas, LONG lDirection)
{
    CALENDAR_CANVAS* pC = (CALENDAR_CANVAS*)GetWindowLongPtrW(hwndCanvas, GWLP_USERDATA);
    if (!pC) return;

    if (lDirection == 0) {
        GetLocalTime(&pC->stAnchor);
        pC->stSelected = pC->stAnchor;
    } else {
        switch (pC->dwViewMode) {
            case VIEW_DAY:
                Date_AddDays(&pC->stAnchor, lDirection);
                break;
            case VIEW_WEEK:
                Date_AddDays(&pC->stAnchor, lDirection * 7);
                break;
            case VIEW_MONTH:
                Date_AddMonths(&pC->stAnchor, lDirection);
                break;
            case VIEW_YEAR:
                Date_AddMonths(&pC->stAnchor, lDirection * 12);
                break;
            default:
                break;
        }
        pC->stSelected = pC->stAnchor;
    }
    InvalidateRect(hwndCanvas, NULL, FALSE);
}
