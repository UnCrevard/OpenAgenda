#include "storage.h"
#include "date_utils.h"
#include "recurrence.h"
#include "settings.h"

BOOL Storage_EnsureDirectory(const WCHAR* pszDirPath)
{
    WCHAR szBuf[MAX_PATH];
    DWORD dwLen;
    DWORD i;

    if (!pszDirPath || pszDirPath[0] == L'\0') {
        return FALSE;
    }

    wcsncpy(szBuf, pszDirPath, MAX_PATH - 1);
    szBuf[MAX_PATH - 1] = L'\0';
    dwLen = (DWORD)wcslen(szBuf);

    /* Trim trailing slashes except root drive like C:\ */
    while (dwLen > 3 && (szBuf[dwLen - 1] == L'\\' || szBuf[dwLen - 1] == L'/')) {
        szBuf[dwLen - 1] = L'\0';
        dwLen--;
    }

    for (i = 0; i < dwLen; ++i) {
        if (szBuf[i] == L'\\' || szBuf[i] == L'/') {
            WCHAR chSave = szBuf[i];
            if (i == 0 || (i == 2 && szBuf[1] == L':')) {
                continue;
            }
            szBuf[i] = L'\0';
            CreateDirectoryW(szBuf, NULL);
            szBuf[i] = chSave;
        }
    }

    if (!CreateDirectoryW(szBuf, NULL)) {
        DWORD dwErr = GetLastError();
        if (dwErr != ERROR_ALREADY_EXISTS) {
            return FALSE;
        }
    }
    return TRUE;
}

/* Dynamic UTF-8 string builder for JSON serialization */
typedef struct _UTF8_BUILDER {
    char* pData;
    DWORD dwLen;
    DWORD dwCap;
} UTF8_BUILDER;

static BOOL Builder_AppendBytes(UTF8_BUILDER* pB, const char* pszStr, DWORD dwCount)
{
    if (pB->dwLen + dwCount + 1 > pB->dwCap) {
        DWORD dwNewCap = (pB->dwCap == 0) ? 4096 : pB->dwCap * 2;
        while (dwNewCap < pB->dwLen + dwCount + 1) {
            dwNewCap *= 2;
        }
        char* pNew = (char*)realloc(pB->pData, dwNewCap);
        if (!pNew) {
            return FALSE;
        }
        pB->pData = pNew;
        pB->dwCap = dwNewCap;
    }
    memcpy(pB->pData + pB->dwLen, pszStr, dwCount);
    pB->dwLen += dwCount;
    pB->pData[pB->dwLen] = '\0';
    return TRUE;
}

static BOOL Builder_AppendStr(UTF8_BUILDER* pB, const char* pszStr)
{
    return Builder_AppendBytes(pB, pszStr, (DWORD)strlen(pszStr));
}

static BOOL Builder_AppendJsonWideString(UTF8_BUILDER* pB, const WCHAR* pszWide)
{
    int nUtf8Len;
    char* pszUtf8;
    int i;

    if (!Builder_AppendBytes(pB, "\"", 1)) return FALSE;
    if (!pszWide || pszWide[0] == L'\0') {
        return Builder_AppendBytes(pB, "\"", 1);
    }

    nUtf8Len = WideCharToMultiByte(CP_UTF8, 0, pszWide, -1, NULL, 0, NULL, NULL);
    if (nUtf8Len <= 0) {
        return Builder_AppendBytes(pB, "\"", 1);
    }
    pszUtf8 = (char*)malloc((size_t)nUtf8Len);
    if (!pszUtf8) {
        return FALSE;
    }
    WideCharToMultiByte(CP_UTF8, 0, pszWide, -1, pszUtf8, nUtf8Len, NULL, NULL);

    for (i = 0; pszUtf8[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)pszUtf8[i];
        if (c == '\"') {
            Builder_AppendStr(pB, "\\\"");
        } else if (c == '\\') {
            Builder_AppendStr(pB, "\\\\");
        } else if (c == '\b') {
            Builder_AppendStr(pB, "\\b");
        } else if (c == '\f') {
            Builder_AppendStr(pB, "\\f");
        } else if (c == '\n') {
            Builder_AppendStr(pB, "\\n");
        } else if (c == '\r') {
            Builder_AppendStr(pB, "\\r");
        } else if (c == '\t') {
            Builder_AppendStr(pB, "\\t");
        } else if (c < 0x20) {
            char szEsc[8];
            sprintf(szEsc, "\\u%04x", (unsigned int)c);
            Builder_AppendStr(pB, szEsc);
        } else {
            Builder_AppendBytes(pB, (const char*)&c, 1);
        }
    }

    free(pszUtf8);
    return Builder_AppendBytes(pB, "\"", 1);
}

