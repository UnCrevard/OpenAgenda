#ifndef AGENDA_CALENDAR_VIEW_H
#define AGENDA_CALENDAR_VIEW_H

#include "types.h"

#define AGENDA_CANVAS_CLASS     L"AgendaWin32CanvasClass"

#define WM_APP_CANVAS_CREATE    (WM_APP + 200) /* lParam = SYSTEMTIME* (owned by caller temporarily) */
#define WM_APP_CANVAS_EDIT      (WM_APP + 201) /* wParam = dwEventId */
#define WM_APP_CANVAS_NAVIGATE  (WM_APP + 202) /* wParam = new VIEW_MODE, lParam = SYSTEMTIME* */

typedef enum _HIT_KIND {
    HIT_NONE           = 0,
    HIT_EVENT          = 1,
    HIT_DAY_CELL       = 2,
    HIT_TIME_SLOT      = 3,
    HIT_YEAR_MONTH     = 4,
    HIT_MORE_LINK      = 5,
    HIT_YEAR_EVENT_DAY = 6
} HIT_KIND;

typedef struct _HIT_ITEM {
    RECT       rc;
    HIT_KIND   kind;
    DWORD      dwEventId;
    SYSTEMTIME stDate;
} HIT_ITEM;

typedef struct _CALENDAR_CANVAS {
    HWND              hwnd;
    HWND              hwndParent;
    DWORD             dwViewMode;
    SYSTEMTIME        stAnchor;
    SYSTEMTIME        stSelected;
    WCHAR             szFilter[128];
    const EVENT_LIST* pEvents;
    const APP_THEME*  pTheme;
    LONG              lScrollY;
    LONG              lMaxScrollY;
    UINT              uDpi;
    HIT_ITEM*         pHits;
    DWORD             dwHitCount;
    DWORD             dwHitCap;
    LONG              lHoverIndex;
    DWORD             dwSelectedEventId;
} CALENDAR_CANVAS;

/* Registers the custom double-buffered calendar canvas window class */
BOOL CalendarCanvas_Register(HINSTANCE hInst);

/* Creates the calendar canvas child window */
HWND CalendarCanvas_Create(
    HINSTANCE hInst,
    HWND hwndParent,
    const EVENT_LIST* pEvents,
    const APP_THEME* pTheme
);

/* Updates state and triggers repaint */
void CalendarCanvas_SetView(HWND hwndCanvas, DWORD dwViewMode, const SYSTEMTIME* pstAnchor);
void CalendarCanvas_SetFilter(HWND hwndCanvas, const WCHAR* pszFilter);
void CalendarCanvas_Refresh(HWND hwndCanvas);
void CalendarCanvas_SetDpi(HWND hwndCanvas, UINT uDpi);
void CalendarCanvas_GetAnchorDate(HWND hwndCanvas, SYSTEMTIME* pstOut);
void CalendarCanvas_NavigateStep(HWND hwndCanvas, LONG lDirection); /* -1 prev, 0 today, +1 next */

#endif /* AGENDA_CALENDAR_VIEW_H */
