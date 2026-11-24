#include "settings.h"

static void BuildShortcutPath(WCHAR* pszOut, DWORD cchOut)
{
    WCHAR szStartupDir[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, szStartupDir))) {
        _snwprintf(pszOut, cchOut, L"%s\\%s", szStartupDir, AGENDA_LNK_FILENAME);
        pszOut[cchOut - 1] = L'\0';
    } else if (cchOut > 0) {
        pszOut[0] = L'\0';
    }
}

void Settings_InitDefaults(APP_SETTINGS* pSettings)
{
    WCHAR szCwd[MAX_PATH];
    DWORD dwLen;

    ZeroMemory(pSettings, sizeof(*pSettings));

    dwLen = GetCurrentDirectoryW(MAX_PATH, szCwd);
    if (dwLen == 0 || dwLen >= MAX_PATH) {
        wcsncpy(szCwd, L".", MAX_PATH - 1);
    }

    _snwprintf(pSettings->szIniPath, MAX_PATH, L"%s\\%s", szCwd, AGENDA_INI_FILENAME);
    pSettings->szIniPath[MAX_PATH - 1] = L'\0';

    _snwprintf(pSettings->szDataFolder, MAX_PATH, L"%s\\data", szCwd);
    pSettings->szDataFolder[MAX_PATH - 1] = L'\0';

    pSettings->dwViewMode = VIEW_MONTH;
    pSettings->bStartMinimized = FALSE;
    pSettings->bMinimizeToTray = TRUE;
    pSettings->bNotifySoundEnabled = TRUE;
    pSettings->szNotifySoundPath[0] = L'\0';
    pSettings->bStartWithWindows = Settings_IsStartupShortcutPresent();
    wcsncpy(pSettings->szFontName, AGENDA_DEFAULT_FONT_NAME, LF_FACESIZE - 1);
    pSettings->szFontName[LF_FACESIZE - 1] = L'\0';
    pSettings->dwFontSizePt = AGENDA_DEFAULT_FONT_PT;
    pSettings->dwNextEventId = 1;
    pSettings->dwDataVersionCounter = 0;
}

BOOL Settings_Load(APP_SETTINGS* pSettings)
{
    WCHAR szBuf[MAX_PATH];
    const WCHAR* pszIni = pSettings->szIniPath;

    if (GetFileAttributesW(pszIni) == INVALID_FILE_ATTRIBUTES) {
        /* First launch: persist default INI file in current directory */
        Settings_Save(pSettings);
        return TRUE;
    }

    GetPrivateProfileStringW(L"General", L"DataFolder", pSettings->szDataFolder, szBuf, MAX_PATH, pszIni);
    if (szBuf[0] != L'\0') {
        wcsncpy(pSettings->szDataFolder, szBuf, MAX_PATH - 1);
        pSettings->szDataFolder[MAX_PATH - 1] = L'\0';
    }

    pSettings->dwViewMode = (DWORD)GetPrivateProfileIntW(L"General", L"ViewMode", (INT)pSettings->dwViewMode, pszIni);
    if (pSettings->dwViewMode > VIEW_YEAR) {
        pSettings->dwViewMode = VIEW_MONTH;
    }

    GetPrivateProfileStringW(L"General", L"FontName", AGENDA_DEFAULT_FONT_NAME, szBuf, LF_FACESIZE, pszIni);
    if (szBuf[0] != L'\0') {
        wcsncpy(pSettings->szFontName, szBuf, LF_FACESIZE - 1);
        pSettings->szFontName[LF_FACESIZE - 1] = L'\0';
    }
    pSettings->dwFontSizePt = (DWORD)GetPrivateProfileIntW(L"General", L"FontSizePt", AGENDA_DEFAULT_FONT_PT, pszIni);
    if (pSettings->dwFontSizePt < 8 || pSettings->dwFontSizePt > 14) {
        pSettings->dwFontSizePt = AGENDA_DEFAULT_FONT_PT;
    }

    pSettings->bStartMinimized = GetPrivateProfileIntW(L"Tray", L"StartMinimized", 0, pszIni) != 0;
    pSettings->bMinimizeToTray = GetPrivateProfileIntW(L"Tray", L"MinimizeToTray", 1, pszIni) != 0;

    pSettings->bNotifySoundEnabled = GetPrivateProfileIntW(L"Notifications", L"SoundEnabled", 1, pszIni) != 0;
    GetPrivateProfileStringW(L"Notifications", L"SoundPath", L"", pSettings->szNotifySoundPath, MAX_PATH, pszIni);

    pSettings->bStartWithWindows = GetPrivateProfileIntW(L"System", L"StartWithWindows", 0, pszIni) != 0;
    pSettings->dwNextEventId = (DWORD)GetPrivateProfileIntW(L"Data", L"NextEventId", 1, pszIni);
    pSettings->dwDataVersionCounter = (DWORD)GetPrivateProfileIntW(L"Data", L"DataVersion", 0, pszIni);

    /* Keep startup checkbox accurate against the actual .lnk file on disk */
    pSettings->bStartWithWindows = Settings_IsStartupShortcutPresent();
    return TRUE;
}

