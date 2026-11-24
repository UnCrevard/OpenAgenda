#include "notify.h"
#include "date_utils.h"
#include "recurrence.h"

#define TRAY_ICON_UID 1

void FiredList_Init(FIRED_NOTIFY_LIST* pList)
{
    pList->pItems = NULL;
    pList->dwCount = 0;
    pList->dwCapacity = 0;
}

void FiredList_Free(FIRED_NOTIFY_LIST* pList)
{
    if (pList->pItems) {
        free(pList->pItems);
        pList->pItems = NULL;
    }
    pList->dwCount = 0;
    pList->dwCapacity = 0;
}

static BOOL FiredList_Contains(const FIRED_NOTIFY_LIST* pList, DWORD dwEventId, DWORD dwVersion, ULONGLONG ullOccStart)
{
    DWORD i;
    for (i = 0; i < pList->dwCount; ++i) {
        if (pList->pItems[i].dwEventId == dwEventId &&
            pList->pItems[i].dwEventVersion == dwVersion &&
            pList->pItems[i].ullOccStartFt == ullOccStart) {
            return TRUE;
        }
    }
    return FALSE;
}

static void FiredList_Add(FIRED_NOTIFY_LIST* pList, DWORD dwEventId, DWORD dwVersion, ULONGLONG ullOccStart)
{
    if (pList->dwCount >= pList->dwCapacity) {
        DWORD dwNewCap = (pList->dwCapacity == 0) ? 32 : pList->dwCapacity * 2;
        FIRED_NOTIFY_ENTRY* pNew = (FIRED_NOTIFY_ENTRY*)realloc(
            pList->pItems, dwNewCap * sizeof(FIRED_NOTIFY_ENTRY));
        if (!pNew) {
            return;
        }
        pList->pItems = pNew;
        pList->dwCapacity = dwNewCap;
    }
    pList->pItems[pList->dwCount].dwEventId = dwEventId;
    pList->pItems[pList->dwCount].dwEventVersion = dwVersion;
    pList->pItems[pList->dwCount].ullOccStartFt = ullOccStart;
    pList->dwCount++;
}

BOOL Notify_InitTray(HWND hwnd, HICON hIcon)
{
    NOTIFYICONDATAW nid;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ICON_UID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_APP_TRAYICON;
    nid.hIcon = hIcon;
    wcsncpy(nid.szTip, AGENDA_APP_NAME, 127);
    nid.szTip[127] = L'\0';
    return Shell_NotifyIconW(NIM_ADD, &nid);
}

