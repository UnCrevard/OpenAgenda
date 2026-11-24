#ifndef AGENDA_TYPES_H
#define AGENDA_TYPES_H

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <mmsystem.h>
#include <shlwapi.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Application constants */
#define AGENDA_APP_NAME          L"Agenda"
#define AGENDA_APP_USER_MODEL_ID L"Agenda"
#define AGENDA_WND_CLASS         L"AgendaWin32MainWindowClass"
#define AGENDA_DLG_EVENT_CLASS  L"AgendaWin32EventDialogClass"
#define AGENDA_DLG_CFG_CLASS    L"AgendaWin32SettingsDialogClass"
#define AGENDA_MUTEX_NAME       L"Local\\AgendaWin32SingleInstanceMutex"
#define AGENDA_INI_FILENAME     L"agenda.ini"
#define AGENDA_JSON_FILENAME    L"events.json"
#define AGENDA_VERSIONS_SUBDIR  L"versions"
#define AGENDA_LNK_FILENAME      L"AgendaWin32.lnk"
#define IDI_APP_ICON             101

#define WM_APP_TRAYICON         (WM_APP + 100)
#define WM_APP_SHOW_MAIN        (WM_APP + 101)
#define TIMER_ID_NOTIFY         1001
#define TIMER_INTERVAL_MS       15000

#define MAX_EVENT_TITLE         128
#define MAX_EVENT_DESC          512
#define EVENT_COLOR_COUNT       8
#define AGENDA_DEFAULT_FONT_NAME L"Segoe UI"
#define AGENDA_DEFAULT_FONT_PT   9

/* Calendar display modes */
typedef enum _CALENDAR_VIEW_MODE {
    VIEW_DAY   = 0,
    VIEW_WEEK  = 1,
    VIEW_MONTH = 2,
    VIEW_YEAR  = 3
} CALENDAR_VIEW_MODE;

/* Event recurrence types */
typedef enum _RECURRENCE_TYPE {
    REC_NONE         = 0, /* Single occurrence */
    REC_DAILY        = 1, /* Every N days */
    REC_WEEKLY       = 2, /* Every N weeks on selected weekdays */
    REC_MONTHLY_DATE = 3, /* Every N months on fixed day of month */
    REC_MONTHLY_NTH  = 4, /* Every N months on Nth weekday (e.g. 1st Monday) */
    REC_YEARLY       = 5, /* Every N years */
    REC_BIRTHDAY     = 6  /* Annual birthday showing age in calendar and notifications */
} RECURRENCE_TYPE;

/* Weekday bitmask flags (Monday = bit 0 .. Sunday = bit 6) */
#define WEEKDAY_MON_BIT  (1U << 0)
#define WEEKDAY_TUE_BIT  (1U << 1)
#define WEEKDAY_WED_BIT  (1U << 2)
#define WEEKDAY_THU_BIT  (1U << 3)
#define WEEKDAY_FRI_BIT  (1U << 4)
#define WEEKDAY_SAT_BIT  (1U << 5)
#define WEEKDAY_SUN_BIT  (1U << 6)

/* Notification timing mode */
typedef enum _NOTIFY_MODE {
    NOTIFY_NONE      = 0,
    NOTIFY_IMMEDIATE = 1, /* At exact event start time */
    NOTIFY_BEFORE    = 2  /* Specific number of minutes before start */
} NOTIFY_MODE;

/* Core event record */
typedef struct _CALENDAR_EVENT {
    DWORD      dwId;
    DWORD      dwVersion;
    WCHAR      szTitle[MAX_EVENT_TITLE];
    WCHAR      szDescription[MAX_EVENT_DESC];
    SYSTEMTIME stStart;
    DWORD      dwDurationMinutes;
    BOOL       bAllDay;
    DWORD      dwRecurrenceType;      /* RECURRENCE_TYPE */
    DWORD      dwRecurrenceInterval;  /* >= 1 */
    DWORD      dwWeekdaysMask;        /* WEEKDAY_*_BIT */
    LONG       lNthOrdinal;           /* 1..4 or -1 for last */
    DWORD      dwNthWeekday;          /* 0=Sunday..6=Saturday (SYSTEMTIME.wDayOfWeek) */
    BOOL       bHasEndDate;
    SYSTEMTIME stEndDate;
    DWORD      dwNotifyMode;          /* NOTIFY_MODE */
    DWORD      dwNotifyMinutesBefore; /* Used when dwNotifyMode == NOTIFY_BEFORE */
    DWORD      dwColorIndex;          /* 0 .. EVENT_COLOR_COUNT-1 */
} CALENDAR_EVENT;

/* Expanded occurrence instance for rendering and notifications */
typedef struct _EVENT_OCCURRENCE {
    DWORD          dwEventIndex;
    DWORD          dwEventId;
    DWORD          dwEventVersion;
    SYSTEMTIME     stStart;
    SYSTEMTIME     stEnd;
    ULONGLONG      ullStartFt;
    ULONGLONG      ullEndFt;
} EVENT_OCCURRENCE;

/* Dynamic list of calendar events */
typedef struct _EVENT_LIST {
    CALENDAR_EVENT* pItems;
    DWORD           dwCount;
    DWORD           dwCapacity;
} EVENT_LIST;

/* Persistent application settings (stored in local INI file) */
typedef struct _APP_SETTINGS {
    WCHAR szIniPath[MAX_PATH];
    WCHAR szDataFolder[MAX_PATH];
    DWORD dwViewMode;
    BOOL  bStartMinimized;
    BOOL  bMinimizeToTray;
    BOOL  bNotifySoundEnabled;
    WCHAR szNotifySoundPath[MAX_PATH];
    BOOL  bStartWithWindows;
    WCHAR szFontName[LF_FACESIZE];
    DWORD dwFontSizePt;
    DWORD dwNextEventId;
    DWORD dwDataVersionCounter;
} APP_SETTINGS;

/* Theme color palette (adapted dynamically to Windows Light/Dark mode) */
typedef struct _APP_THEME {
    BOOL     bDarkMode;
    COLORREF crBgWindow;
    COLORREF crBgSurface;
    COLORREF crBgHeader;
    COLORREF crBgGridAlt;
    COLORREF crBgToday;
    COLORREF crBgSelected;
    COLORREF crBgHover;
    COLORREF crBorderSubtle;
    COLORREF crBorderStrong;
    COLORREF crTextPrimary;
    COLORREF crTextSecondary;
    COLORREF crTextMuted;
    COLORREF crAccent;
    COLORREF crAccentText;
    COLORREF crCurrentTime;
    COLORREF crEventColors[EVENT_COLOR_COUNT];
    COLORREF crEventBgTint[EVENT_COLOR_COUNT];
} APP_THEME;

/* Tracks notifications already triggered so occurrences notify only once */
typedef struct _FIRED_NOTIFY_ENTRY {
    DWORD     dwEventId;
    DWORD     dwEventVersion;
    ULONGLONG ullOccStartFt;
} FIRED_NOTIFY_ENTRY;

typedef struct _FIRED_NOTIFY_LIST {
    FIRED_NOTIFY_ENTRY* pItems;
    DWORD               dwCount;
    DWORD               dwCapacity;
} FIRED_NOTIFY_LIST;

#endif /* AGENDA_TYPES_H */