BOOL Settings_Save(const APP_SETTINGS* pSettings)
{
    WCHAR szNum[32];
    const WCHAR* pszIni = pSettings->szIniPath;
    BOOL bOk = TRUE;

    bOk &= WritePrivateProfileStringW(L"General", L"DataFolder", pSettings->szDataFolder, pszIni);

    _snwprintf(szNum, 32, L"%u", (UINT)pSettings->dwViewMode);
    szNum[31] = L'\0';
    bOk &= WritePrivateProfileStringW(L"General", L"ViewMode", szNum, pszIni);

    bOk &= WritePrivateProfileStringW(L"General", L"FontName", pSettings->szFontName, pszIni);
    _snwprintf(szNum, 32, L"%u", (UINT)pSettings->dwFontSizePt);
    szNum[31] = L'\0';
    bOk &= WritePrivateProfileStringW(L"General", L"FontSizePt", szNum, pszIni);

    bOk &= WritePrivateProfileStringW(L"Tray", L"StartMinimized", pSettings->bStartMinimized ? L"1" : L"0", pszIni);
    bOk &= WritePrivateProfileStringW(L"Tray", L"MinimizeToTray", pSettings->bMinimizeToTray ? L"1" : L"0", pszIni);

    bOk &= WritePrivateProfileStringW(L"Notifications", L"SoundEnabled", pSettings->bNotifySoundEnabled ? L"1" : L"0", pszIni);
    bOk &= WritePrivateProfileStringW(L"Notifications", L"SoundPath", pSettings->szNotifySoundPath, pszIni);

    bOk &= WritePrivateProfileStringW(L"System", L"StartWithWindows", pSettings->bStartWithWindows ? L"1" : L"0", pszIni);

    _snwprintf(szNum, 32, L"%u", (UINT)pSettings->dwNextEventId);
    bOk &= WritePrivateProfileStringW(L"Data", L"NextEventId", szNum, pszIni);

    _snwprintf(szNum, 32, L"%u", (UINT)pSettings->dwDataVersionCounter);
    bOk &= WritePrivateProfileStringW(L"Data", L"DataVersion", szNum, pszIni);

    return bOk;
}

BOOL Settings_IsStartupShortcutPresent(void)
{
    WCHAR szLnkPath[MAX_PATH];
    BuildShortcutPath(szLnkPath, MAX_PATH);
    if (szLnkPath[0] == L'\0') {
        return FALSE;
    }
    return (GetFileAttributesW(szLnkPath) != INVALID_FILE_ATTRIBUTES);
}

BOOL Settings_SyncStartupShortcut(BOOL bEnable)
{
    WCHAR szLnkPath[MAX_PATH];
    WCHAR szExePath[MAX_PATH];
    WCHAR szWorkDir[MAX_PATH];
    WCHAR* pLastSlash;
    IShellLinkW* pShellLink = NULL;
    IPersistFile* pPersistFile = NULL;
    HRESULT hr;

    BuildShortcutPath(szLnkPath, MAX_PATH);
    if (szLnkPath[0] == L'\0') {
        return FALSE;
    }

    if (!bEnable) {
        if (GetFileAttributesW(szLnkPath) != INVALID_FILE_ATTRIBUTES) {
            return DeleteFileW(szLnkPath);
        }
        return TRUE;
    }

    if (GetModuleFileNameW(NULL, szExePath, MAX_PATH) == 0) {
        return FALSE;
    }

    /* Preserve the current directory containing the INI file as shortcut WorkingDirectory */
    if (GetCurrentDirectoryW(MAX_PATH, szWorkDir) == 0) {
        wcsncpy(szWorkDir, szExePath, MAX_PATH - 1);
        szWorkDir[MAX_PATH - 1] = L'\0';
        pLastSlash = wcsrchr(szWorkDir, L'\\');
        if (pLastSlash) {
            *pLastSlash = L'\0';
        }
    }

    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void**)&pShellLink);
    if (FAILED(hr) || !pShellLink) {
        return FALSE;
    }

    IShellLinkW_SetPath(pShellLink, szExePath);
    IShellLinkW_SetWorkingDirectory(pShellLink, szWorkDir);
    IShellLinkW_SetDescription(pShellLink, AGENDA_APP_NAME);

    hr = IShellLinkW_QueryInterface(pShellLink, &IID_IPersistFile, (void**)&pPersistFile);
    if (SUCCEEDED(hr) && pPersistFile) {
        hr = IPersistFile_Save(pPersistFile, szLnkPath, TRUE);
        IPersistFile_Release(pPersistFile);
    }

    IShellLinkW_Release(pShellLink);
    return SUCCEEDED(hr);
}
