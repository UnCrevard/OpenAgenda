#ifndef AGENDA_RECURRENCE_H
#define AGENDA_RECURRENCE_H

#include "types.h"

/* Event list memory helpers */
void EventList_Init(EVENT_LIST* pList);
void EventList_Free(EVENT_LIST* pList);
BOOL EventList_Add(EVENT_LIST* pList, const CALENDAR_EVENT* pEvent);
BOOL EventList_Update(EVENT_LIST* pList, const CALENDAR_EVENT* pEvent);
BOOL EventList_Remove(EVENT_LIST* pList, DWORD dwEventId);
CALENDAR_EVENT* EventList_FindById(EVENT_LIST* pList, DWORD dwEventId);

/* Expands all matching event occurrences inside [pstRangeStart, pstRangeEnd] */
BOOL Recurrence_ExpandRange(
    const EVENT_LIST* pEvents,
    const SYSTEMTIME* pstRangeStart,
    const SYSTEMTIME* pstRangeEnd,
    const WCHAR* pszSearchFilter,
    EVENT_OCCURRENCE** ppOutOccurrences,
    DWORD* pdwOutCount
);

/* Formats a human-readable French description of the recurrence rule */
void Recurrence_FormatSummary(const CALENDAR_EVENT* pEvent, WCHAR* pszOut, DWORD cchOut);

/* Computes age in years for a birthday occurrence (-1 if not a birthday or before birth year) */
LONG Recurrence_GetBirthdayAge(const CALENDAR_EVENT* pEvent, const SYSTEMTIME* pstOccurrence);

/* Formats event display title, appending age for REC_BIRTHDAY occurrences */
void Recurrence_FormatDisplayTitle(
    const CALENDAR_EVENT* pEvent,
    const SYSTEMTIME* pstOccurrence,
    WCHAR* pszOut,
    DWORD cchOut
);

#endif /* AGENDA_RECURRENCE_H */
