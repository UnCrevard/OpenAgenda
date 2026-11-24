#include "date_utils.h"

static const WCHAR* g_szMonthsFr[12] = {
    L"Janvier", L"F\u00E9vrier", L"Mars", L"Avril",
    L"Mai", L"Juin", L"Juillet", L"Ao\u00FBt",
    L"Septembre", L"Octobre", L"Novembre", L"D\u00E9cembre"
};

static const WCHAR* g_szWeekdaysShortMon[7] = {
    L"Lun", L"Mar", L"Mer", L"Jeu", L"Ven", L"Sam", L"Dim"
};

static const WCHAR* g_szWeekdaysFullSun[7] = {
    L"Dimanche", L"Lundi", L"Mardi", L"Mercredi",
    L"Jeudi", L"Vendredi", L"Samedi"
};

BOOL Date_IsLeapYear(DWORD dwYear)
{
    if ((dwYear % 400) == 0) {
        return TRUE;
    }
    if ((dwYear % 100) == 0) {
        return FALSE;
    }
    return ((dwYear % 4) == 0);
}

DWORD Date_DaysInMonth(DWORD dwYear, DWORD dwMonth)
{
    static const DWORD dwDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (dwMonth < 1 || dwMonth > 12) {
        return 30;
    }
    if (dwMonth == 2 && Date_IsLeapYear(dwYear)) {
        return 29;
    }
    return dwDays[dwMonth - 1];
}