BOOL Notify_UpdateTrayIcon(HWND hwnd, HICON hIcon)
{
    NOTIFYICONDATAW nid;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ICON_UID;
    nid.uFlags = NIF_ICON;
    nid.hIcon = hIcon;
    return Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void Notify_RemoveTray(HWND hwnd)
{
    NOTIFYICONDATAW nid;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ICON_UID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void Notify_PlaySound(const APP_SETTINGS* pSettings)
{
    if (!pSettings || !pSettings->bNotifySoundEnabled) {
        return;
    }
    if (pSettings->szNotifySoundPath[0] != L'\0' &&
        GetFileAttributesW(pSettings->szNotifySoundPath) != INVALID_FILE_ATTRIBUTES) {
        if (PlaySoundW(pSettings->szNotifySoundPath, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) {
            return;
        }
    }
    if (!PlaySoundW(L"Notification.Default", NULL, SND_ALIAS | SND_ASYNC | SND_NODEFAULT)) {
        MessageBeep(MB_ICONINFORMATION);
    }
}

BOOL Notify_ShowBalloon(
    HWND hwnd,
    const WCHAR* pszTitle,
    const WCHAR* pszDescription,
    const APP_SETTINGS* pSettings
)
{
    NOTIFYICONDATAW nid;
    BOOL bCustomWav = FALSE;

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ICON_UID;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;

    if (!pSettings || !pSettings->bNotifySoundEnabled) {
        nid.dwInfoFlags |= NIIF_NOSOUND;
    } else if (pSettings->szNotifySoundPath[0] != L'\0') {
        bCustomWav = TRUE;
        nid.dwInfoFlags |= NIIF_NOSOUND;
    }

    if (pszTitle && pszTitle[0] != L'\0') {
        wcsncpy(nid.szInfoTitle, pszTitle, 63);
        nid.szInfoTitle[63] = L'\0';
    } else {
        wcsncpy(nid.szInfoTitle, AGENDA_APP_NAME, 63);
        nid.szInfoTitle[63] = L'\0';
    }

    if (pszDescription && pszDescription[0] != L'\0') {
        wcsncpy(nid.szInfo, pszDescription, 255);
        nid.szInfo[255] = L'\0';
    } else {
        wcsncpy(nid.szInfo, L"Rappel d'\u00E9v\u00E9nement", 255);
        nid.szInfo[255] = L'\0';
    }

    if (bCustomWav) {
        Notify_PlaySound(pSettings);
    }

    return Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void Notify_CheckDueEvents(
    HWND hwnd,
    const EVENT_LIST* pEvents,
    const APP_SETTINGS* pSettings,
    FIRED_NOTIFY_LIST* pFired
)
{
    SYSTEMTIME stNow;
    SYSTEMTIME stRangeStart;
    SYSTEMTIME stRangeEnd;
    ULONGLONG ullNow;
    EVENT_OCCURRENCE* pOccs = NULL;
    DWORD dwCount = 0;
    DWORD i;

    if (!pEvents || pEvents->dwCount == 0) {
        return;
    }

    GetLocalTime(&stNow);
    ullNow = Date_ToUInt64(&stNow);

    stRangeStart = stNow;
    Date_AddDays(&stRangeStart, -1);
    stRangeEnd = stNow;
    Date_AddDays(&stRangeEnd, 3);

    if (!Recurrence_ExpandRange(pEvents, &stRangeStart, &stRangeEnd, NULL, &pOccs, &dwCount)) {
        return;
    }

    for (i = 0; i < dwCount; ++i) {
        const EVENT_OCCURRENCE* pOcc = &pOccs[i];
        const CALENDAR_EVENT* pEv = &pEvents->pItems[pOcc->dwEventIndex];
        ULONGLONG ullNotifyAt;
        ULONGLONG ullExpireAt;
        DWORD dwLeadMins;

        if (pEv->dwNotifyMode == NOTIFY_NONE) {
            continue;
        }

        dwLeadMins = (pEv->dwNotifyMode == NOTIFY_IMMEDIATE) ? 0 : pEv->dwNotifyMinutesBefore;
        ullNotifyAt = pOcc->ullStartFt - (ULONGLONG)dwLeadMins * 60ULL * 10000000ULL;
        ullExpireAt = pOcc->ullStartFt + 5ULL * 60ULL * 10000000ULL;

        if (ullNow >= ullNotifyAt && ullNow <= ullExpireAt) {
            if (!FiredList_Contains(pFired, pEv->dwId, pEv->dwVersion, pOcc->ullStartFt)) {
                WCHAR szTitle[64];
                WCHAR szDesc[256];
                WCHAR szDispTitle[64];
                LONG lAge = Recurrence_GetBirthdayAge(pEv, &pOcc->stStart);

                Recurrence_FormatDisplayTitle(pEv, &pOcc->stStart, szDispTitle, 64);

                _snwprintf(
                    szTitle,
                    64,
                    L"[%02u:%02u] %s",
                    (UINT)pOcc->stStart.wHour,
                    (UINT)pOcc->stStart.wMinute,
                    szDispTitle
                );
                szTitle[63] = L'\0';

                if (lAge >= 0) {
                    WCHAR szAgePart[64];
                    if (lAge == 0) {
                        wcsncpy(szAgePart, L"Anniversaire (Naissance)", 63);
                    } else {
                        _snwprintf(szAgePart, 64, L"Anniversaire : %ld %s",
                                   (long)lAge, (lAge == 1) ? L"an" : L"ans");
                    }
                    szAgePart[63] = L'\0';

                    if (pEv->szDescription[0] != L'\0') {
                        _snwprintf(szDesc, 256, L"%s \u2014 %s", szAgePart, pEv->szDescription);
                    } else {
                        _snwprintf(
                            szDesc,
                            256,
                            L"%s (%02u/%02u/%04u)",
                            szAgePart,
                            (UINT)pOcc->stStart.wDay,
                            (UINT)pOcc->stStart.wMonth,
                            (UINT)pOcc->stStart.wYear
                        );
                    }
                    szDesc[255] = L'\0';
                } else if (pEv->szDescription[0] != L'\0') {
                    wcsncpy(szDesc, pEv->szDescription, 255);
                    szDesc[255] = L'\0';
                } else {
                    _snwprintf(
                        szDesc,
                        256,
                        L"%02u/%02u/%04u \u00E0 %02u:%02u",
                        (UINT)pOcc->stStart.wDay,
                        (UINT)pOcc->stStart.wMonth,
                        (UINT)pOcc->stStart.wYear,
                        (UINT)pOcc->stStart.wHour,
                        (UINT)pOcc->stStart.wMinute
                    );
                    szDesc[255] = L'\0';
                }

                Notify_ShowBalloon(hwnd, szTitle, szDesc, pSettings);
                FiredList_Add(pFired, pEv->dwId, pEv->dwVersion, pOcc->ullStartFt);
            }
        }
    }

    if (pOccs) {
        free(pOccs);
    }
}