static BOOL WriteBufferToFile(const WCHAR* pszPath, const char* pData, DWORD dwLen)
{
    HANDLE hFile;
    DWORD dwWritten = 0;
    BOOL bOk;

    hFile = CreateFileW(pszPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    bOk = WriteFile(hFile, pData, dwLen, &dwWritten, NULL);
    CloseHandle(hFile);
    return bOk && (dwWritten == dwLen);
}

BOOL Storage_SaveEvents(APP_SETTINGS* pSettings, const EVENT_LIST* pList)
{
    UTF8_BUILDER jb;
    SYSTEMTIME stNow;
    WCHAR szIsoNow[64];
    WCHAR szJsonPath[MAX_PATH];
    WCHAR szTmpPath[MAX_PATH];
    WCHAR szVersionsDir[MAX_PATH];
    WCHAR szSnapshotPath[MAX_PATH];
    char szLine[256];
    DWORD i;
    BOOL bOk;

    if (!Storage_EnsureDirectory(pSettings->szDataFolder)) {
        return FALSE;
    }

    _snwprintf(szVersionsDir, MAX_PATH, L"%s\\%s", pSettings->szDataFolder, AGENDA_VERSIONS_SUBDIR);
    szVersionsDir[MAX_PATH - 1] = L'\0';
    Storage_EnsureDirectory(szVersionsDir);

    pSettings->dwDataVersionCounter += 1;
    GetLocalTime(&stNow);
    Date_FormatIsoDateTime(&stNow, szIsoNow, 64);

    jb.pData = NULL;
    jb.dwLen = 0;
    jb.dwCap = 0;

    Builder_AppendStr(&jb, "{\n");
    Builder_AppendStr(&jb, "  \"schemaVersion\": 1,\n");
    sprintf(szLine, "  \"dataVersion\": %u,\n", (UINT)pSettings->dwDataVersionCounter);
    Builder_AppendStr(&jb, szLine);
    sprintf(szLine, "  \"nextEventId\": %u,\n", (UINT)pSettings->dwNextEventId);
    Builder_AppendStr(&jb, szLine);
    Builder_AppendStr(&jb, "  \"updatedAt\": ");
    Builder_AppendJsonWideString(&jb, szIsoNow);
    Builder_AppendStr(&jb, ",\n");
    Builder_AppendStr(&jb, "  \"events\": [\n");

    for (i = 0; i < pList->dwCount; ++i) {
        const CALENDAR_EVENT* pEv = &pList->pItems[i];
        WCHAR szStart[64];
        WCHAR szEnd[64];

        Date_FormatIsoDateTime(&pEv->stStart, szStart, 64);
        Date_FormatIsoDateTime(&pEv->stEndDate, szEnd, 64);

        Builder_AppendStr(&jb, "    {\n");
        sprintf(szLine, "      \"id\": %u,\n", (UINT)pEv->dwId);
        Builder_AppendStr(&jb, szLine);
        sprintf(szLine, "      \"version\": %u,\n", (UINT)pEv->dwVersion);
        Builder_AppendStr(&jb, szLine);

        Builder_AppendStr(&jb, "      \"title\": ");
        Builder_AppendJsonWideString(&jb, pEv->szTitle);
        Builder_AppendStr(&jb, ",\n");

        Builder_AppendStr(&jb, "      \"description\": ");
        Builder_AppendJsonWideString(&jb, pEv->szDescription);
        Builder_AppendStr(&jb, ",\n");

        Builder_AppendStr(&jb, "      \"startDateTime\": ");
        Builder_AppendJsonWideString(&jb, szStart);
        Builder_AppendStr(&jb, ",\n");

        sprintf(szLine, "      \"durationMinutes\": %u,\n", (UINT)pEv->dwDurationMinutes);
        Builder_AppendStr(&jb, szLine);
        sprintf(szLine, "      \"allDay\": %s,\n", pEv->bAllDay ? "true" : "false");
        Builder_AppendStr(&jb, szLine);
        sprintf(szLine, "      \"recurrenceType\": %u,\n", (UINT)pEv->dwRecurrenceType);
        Builder_AppendStr(&jb, szLine);
        sprintf(szLine, "      \"recurrenceInterval\": %u,\n", (UINT)pEv->dwRecurrenceInterval);
        Builder_AppendStr(&jb, szLine);
        sprintf(szLine, "      \"weekdaysMask\": %u,\n", (UINT)pEv->dwWeekdaysMask);
        Builder_AppendStr(&jb, szLine);
        sprintf(szLine, "      \"nthOrdinal\": %ld,\n", (long)pEv->lNthOrdinal);
        Builder_AppendStr(&jb, szLine);
        sprintf(szLine, "      \"nthWeekday\": %u,\n", (UINT)pEv->dwNthWeekday);
        Builder_AppendStr(&jb, szLine);
        sprintf(szLine, "      \"hasEndDate\": %s,\n", pEv->bHasEndDate ? "true" : "false");
        Builder_AppendStr(&jb, szLine);

        Builder_AppendStr(&jb, "      \"endDate\": ");
        Builder_AppendJsonWideString(&jb, szEnd);
        Builder_AppendStr(&jb, ",\n");

        sprintf(szLine, "      \"notifyMode\": %u,\n", (UINT)pEv->dwNotifyMode);
        Builder_AppendStr(&jb, szLine);
        sprintf(szLine, "      \"notifyMinutesBefore\": %u,\n", (UINT)pEv->dwNotifyMinutesBefore);
        Builder_AppendStr(&jb, szLine);
        sprintf(szLine, "      \"colorIndex\": %u\n", (UINT)pEv->dwColorIndex);
        Builder_AppendStr(&jb, szLine);

        if (i + 1 < pList->dwCount) {
            Builder_AppendStr(&jb, "    },\n");
        } else {
            Builder_AppendStr(&jb, "    }\n");
        }
    }

    Builder_AppendStr(&jb, "  ]\n}\n");

    _snwprintf(szJsonPath, MAX_PATH, L"%s\\%s", pSettings->szDataFolder, AGENDA_JSON_FILENAME);
    _snwprintf(szTmpPath, MAX_PATH, L"%s\\%s.tmp", pSettings->szDataFolder, AGENDA_JSON_FILENAME);
    szJsonPath[MAX_PATH - 1] = L'\0';
    szTmpPath[MAX_PATH - 1] = L'\0';

    bOk = WriteBufferToFile(szTmpPath, jb.pData, jb.dwLen);
    if (bOk) {
        bOk = MoveFileExW(szTmpPath, szJsonPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (!bOk) {
            bOk = WriteBufferToFile(szJsonPath, jb.pData, jb.dwLen);
            DeleteFileW(szTmpPath);
        }
    }

    /* Create unlimited timestamped snapshot in versions/ on every change */
    if (bOk) {
        _snwprintf(
            szSnapshotPath,
            MAX_PATH,
            L"%s\\events_%04u%02u%02u_%02u%02u%02u_v%04u.json",
            szVersionsDir,
            (UINT)stNow.wYear,
            (UINT)stNow.wMonth,
            (UINT)stNow.wDay,
            (UINT)stNow.wHour,
            (UINT)stNow.wMinute,
            (UINT)stNow.wSecond,
            (UINT)pSettings->dwDataVersionCounter
        );
        szSnapshotPath[MAX_PATH - 1] = L'\0';
        WriteBufferToFile(szSnapshotPath, jb.pData, jb.dwLen);
        Settings_Save(pSettings);
    }

    free(jb.pData);
    return bOk;
}

/* Minimal recursive-descent UTF-8 JSON parser */
typedef struct _JSON_CURSOR {
    const char* pCur;
    const char* pEnd;
} JSON_CURSOR;

static void Json_SkipWs(JSON_CURSOR* pC)
{
    while (pC->pCur < pC->pEnd) {
        char c = *pC->pCur;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            pC->pCur++;
        } else {
            break;
        }
    }
}

static BOOL Json_Consume(JSON_CURSOR* pC, char chExpected)
{
    Json_SkipWs(pC);
    if (pC->pCur < pC->pEnd && *pC->pCur == chExpected) {
        pC->pCur++;
        return TRUE;
    }
    return FALSE;
}

static BOOL Json_ParseStringUtf8(JSON_CURSOR* pC, char* pszOut, DWORD cchOut)
{
    DWORD dwOut = 0;
    Json_SkipWs(pC);
    if (pC->pCur >= pC->pEnd || *pC->pCur != '\"') {
        return FALSE;
    }
    pC->pCur++;

    while (pC->pCur < pC->pEnd) {
        char c = *pC->pCur++;
        if (c == '\"') {
            if (cchOut > 0) {
                pszOut[dwOut < cchOut ? dwOut : cchOut - 1] = '\0';
            }
            return TRUE;
        }
        if (c == '\\') {
            if (pC->pCur >= pC->pEnd) return FALSE;
            char esc = *pC->pCur++;
            if (esc == '\"' || esc == '\\' || esc == '/') {
                c = esc;
            } else if (esc == 'b') {
                c = '\b';
            } else if (esc == 'f') {
                c = '\f';
            } else if (esc == 'n') {
                c = '\n';
            } else if (esc == 'r') {
                c = '\r';
            } else if (esc == 't') {
                c = '\t';
            } else if (esc == 'u') {
                UINT code = 0;
                int k;
                if (pC->pCur + 4 > pC->pEnd) return FALSE;
                for (k = 0; k < 4; ++k) {
                    char h = pC->pCur[k];
                    code <<= 4;
                    if (h >= '0' && h <= '9') code |= (UINT)(h - '0');
                    else if (h >= 'a' && h <= 'f') code |= (UINT)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') code |= (UINT)(h - 'A' + 10);
                    else return FALSE;
                }
                pC->pCur += 4;
                if (code < 0x80) {
                    c = (char)code;
                } else {
                    WCHAR wch = (WCHAR)code;
                    char utfBuf[4];
                    int nBytes = WideCharToMultiByte(CP_UTF8, 0, &wch, 1, utfBuf, 4, NULL, NULL);
                    int m;
                    for (m = 0; m < nBytes; ++m) {
                        if (dwOut + 1 < cchOut) {
                            pszOut[dwOut++] = utfBuf[m];
                        }
                    }
                    continue;
                }
            } else {
                c = esc;
            }
        }
        if (dwOut + 1 < cchOut) {
            pszOut[dwOut++] = c;
        }
    }
    return FALSE;
}

static BOOL Json_ParseWideString(JSON_CURSOR* pC, WCHAR* pszWideOut, DWORD cchWideOut)
{
    char szUtf8[2048];
    if (!Json_ParseStringUtf8(pC, szUtf8, sizeof(szUtf8))) {
        return FALSE;
    }
    if (cchWideOut > 0) {
        int nRes = MultiByteToWideChar(CP_UTF8, 0, szUtf8, -1, pszWideOut, (int)cchWideOut);
        if (nRes <= 0) {
            pszWideOut[0] = L'\0';
        }
        pszWideOut[cchWideOut - 1] = L'\0';
    }
    return TRUE;
}

static BOOL Json_ParseInt64(JSON_CURSOR* pC, LONGLONG* pllOut)
{
    char szNum[64];
    DWORD dwLen = 0;
    Json_SkipWs(pC);
    while (pC->pCur < pC->pEnd && dwLen + 1 < sizeof(szNum)) {
        char c = *pC->pCur;
        if ((c >= '0' && c <= '9') || c == '-' || c == '+') {
            szNum[dwLen++] = c;
            pC->pCur++;
        } else {
            break;
        }
    }
    if (dwLen == 0) {
        return FALSE;
    }
    szNum[dwLen] = '\0';
    *pllOut = _atoi64(szNum);
    return TRUE;
}

static BOOL Json_ParseBool(JSON_CURSOR* pC, BOOL* pbOut)
{
    Json_SkipWs(pC);
    if (pC->pCur + 4 <= pC->pEnd && strncmp(pC->pCur, "true", 4) == 0) {
        pC->pCur += 4;
        *pbOut = TRUE;
        return TRUE;
    }
    if (pC->pCur + 5 <= pC->pEnd && strncmp(pC->pCur, "false", 5) == 0) {
        pC->pCur += 5;
        *pbOut = FALSE;
        return TRUE;
    }
    return FALSE;
}

static BOOL Json_SkipValue(JSON_CURSOR* pC);

static BOOL Json_SkipArray(JSON_CURSOR* pC)
{
    if (!Json_Consume(pC, '[')) return FALSE;
    Json_SkipWs(pC);
    if (pC->pCur < pC->pEnd && *pC->pCur == ']') {
        pC->pCur++;
        return TRUE;
    }
    while (pC->pCur < pC->pEnd) {
        if (!Json_SkipValue(pC)) return FALSE;
        if (Json_Consume(pC, ',')) continue;
        if (Json_Consume(pC, ']')) return TRUE;
        return FALSE;
    }
    return FALSE;
}

static BOOL Json_SkipObject(JSON_CURSOR* pC)
{
    char szDummyKey[256];
    if (!Json_Consume(pC, '{')) return FALSE;
    Json_SkipWs(pC);
    if (pC->pCur < pC->pEnd && *pC->pCur == '}') {
        pC->pCur++;
        return TRUE;
    }
    while (pC->pCur < pC->pEnd) {
        if (!Json_ParseStringUtf8(pC, szDummyKey, sizeof(szDummyKey))) return FALSE;
        if (!Json_Consume(pC, ':')) return FALSE;
        if (!Json_SkipValue(pC)) return FALSE;
        if (Json_Consume(pC, ',')) continue;
        if (Json_Consume(pC, '}')) return TRUE;
        return FALSE;
    }
    return FALSE;
}

static BOOL Json_SkipValue(JSON_CURSOR* pC)
{
    char szDummy[1024];
    LONGLONG llDummy;
    BOOL bDummy;

    Json_SkipWs(pC);
    if (pC->pCur >= pC->pEnd) return FALSE;
    if (*pC->pCur == '\"') return Json_ParseStringUtf8(pC, szDummy, sizeof(szDummy));
    if (*pC->pCur == '{') return Json_SkipObject(pC);
    if (*pC->pCur == '[') return Json_SkipArray(pC);
    if (*pC->pCur == 't' || *pC->pCur == 'f') return Json_ParseBool(pC, &bDummy);
    if (pC->pCur + 4 <= pC->pEnd && strncmp(pC->pCur, "null", 4) == 0) {
        pC->pCur += 4;
        return TRUE;
    }
    /* Skip numeric literal (including optional decimal/exponent) */
    if (!Json_ParseInt64(pC, &llDummy)) return FALSE;
    while (pC->pCur < pC->pEnd && (*pC->pCur == '.' || *pC->pCur == 'e' || *pC->pCur == 'E' ||
           (*pC->pCur >= '0' && *pC->pCur <= '9') || *pC->pCur == '+' || *pC->pCur == '-')) {
        pC->pCur++;
    }
    return TRUE;
}

static BOOL Json_ParseEventObject(JSON_CURSOR* pC, CALENDAR_EVENT* pEv)
{
    char szKey[128];

    ZeroMemory(pEv, sizeof(*pEv));
    GetLocalTime(&pEv->stStart);
    pEv->stEndDate = pEv->stStart;
    pEv->dwDurationMinutes = 60;
    pEv->dwRecurrenceInterval = 1;
    pEv->lNthOrdinal = 1;

    if (!Json_Consume(pC, '{')) return FALSE;
    Json_SkipWs(pC);
    if (pC->pCur < pC->pEnd && *pC->pCur == '}') {
        pC->pCur++;
        return TRUE;
    }

    while (pC->pCur < pC->pEnd) {
        if (!Json_ParseStringUtf8(pC, szKey, sizeof(szKey))) return FALSE;
        if (!Json_Consume(pC, ':')) return FALSE;

        if (strcmp(szKey, "id") == 0) {
            LONGLONG v = 0;
            if (!Json_ParseInt64(pC, &v)) return FALSE;
            pEv->dwId = (DWORD)v;
        } else if (strcmp(szKey, "version") == 0) {
            LONGLONG v = 1;
            if (!Json_ParseInt64(pC, &v)) return FALSE;
            pEv->dwVersion = (DWORD)v;
        } else if (strcmp(szKey, "title") == 0) {
            if (!Json_ParseWideString(pC, pEv->szTitle, MAX_EVENT_TITLE)) return FALSE;
        } else if (strcmp(szKey, "description") == 0) {
            if (!Json_ParseWideString(pC, pEv->szDescription, MAX_EVENT_DESC)) return FALSE;
        } else if (strcmp(szKey, "startDateTime") == 0) {
            WCHAR szIso[64];
            if (!Json_ParseWideString(pC, szIso, 64)) return FALSE;
            Date_ParseIsoDateTime(szIso, &pEv->stStart);
        } else if (strcmp(szKey, "durationMinutes") == 0) {
            LONGLONG v = 60;
            if (!Json_ParseInt64(pC, &v)) return FALSE;
            pEv->dwDurationMinutes = (DWORD)(v > 0 ? v : 30);
        } else if (strcmp(szKey, "allDay") == 0) {
            if (!Json_ParseBool(pC, &pEv->bAllDay)) return FALSE;
        } else if (strcmp(szKey, "recurrenceType") == 0) {
            LONGLONG v = 0;
            if (!Json_ParseInt64(pC, &v)) return FALSE;
            pEv->dwRecurrenceType = (DWORD)v;
        } else if (strcmp(szKey, "recurrenceInterval") == 0) {
            LONGLONG v = 1;
            if (!Json_ParseInt64(pC, &v)) return FALSE;
            pEv->dwRecurrenceInterval = (DWORD)(v >= 1 ? v : 1);
        } else if (strcmp(szKey, "weekdaysMask") == 0) {
            LONGLONG v = 0;
            if (!Json_ParseInt64(pC, &v)) return FALSE;
            pEv->dwWeekdaysMask = (DWORD)v;
        } else if (strcmp(szKey, "nthOrdinal") == 0) {
            LONGLONG v = 1;
            if (!Json_ParseInt64(pC, &v)) return FALSE;
            pEv->lNthOrdinal = (LONG)v;
        } else if (strcmp(szKey, "nthWeekday") == 0) {
            LONGLONG v = 0;
            if (!Json_ParseInt64(pC, &v)) return FALSE;
            pEv->dwNthWeekday = (DWORD)v;
        } else if (strcmp(szKey, "hasEndDate") == 0) {
            if (!Json_ParseBool(pC, &pEv->bHasEndDate)) return FALSE;
        } else if (strcmp(szKey, "endDate") == 0) {
            WCHAR szIso[64];
            if (!Json_ParseWideString(pC, szIso, 64)) return FALSE;
            Date_ParseIsoDateTime(szIso, &pEv->stEndDate);
        } else if (strcmp(szKey, "notifyMode") == 0) {
            LONGLONG v = 0;
            if (!Json_ParseInt64(pC, &v)) return FALSE;
            pEv->dwNotifyMode = (DWORD)v;
        } else if (strcmp(szKey, "notifyMinutesBefore") == 0) {
            LONGLONG v = 0;
            if (!Json_ParseInt64(pC, &v)) return FALSE;
            pEv->dwNotifyMinutesBefore = (DWORD)v;
        } else if (strcmp(szKey, "colorIndex") == 0) {
            LONGLONG v = 0;
            if (!Json_ParseInt64(pC, &v)) return FALSE;
            pEv->dwColorIndex = (DWORD)(v % EVENT_COLOR_COUNT);
        } else {
            if (!Json_SkipValue(pC)) return FALSE;
        }

        if (Json_Consume(pC, ',')) continue;
        if (Json_Consume(pC, '}')) return TRUE;
        return FALSE;
    }
    return FALSE;
}

static BOOL Json_ParseEventsArray(JSON_CURSOR* pC, EVENT_LIST* pList)
{
    if (!Json_Consume(pC, '[')) return FALSE;
    Json_SkipWs(pC);
    if (pC->pCur < pC->pEnd && *pC->pCur == ']') {
        pC->pCur++;
        return TRUE;
    }

    while (pC->pCur < pC->pEnd) {
        CALENDAR_EVENT ev;
        if (!Json_ParseEventObject(pC, &ev)) return FALSE;
        EventList_Add(pList, &ev);
        if (Json_Consume(pC, ',')) continue;
        if (Json_Consume(pC, ']')) return TRUE;
        return FALSE;
    }
    return FALSE;
}

BOOL Storage_LoadEvents(APP_SETTINGS* pSettings, EVENT_LIST* pList)
{
    WCHAR szJsonPath[MAX_PATH];
    HANDLE hFile;
    LARGE_INTEGER liSize;
    char* pBuf;
    DWORD dwRead = 0;
    JSON_CURSOR cursor;
    char szKey[128];
    DWORD dwMaxId = 0;
    DWORD i;

    EventList_Free(pList);
    EventList_Init(pList);

    _snwprintf(szJsonPath, MAX_PATH, L"%s\\%s", pSettings->szDataFolder, AGENDA_JSON_FILENAME);
    szJsonPath[MAX_PATH - 1] = L'\0';

    hFile = CreateFileW(szJsonPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        /* No file yet: start cleanly with empty list */
        return TRUE;
    }

    if (!GetFileSizeEx(hFile, &liSize) || liSize.QuadPart <= 0 || liSize.QuadPart > 64 * 1024 * 1024) {
        CloseHandle(hFile);
        return TRUE;
    }

    pBuf = (char*)malloc((size_t)liSize.QuadPart + 1);
    if (!pBuf) {
        CloseHandle(hFile);
        return FALSE;
    }

    if (!ReadFile(hFile, pBuf, (DWORD)liSize.QuadPart, &dwRead, NULL)) {
        free(pBuf);
        CloseHandle(hFile);
        return FALSE;
    }
    CloseHandle(hFile);
    pBuf[dwRead] = '\0';

    cursor.pCur = pBuf;
    cursor.pEnd = pBuf + dwRead;

    /* Strip optional UTF-8 BOM */
    if (dwRead >= 3 && (unsigned char)pBuf[0] == 0xEF &&
        (unsigned char)pBuf[1] == 0xBB && (unsigned char)pBuf[2] == 0xBF) {
        cursor.pCur += 3;
    }

    if (Json_Consume(&cursor, '{')) {
        Json_SkipWs(&cursor);
        if (cursor.pCur < cursor.pEnd && *cursor.pCur != '}') {
            while (cursor.pCur < cursor.pEnd) {
                if (!Json_ParseStringUtf8(&cursor, szKey, sizeof(szKey))) break;
                if (!Json_Consume(&cursor, ':')) break;

                if (strcmp(szKey, "dataVersion") == 0) {
                    LONGLONG v = 0;
                    if (Json_ParseInt64(&cursor, &v) && (DWORD)v > pSettings->dwDataVersionCounter) {
                        pSettings->dwDataVersionCounter = (DWORD)v;
                    }
                } else if (strcmp(szKey, "nextEventId") == 0) {
                    LONGLONG v = 1;
                    if (Json_ParseInt64(&cursor, &v) && (DWORD)v > pSettings->dwNextEventId) {
                        pSettings->dwNextEventId = (DWORD)v;
                    }
                } else if (strcmp(szKey, "events") == 0) {
                    Json_ParseEventsArray(&cursor, pList);
                } else {
                    if (!Json_SkipValue(&cursor)) break;
                }

                if (Json_Consume(&cursor, ',')) continue;
                if (Json_Consume(&cursor, '}')) break;
                break;
            }
        }
    }

    for (i = 0; i < pList->dwCount; ++i) {
        if (pList->pItems[i].dwId >= dwMaxId) {
            dwMaxId = pList->pItems[i].dwId;
        }
    }
    if (pSettings->dwNextEventId <= dwMaxId) {
        pSettings->dwNextEventId = dwMaxId + 1;
    }

    free(pBuf);
    return TRUE;
}

DWORD Storage_CountSnapshots(const WCHAR* pszDataFolder)
{
    WCHAR szSearch[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    DWORD dwCount = 0;

    if (!pszDataFolder || pszDataFolder[0] == L'\0') {
        return 0;
    }
    _snwprintf(szSearch, MAX_PATH, L"%s\\%s\\*.json", pszDataFolder, AGENDA_VERSIONS_SUBDIR);
    szSearch[MAX_PATH - 1] = L'\0';

    hFind = FindFirstFileW(szSearch, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            dwCount++;
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return dwCount;
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    (void)lParam;
    if (uMsg == BFFM_INITIALIZED && lpData != 0) {
        SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, lpData);
    }
    return 0;
}

BOOL Storage_PickFolderDialog(HWND hOwner, const WCHAR* pszInitialFolder, WCHAR* pszOutFolder, DWORD cchOut)
{
    BROWSEINFOW bi;
    PIDLIST_ABSOLUTE pidl;
    WCHAR szDisplay[MAX_PATH];

    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = hOwner;
    bi.pszDisplayName = szDisplay;
    bi.lpszTitle = L"Choisissez le dossier de stockage des donn\u00E9es JSON :";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
    bi.lpfn = BrowseCallbackProc;
    bi.lParam = (LPARAM)pszInitialFolder;

    pidl = SHBrowseForFolderW(&bi);
    if (!pidl) {
        return FALSE;
    }

    if (SHGetPathFromIDListW(pidl, szDisplay)) {
        wcsncpy(pszOutFolder, szDisplay, cchOut - 1);
        pszOutFolder[cchOut - 1] = L'\0';
        CoTaskMemFree(pidl);
        return TRUE;
    }

    CoTaskMemFree(pidl);
    return FALSE;
}