DWORD Date_DayOfWeek(DWORD dwYear, DWORD dwMonth, DWORD dwDay)
{
    /* Sakamoto's algorithm returning 0=Sunday .. 6=Saturday */
    static const INT nOffsets[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    INT y = (INT)dwYear;
    if (dwMonth < 3) {
        y -= 1;
    }
    INT res = (y + y / 4 - y / 100 + y / 400 + nOffsets[dwMonth - 1] + (INT)dwDay) % 7;
    if (res < 0) {
        res += 7;
    }
    return (DWORD)res;
}

DWORD Date_MondayBasedDayOfWeek(const SYSTEMTIME* pst)
{
    DWORD dwSun = Date_DayOfWeek(pst->wYear, pst->wMonth, pst->wDay);
    return (dwSun + 6) % 7;
}

void Date_NormalizeTime(SYSTEMTIME* pst)
{
    pst->wDayOfWeek = (WORD)Date_DayOfWeek(pst->wYear, pst->wMonth, pst->wDay);
    pst->wMilliseconds = 0;
}

ULONGLONG Date_ToUInt64(const SYSTEMTIME* pst)
{
    FILETIME ft;
    ULARGE_INTEGER uli;
    SYSTEMTIME stClean = *pst;
    stClean.wDayOfWeek = 0;
    stClean.wMilliseconds = 0;
    if (!SystemTimeToFileTime(&stClean, &ft)) {
        return 0;
    }
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

BOOL Date_FromUInt64(ULONGLONG ullTime, SYSTEMTIME* pstOut)
{
    FILETIME ft;
    ULARGE_INTEGER uli;
    uli.QuadPart = ullTime;
    ft.dwLowDateTime = uli.LowPart;
    ft.dwHighDateTime = uli.HighPart;
    if (!FileTimeToSystemTime(&ft, pstOut)) {
        return FALSE;
    }
    pstOut->wDayOfWeek = (WORD)Date_DayOfWeek(pstOut->wYear, pstOut->wMonth, pstOut->wDay);
    return TRUE;
}

void Date_AddMinutes(SYSTEMTIME* pst, LONGLONG llMinutes)
{
    ULONGLONG ull = Date_ToUInt64(pst);
    LONGLONG llDelta = llMinutes * 60LL * 10000000LL;
    ULONGLONG ullNew = (ULONGLONG)((LONGLONG)ull + llDelta);
    Date_FromUInt64(ullNew, pst);
}

void Date_AddDays(SYSTEMTIME* pst, LONG lDays)
{
    Date_AddMinutes(pst, (LONGLONG)lDays * 1440LL);
}

void Date_AddMonths(SYSTEMTIME* pst, LONG lMonths)
{
    LONG lTotalMonths = ((LONG)pst->wYear * 12L) + ((LONG)pst->wMonth - 1L) + lMonths;
    LONG lNewYear;
    DWORD dwNewMonth;
    DWORD dwMaxDay;

    if (lTotalMonths < 0) {
        lTotalMonths = 0;
    }
    lNewYear = lTotalMonths / 12L;
    dwNewMonth = (DWORD)(lTotalMonths % 12L) + 1;
    if (lNewYear < 1601) {
        lNewYear = 1601;
    }
    pst->wYear = (WORD)lNewYear;
    pst->wMonth = (WORD)dwNewMonth;
    dwMaxDay = Date_DaysInMonth(pst->wYear, pst->wMonth);
    if (pst->wDay > dwMaxDay) {
        pst->wDay = (WORD)dwMaxDay;
    }
    Date_NormalizeTime(pst);
}

LONG Date_CompareDate(const SYSTEMTIME* pstA, const SYSTEMTIME* pstB)
{
    if (pstA->wYear != pstB->wYear) {
        return (pstA->wYear < pstB->wYear) ? -1 : 1;
    }
    if (pstA->wMonth != pstB->wMonth) {
        return (pstA->wMonth < pstB->wMonth) ? -1 : 1;
    }
    if (pstA->wDay != pstB->wDay) {
        return (pstA->wDay < pstB->wDay) ? -1 : 1;
    }
    return 0;
}

LONG Date_CompareDateTime(const SYSTEMTIME* pstA, const SYSTEMTIME* pstB)
{
    LONG lDateCmp = Date_CompareDate(pstA, pstB);
    if (lDateCmp != 0) {
        return lDateCmp;
    }
    if (pstA->wHour != pstB->wHour) {
        return (pstA->wHour < pstB->wHour) ? -1 : 1;
    }
    if (pstA->wMinute != pstB->wMinute) {
        return (pstA->wMinute < pstB->wMinute) ? -1 : 1;
    }
    if (pstA->wSecond != pstB->wSecond) {
        return (pstA->wSecond < pstB->wSecond) ? -1 : 1;
    }
    return 0;
}

LONGLONG Date_DiffDays(const SYSTEMTIME* pstStart, const SYSTEMTIME* pstEnd)
{
    SYSTEMTIME stA = *pstStart;
    SYSTEMTIME stB = *pstEnd;
    ULONGLONG ullA;
    ULONGLONG ullB;

    stA.wHour = 12; stA.wMinute = 0; stA.wSecond = 0; stA.wMilliseconds = 0;
    stB.wHour = 12; stB.wMinute = 0; stB.wSecond = 0; stB.wMilliseconds = 0;

    ullA = Date_ToUInt64(&stA);
    ullB = Date_ToUInt64(&stB);
    return ((LONGLONG)ullB - (LONGLONG)ullA) / (86400LL * 10000000LL);
}

void Date_GetWeekStartMonday(const SYSTEMTIME* pst, SYSTEMTIME* pstOut)
{
    DWORD dwMonIdx = Date_MondayBasedDayOfWeek(pst);
    *pstOut = *pst;
    pstOut->wHour = 0;
    pstOut->wMinute = 0;
    pstOut->wSecond = 0;
    pstOut->wMilliseconds = 0;
    if (dwMonIdx > 0) {
        Date_AddDays(pstOut, -(LONG)dwMonIdx);
    }
    Date_NormalizeTime(pstOut);
}

BOOL Date_GetNthWeekdayOfMonth(DWORD dwYear, DWORD dwMonth, LONG lOrdinal, DWORD dwWeekday, SYSTEMTIME* pstOut)
{
    DWORD dwDays = Date_DaysInMonth(dwYear, dwMonth);
    DWORD dwFirstDow = Date_DayOfWeek(dwYear, dwMonth, 1);
    DWORD dwDay;

    if (dwWeekday > 6) {
        return FALSE;
    }

    if (lOrdinal == -1) {
        /* Last occurrence of dwWeekday in the month */
        DWORD dwLastDow = Date_DayOfWeek(dwYear, dwMonth, dwDays);
        DWORD dwBack = (dwLastDow + 7 - dwWeekday) % 7;
        dwDay = dwDays - dwBack;
    } else if (lOrdinal >= 1 && lOrdinal <= 5) {
        DWORD dwOffset = (dwWeekday + 7 - dwFirstDow) % 7;
        dwDay = 1 + dwOffset + (DWORD)(lOrdinal - 1) * 7;
        if (dwDay > dwDays) {
            return FALSE;
        }
    } else {
        return FALSE;
    }

    ZeroMemory(pstOut, sizeof(*pstOut));
    pstOut->wYear = (WORD)dwYear;
    pstOut->wMonth = (WORD)dwMonth;
    pstOut->wDay = (WORD)dwDay;
    Date_NormalizeTime(pstOut);
    return TRUE;
}

void Date_GetMonthName(DWORD dwMonth, WCHAR* pszOut, DWORD cchOut)
{
    if (dwMonth >= 1 && dwMonth <= 12) {
        wcsncpy(pszOut, g_szMonthsFr[dwMonth - 1], cchOut - 1);
        pszOut[cchOut - 1] = L'\0';
    } else if (cchOut > 0) {
        pszOut[0] = L'\0';
    }
}

void Date_GetWeekdayShort(DWORD dwMondayIndex, WCHAR* pszOut, DWORD cchOut)
{
    if (dwMondayIndex < 7) {
        wcsncpy(pszOut, g_szWeekdaysShortMon[dwMondayIndex], cchOut - 1);
        pszOut[cchOut - 1] = L'\0';
    } else if (cchOut > 0) {
        pszOut[0] = L'\0';
    }
}

void Date_GetWeekdayFull(DWORD dwSystemDayOfWeek, WCHAR* pszOut, DWORD cchOut)
{
    if (dwSystemDayOfWeek < 7) {
        wcsncpy(pszOut, g_szWeekdaysFullSun[dwSystemDayOfWeek], cchOut - 1);
        pszOut[cchOut - 1] = L'\0';
    } else if (cchOut > 0) {
        pszOut[0] = L'\0';
    }
}

void Date_FormatFrenchDate(const SYSTEMTIME* pst, WCHAR* pszOut, DWORD cchOut)
{
    WCHAR szDayName[32];
    WCHAR szMonthName[32];
    DWORD dwDow = Date_DayOfWeek(pst->wYear, pst->wMonth, pst->wDay);

    Date_GetWeekdayFull(dwDow, szDayName, 32);
    Date_GetMonthName(pst->wMonth, szMonthName, 32);
    _snwprintf(pszOut, cchOut, L"%s %u %s %u", szDayName, (UINT)pst->wDay, szMonthName, (UINT)pst->wYear);
    pszOut[cchOut - 1] = L'\0';
}

void Date_FormatMonthYear(const SYSTEMTIME* pst, WCHAR* pszOut, DWORD cchOut)
{
    WCHAR szMonthName[32];
    Date_GetMonthName(pst->wMonth, szMonthName, 32);
    _snwprintf(pszOut, cchOut, L"%s %u", szMonthName, (UINT)pst->wYear);
    pszOut[cchOut - 1] = L'\0';
}

void Date_FormatIsoDateTime(const SYSTEMTIME* pst, WCHAR* pszOut, DWORD cchOut)
{
    _snwprintf(
        pszOut,
        cchOut,
        L"%04u-%02u-%02uT%02u:%02u:%02u",
        (UINT)pst->wYear,
        (UINT)pst->wMonth,
        (UINT)pst->wDay,
        (UINT)pst->wHour,
        (UINT)pst->wMinute,
        (UINT)pst->wSecond
    );
    pszOut[cchOut - 1] = L'\0';
}

BOOL Date_ParseIsoDateTime(const WCHAR* pszIso, SYSTEMTIME* pstOut)
{
    UINT y = 0, m = 0, d = 0, hh = 0, mm = 0, ss = 0;
    INT nParsed;

    if (!pszIso || !pstOut) {
        return FALSE;
    }
    nParsed = swscanf(pszIso, L"%u-%u-%uT%u:%u:%u", &y, &m, &d, &hh, &mm, &ss);
    if (nParsed < 3) {
        return FALSE;
    }
    if (y < 1601 || m < 1 || m > 12 || d < 1 || d > 31) {
        return FALSE;
    }
    ZeroMemory(pstOut, sizeof(*pstOut));
    pstOut->wYear = (WORD)y;
    pstOut->wMonth = (WORD)m;
    pstOut->wDay = (WORD)d;
    pstOut->wHour = (WORD)(hh <= 23 ? hh : 0);
    pstOut->wMinute = (WORD)(mm <= 59 ? mm : 0);
    pstOut->wSecond = (WORD)(ss <= 59 ? ss : 0);
    Date_NormalizeTime(pstOut);
    return TRUE;
}
