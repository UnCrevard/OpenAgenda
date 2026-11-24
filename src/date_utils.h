#ifndef AGENDA_DATE_UTILS_H
#define AGENDA_DATE_UTILS_H

#include "types.h"

/* Leap year and month length helpers */
BOOL Date_IsLeapYear(DWORD dwYear);
DWORD Date_DaysInMonth(DWORD dwYear, DWORD dwMonth);

/* Weekday computation (0=Sunday..6=Saturday, and 0=Monday..6=Sunday) */
DWORD Date_DayOfWeek(DWORD dwYear, DWORD dwMonth, DWORD dwDay);
DWORD Date_MondayBasedDayOfWeek(const SYSTEMTIME* pst);

/* Date and time arithmetic */
void Date_NormalizeTime(SYSTEMTIME* pst);
void Date_AddDays(SYSTEMTIME* pst, LONG lDays);
void Date_AddMonths(SYSTEMTIME* pst, LONG lMonths);
void Date_AddMinutes(SYSTEMTIME* pst, LONGLONG llMinutes);

/* Comparisons */
LONG Date_CompareDate(const SYSTEMTIME* pstA, const SYSTEMTIME* pstB);
LONG Date_CompareDateTime(const SYSTEMTIME* pstA, const SYSTEMTIME* pstB);

/* FILETIME 100-ns conversion helpers */
ULONGLONG Date_ToUInt64(const SYSTEMTIME* pst);
BOOL Date_FromUInt64(ULONGLONG ullTime, SYSTEMTIME* pstOut);
LONGLONG Date_DiffDays(const SYSTEMTIME* pstStart, const SYSTEMTIME* pstEnd);

/* Calendar navigation helpers */
void Date_GetWeekStartMonday(const SYSTEMTIME* pst, SYSTEMTIME* pstOut);
BOOL Date_GetNthWeekdayOfMonth(DWORD dwYear, DWORD dwMonth, LONG lOrdinal, DWORD dwWeekday, SYSTEMTIME* pstOut);

/* French localized formatting */
void Date_GetMonthName(DWORD dwMonth, WCHAR* pszOut, DWORD cchOut);
void Date_GetWeekdayShort(DWORD dwMondayIndex, WCHAR* pszOut, DWORD cchOut);
void Date_GetWeekdayFull(DWORD dwSystemDayOfWeek, WCHAR* pszOut, DWORD cchOut);
void Date_FormatFrenchDate(const SYSTEMTIME* pst, WCHAR* pszOut, DWORD cchOut);
void Date_FormatMonthYear(const SYSTEMTIME* pst, WCHAR* pszOut, DWORD cchOut);
void Date_FormatIsoDateTime(const SYSTEMTIME* pst, WCHAR* pszOut, DWORD cchOut);
BOOL Date_ParseIsoDateTime(const WCHAR* pszIso, SYSTEMTIME* pstOut);

#endif /* AGENDA_DATE_UTILS_H */
