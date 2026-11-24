#include "recurrence.h"
#include "date_utils.h"

void EventList_Init(EVENT_LIST* pList)
{
    pList->pItems = NULL;
    pList->dwCount = 0;
    pList->dwCapacity = 0;
}

void EventList_Free(EVENT_LIST* pList)
{
    if (pList->pItems) {
        free(pList->pItems);
        pList->pItems = NULL;
    }
    pList->dwCount = 0;
    pList->dwCapacity = 0;
}

BOOL EventList_Add(EVENT_LIST* pList, const CALENDAR_EVENT* pEvent)
{
    if (pList->dwCount >= pList->dwCapacity) {
        DWORD dwNewCap = (pList->dwCapacity == 0) ? 16 : pList->dwCapacity * 2;
        CALENDAR_EVENT* pNew = (CALENDAR_EVENT*)realloc(pList->pItems, dwNewCap * sizeof(CALENDAR_EVENT));
        if (!pNew) {
            return FALSE;
        }
        pList->pItems = pNew;
        pList->dwCapacity = dwNewCap;
    }
    pList->pItems[pList->dwCount++] = *pEvent;
    return TRUE;
}

BOOL EventList_Update(EVENT_LIST* pList, const CALENDAR_EVENT* pEvent)
{
    DWORD i;
    for (i = 0; i < pList->dwCount; ++i) {
        if (pList->pItems[i].dwId == pEvent->dwId) {
            pList->pItems[i] = *pEvent;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL EventList_Remove(EVENT_LIST* pList, DWORD dwEventId)
{
    DWORD i;
    for (i = 0; i < pList->dwCount; ++i) {
        if (pList->pItems[i].dwId == dwEventId) {
            if (i + 1 < pList->dwCount) {
                memmove(&pList->pItems[i], &pList->pItems[i + 1], (pList->dwCount - i - 1) * sizeof(CALENDAR_EVENT));
            }
            pList->dwCount--;
            return TRUE;
        }
    }
    return FALSE;
}

CALENDAR_EVENT* EventList_FindById(EVENT_LIST* pList, DWORD dwEventId)
{
    DWORD i;
    for (i = 0; i < pList->dwCount; ++i) {
        if (pList->pItems[i].dwId == dwEventId) {
            return &pList->pItems[i];
        }
    }
    return NULL;
}

/* Case-insensitive Unicode substring check */
static BOOL ContainsIgnoreCase(const WCHAR* pszHaystack, const WCHAR* pszNeedle)
{
    DWORD dwHayLen;
    DWORD dwNeedleLen;
    DWORD i;

    if (!pszNeedle || pszNeedle[0] == L'\0') {
        return TRUE;
    }
    if (!pszHaystack || pszHaystack[0] == L'\0') {
        return FALSE;
    }

    dwHayLen = (DWORD)wcslen(pszHaystack);
    dwNeedleLen = (DWORD)wcslen(pszNeedle);
    if (dwNeedleLen > dwHayLen) {
        return FALSE;
    }

    for (i = 0; i <= dwHayLen - dwNeedleLen; ++i) {
        if (CompareStringOrdinal(&pszHaystack[i], (INT)dwNeedleLen, pszNeedle, (INT)dwNeedleLen, TRUE) == CSTR_EQUAL) {
            return TRUE;
        }
    }
    return FALSE;
}

typedef struct _OCC_ARRAY {
    EVENT_OCCURRENCE* pItems;
    DWORD             dwCount;
    DWORD             dwCapacity;
} OCC_ARRAY;

static BOOL OccArray_Push(
    OCC_ARRAY* pArr,
    DWORD dwEventIndex,
    const CALENDAR_EVENT* pEvent,
    const SYSTEMTIME* pstOccStart,
    ULONGLONG ullRangeStart,
    ULONGLONG ullRangeEnd
)
{
    EVENT_OCCURRENCE occ;
    SYSTEMTIME stEnd;
    ULONGLONG ullStart;
    ULONGLONG ullEnd;
    DWORD dwDur = pEvent->bAllDay ? 1440 : (pEvent->dwDurationMinutes > 0 ? pEvent->dwDurationMinutes : 30);

    /* Enforce optional recurrence end date (date comparison inclusive) */
    if (pEvent->dwRecurrenceType != REC_NONE && pEvent->bHasEndDate) {
        if (Date_CompareDate(pstOccStart, &pEvent->stEndDate) > 0) {
            return TRUE;
        }
    }

    stEnd = *pstOccStart;
    Date_AddMinutes(&stEnd, (LONGLONG)dwDur);

    ullStart = Date_ToUInt64(pstOccStart);
    ullEnd = Date_ToUInt64(&stEnd);

    /* Overlap test against [ullRangeStart, ullRangeEnd] */
    if (ullEnd <= ullRangeStart || ullStart > ullRangeEnd) {
        return TRUE;
    }

    if (pArr->dwCount >= pArr->dwCapacity) {
        DWORD dwNewCap = (pArr->dwCapacity == 0) ? 32 : pArr->dwCapacity * 2;
        EVENT_OCCURRENCE* pNew = (EVENT_OCCURRENCE*)realloc(pArr->pItems, dwNewCap * sizeof(EVENT_OCCURRENCE));
        if (!pNew) {
            return FALSE;
        }
        pArr->pItems = pNew;
        pArr->dwCapacity = dwNewCap;
    }

    occ.dwEventIndex = dwEventIndex;
    occ.dwEventId = pEvent->dwId;
    occ.dwEventVersion = pEvent->dwVersion;
    occ.stStart = *pstOccStart;
    occ.stEnd = stEnd;
    occ.ullStartFt = ullStart;
    occ.ullEndFt = ullEnd;

    pArr->pItems[pArr->dwCount++] = occ;
    return TRUE;
}

static INT __cdecl CompareOccurrences(const void* a, const void* b)
{
    const EVENT_OCCURRENCE* pA = (const EVENT_OCCURRENCE*)a;
    const EVENT_OCCURRENCE* pB = (const EVENT_OCCURRENCE*)b;
    if (pA->ullStartFt < pB->ullStartFt) return -1;
    if (pA->ullStartFt > pB->ullStartFt) return 1;
    if (pA->dwEventId < pB->dwEventId) return -1;
    if (pA->dwEventId > pB->dwEventId) return 1;
    return 0;
}

BOOL Recurrence_ExpandRange(
    const EVENT_LIST* pEvents,
    const SYSTEMTIME* pstRangeStart,
    const SYSTEMTIME* pstRangeEnd,
    const WCHAR* pszSearchFilter,
    EVENT_OCCURRENCE** ppOutOccurrences,
    DWORD* pdwOutCount
)
{
    OCC_ARRAY arr;
    ULONGLONG ullRangeStart;
    ULONGLONG ullRangeEnd;
    DWORD i;

    *ppOutOccurrences = NULL;
    *pdwOutCount = 0;

    if (!pEvents || pEvents->dwCount == 0) {
        return TRUE;
    }

    arr.pItems = NULL;
    arr.dwCount = 0;
    arr.dwCapacity = 0;

    ullRangeStart = Date_ToUInt64(pstRangeStart);
    ullRangeEnd = Date_ToUInt64(pstRangeEnd);

    for (i = 0; i < pEvents->dwCount; ++i) {
        const CALENDAR_EVENT* pEv = &pEvents->pItems[i];
        DWORD dwInterval = (pEv->dwRecurrenceInterval >= 1) ? pEv->dwRecurrenceInterval : 1;

        if (pszSearchFilter && pszSearchFilter[0] != L'\0') {
            if (!ContainsIgnoreCase(pEv->szTitle, pszSearchFilter) &&
                !ContainsIgnoreCase(pEv->szDescription, pszSearchFilter)) {
                continue;
            }
        }

        if (pEv->dwRecurrenceType == REC_NONE) {
            if (!OccArray_Push(&arr, i, pEv, &pEv->stStart, ullRangeStart, ullRangeEnd)) {
                free(arr.pItems);
                return FALSE;
            }
        } else if (pEv->dwRecurrenceType == REC_DAILY) {
            LONGLONG llDiff = Date_DiffDays(&pEv->stStart, pstRangeStart);
            LONGLONG llMargin = (LONGLONG)(pEv->dwDurationMinutes / 1440) + 2;
            LONGLONG llStep = 0;
            SYSTEMTIME stCur;

            if (llDiff > llMargin) {
                llStep = ((llDiff - llMargin) / (LONGLONG)dwInterval) * (LONGLONG)dwInterval;
            }
            stCur = pEv->stStart;
            if (llStep > 0) {
                Date_AddDays(&stCur, (LONG)llStep);
            }

            while (Date_CompareDateTime(&stCur, pstRangeEnd) <= 0) {
                if (pEv->bHasEndDate && Date_CompareDate(&stCur, &pEv->stEndDate) > 0) {
                    break;
                }
                if (!OccArray_Push(&arr, i, pEv, &stCur, ullRangeStart, ullRangeEnd)) {
                    free(arr.pItems);
                    return FALSE;
                }
                Date_AddDays(&stCur, (LONG)dwInterval);
            }
        } else if (pEv->dwRecurrenceType == REC_WEEKLY) {
            SYSTEMTIME stBaseMon;
            SYSTEMTIME stCurMon;
            LONGLONG llWeeksDiff;
            DWORD dwMask = pEv->dwWeekdaysMask;

            if (dwMask == 0) {
                dwMask = (1U << Date_MondayBasedDayOfWeek(&pEv->stStart));
            }

            Date_GetWeekStartMonday(&pEv->stStart, &stBaseMon);
            llWeeksDiff = Date_DiffDays(&stBaseMon, pstRangeStart) / 7;
            stCurMon = stBaseMon;
            if (llWeeksDiff > 2) {
                LONGLONG llWeekStep = ((llWeeksDiff - 2) / (LONGLONG)dwInterval) * (LONGLONG)dwInterval;
                Date_AddDays(&stCurMon, (LONG)(llWeekStep * 7));
            }

            while (Date_CompareDate(&stCurMon, pstRangeEnd) <= 0) {
                DWORD d;
                if (pEv->bHasEndDate && Date_CompareDate(&stCurMon, &pEv->stEndDate) > 0) {
                    break;
                }
                for (d = 0; d < 7; ++d) {
                    if (dwMask & (1U << d)) {
                        SYSTEMTIME stOcc = stCurMon;
                        Date_AddDays(&stOcc, (LONG)d);
                        stOcc.wHour = pEv->stStart.wHour;
                        stOcc.wMinute = pEv->stStart.wMinute;
                        stOcc.wSecond = pEv->stStart.wSecond;
                        Date_NormalizeTime(&stOcc);

                        if (Date_CompareDate(&stOcc, &pEv->stStart) >= 0) {
                            if (!OccArray_Push(&arr, i, pEv, &stOcc, ullRangeStart, ullRangeEnd)) {
                                free(arr.pItems);
                                return FALSE;
                            }
                        }
                    }
                }
                Date_AddDays(&stCurMon, (LONG)(dwInterval * 7));
            }
        } else if (pEv->dwRecurrenceType == REC_MONTHLY_DATE) {
            LONG lMonthsDiff = ((LONG)pstRangeStart->wYear - (LONG)pEv->stStart.wYear) * 12L
                             + ((LONG)pstRangeStart->wMonth - (LONG)pEv->stStart.wMonth) - 1L;
            LONG lStep = 0;
            SYSTEMTIME stMonthIter;

            if (lMonthsDiff > 0) {
                lStep = (lMonthsDiff / (LONG)dwInterval) * (LONG)dwInterval;
            }
            stMonthIter = pEv->stStart;
            stMonthIter.wDay = 1;
            Date_AddMonths(&stMonthIter, lStep);

            while (Date_CompareDate(&stMonthIter, pstRangeEnd) <= 0) {
                SYSTEMTIME stOcc = stMonthIter;
                DWORD dwMaxD = Date_DaysInMonth(stOcc.wYear, stOcc.wMonth);
                stOcc.wDay = (WORD)((pEv->stStart.wDay <= dwMaxD) ? pEv->stStart.wDay : dwMaxD);
                stOcc.wHour = pEv->stStart.wHour;
                stOcc.wMinute = pEv->stStart.wMinute;
                stOcc.wSecond = pEv->stStart.wSecond;
                Date_NormalizeTime(&stOcc);

                if (pEv->bHasEndDate && Date_CompareDate(&stOcc, &pEv->stEndDate) > 0) {
                    break;
                }
                if (Date_CompareDate(&stOcc, &pEv->stStart) >= 0) {
                    if (!OccArray_Push(&arr, i, pEv, &stOcc, ullRangeStart, ullRangeEnd)) {
                        free(arr.pItems);
                        return FALSE;
                    }
                }
                Date_AddMonths(&stMonthIter, (LONG)dwInterval);
            }
        } else if (pEv->dwRecurrenceType == REC_MONTHLY_NTH) {
            LONG lMonthsDiff = ((LONG)pstRangeStart->wYear - (LONG)pEv->stStart.wYear) * 12L
                             + ((LONG)pstRangeStart->wMonth - (LONG)pEv->stStart.wMonth) - 1L;
            LONG lStep = 0;
            SYSTEMTIME stMonthIter;

            if (lMonthsDiff > 0) {
                lStep = (lMonthsDiff / (LONG)dwInterval) * (LONG)dwInterval;
            }
            stMonthIter = pEv->stStart;
            stMonthIter.wDay = 1;
            Date_AddMonths(&stMonthIter, lStep);

            while (Date_CompareDate(&stMonthIter, pstRangeEnd) <= 0) {
                SYSTEMTIME stOcc;
                if (Date_GetNthWeekdayOfMonth(stMonthIter.wYear, stMonthIter.wMonth,
                                              pEv->lNthOrdinal, pEv->dwNthWeekday, &stOcc)) {
                    stOcc.wHour = pEv->stStart.wHour;
                    stOcc.wMinute = pEv->stStart.wMinute;
                    stOcc.wSecond = pEv->stStart.wSecond;
                    Date_NormalizeTime(&stOcc);

                    if (pEv->bHasEndDate && Date_CompareDate(&stOcc, &pEv->stEndDate) > 0) {
                        break;
                    }
                    if (Date_CompareDate(&stOcc, &pEv->stStart) >= 0) {
                        if (!OccArray_Push(&arr, i, pEv, &stOcc, ullRangeStart, ullRangeEnd)) {
                            free(arr.pItems);
                            return FALSE;
                        }
                    }
                }
                Date_AddMonths(&stMonthIter, (LONG)dwInterval);
            }
        } else if (pEv->dwRecurrenceType == REC_YEARLY || pEv->dwRecurrenceType == REC_BIRTHDAY) {
            DWORD dwYearStep = (pEv->dwRecurrenceType == REC_BIRTHDAY) ? 1 : dwInterval;
            LONG lYearsDiff = (LONG)pstRangeStart->wYear - (LONG)pEv->stStart.wYear - 1L;
            LONG lStep = 0;
            SYSTEMTIME stYearIter;

            if (lYearsDiff > 0) {
                lStep = (lYearsDiff / (LONG)dwYearStep) * (LONG)dwYearStep;
            }
            stYearIter = pEv->stStart;
            stYearIter.wDay = 1;
            Date_AddMonths(&stYearIter, lStep * 12L);

            while (Date_CompareDate(&stYearIter, pstRangeEnd) <= 0) {
                SYSTEMTIME stOcc = stYearIter;
                DWORD dwMaxD = Date_DaysInMonth(stOcc.wYear, stOcc.wMonth);
                stOcc.wDay = (WORD)((pEv->stStart.wDay <= dwMaxD) ? pEv->stStart.wDay : dwMaxD);
                stOcc.wHour = pEv->stStart.wHour;
                stOcc.wMinute = pEv->stStart.wMinute;
                stOcc.wSecond = pEv->stStart.wSecond;
                Date_NormalizeTime(&stOcc);

                if (pEv->bHasEndDate && Date_CompareDate(&stOcc, &pEv->stEndDate) > 0) {
                    break;
                }
                if (Date_CompareDate(&stOcc, &pEv->stStart) >= 0) {
                    if (!OccArray_Push(&arr, i, pEv, &stOcc, ullRangeStart, ullRangeEnd)) {
                        free(arr.pItems);
                        return FALSE;
                    }
                }
                Date_AddMonths(&stYearIter, (LONG)dwYearStep * 12L);
            }
        }
    }

    if (arr.dwCount > 1) {
        qsort(arr.pItems, arr.dwCount, sizeof(EVENT_OCCURRENCE), CompareOccurrences);
    }

    *ppOutOccurrences = arr.pItems;
    *pdwOutCount = arr.dwCount;
    return TRUE;
}

void Recurrence_FormatSummary(const CALENDAR_EVENT* pEvent, WCHAR* pszOut, DWORD cchOut)
{
    WCHAR szWeekday[32];
    const WCHAR* pszOrd = L"1er";

    switch (pEvent->dwRecurrenceType) {
        case REC_NONE:
            wcsncpy(pszOut, L"\u00C9v\u00E9nement unique", cchOut - 1);
            break;
        case REC_DAILY:
            if (pEvent->dwRecurrenceInterval <= 1) {
                wcsncpy(pszOut, L"Tous les jours", cchOut - 1);
            } else {
                _snwprintf(pszOut, cchOut, L"Tous les %u jours", (UINT)pEvent->dwRecurrenceInterval);
            }
            break;
        case REC_WEEKLY:
            if (pEvent->dwRecurrenceInterval <= 1) {
                wcsncpy(pszOut, L"Toutes les semaines", cchOut - 1);
            } else {
                _snwprintf(pszOut, cchOut, L"Toutes les %u semaines", (UINT)pEvent->dwRecurrenceInterval);
            }
            break;
        case REC_MONTHLY_DATE:
            if (pEvent->dwRecurrenceInterval <= 1) {
                _snwprintf(pszOut, cchOut, L"Tous les mois le %u", (UINT)pEvent->stStart.wDay);
            } else {
                _snwprintf(pszOut, cchOut, L"Tous les %u mois le %u", (UINT)pEvent->dwRecurrenceInterval, (UINT)pEvent->stStart.wDay);
            }
            break;
        case REC_MONTHLY_NTH:
            Date_GetWeekdayFull(pEvent->dwNthWeekday, szWeekday, 32);
            if (pEvent->lNthOrdinal == 1) pszOrd = L"Premier";
            else if (pEvent->lNthOrdinal == 2) pszOrd = L"Deuxi\u00E8me";
            else if (pEvent->lNthOrdinal == 3) pszOrd = L"Troisi\u00E8me";
            else if (pEvent->lNthOrdinal == 4) pszOrd = L"Quatri\u00E8me";
            else pszOrd = L"Dernier";
            _snwprintf(pszOut, cchOut, L"%s %s du mois", pszOrd, szWeekday);
            break;
        case REC_YEARLY:
            if (pEvent->dwRecurrenceInterval <= 1) {
                wcsncpy(pszOut, L"Tous les ans", cchOut - 1);
            } else {
                _snwprintf(pszOut, cchOut, L"Tous les %u ans", (UINT)pEvent->dwRecurrenceInterval);
            }
            break;
        case REC_BIRTHDAY:
            wcsncpy(pszOut, L"Anniversaire (tous les ans)", cchOut - 1);
            break;
        default:
            pszOut[0] = L'\0';
            break;
    }
    pszOut[cchOut - 1] = L'\0';
}

LONG Recurrence_GetBirthdayAge(const CALENDAR_EVENT* pEvent, const SYSTEMTIME* pstOccurrence)
{
    if (!pEvent || !pstOccurrence || pEvent->dwRecurrenceType != REC_BIRTHDAY) {
        return -1;
    }
    if (pstOccurrence->wYear < pEvent->stStart.wYear) {
        return -1;
    }
    return (LONG)pstOccurrence->wYear - (LONG)pEvent->stStart.wYear;
}

void Recurrence_FormatDisplayTitle(
    const CALENDAR_EVENT* pEvent,
    const SYSTEMTIME* pstOccurrence,
    WCHAR* pszOut,
    DWORD cchOut
)
{
    LONG lAge;
    if (!pszOut || cchOut == 0) {
        return;
    }
    if (!pEvent) {
        pszOut[0] = L'\0';
        return;
    }

    lAge = Recurrence_GetBirthdayAge(pEvent, pstOccurrence);
    if (lAge > 0) {
        _snwprintf(pszOut, cchOut, L"%s (%ld %s)",
                   pEvent->szTitle, (long)lAge, (lAge == 1) ? L"an" : L"ans");
    } else if (lAge == 0) {
        _snwprintf(pszOut, cchOut, L"%s (Naissance)", pEvent->szTitle);
    } else {
        wcsncpy(pszOut, pEvent->szTitle, cchOut - 1);
    }
    pszOut[cchOut - 1] = L'\0';
}
