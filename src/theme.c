#include "theme.h"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
typedef HRESULT (WINAPI *PFN_SetWindowTheme)(HWND, LPCWSTR, LPCWSTR);
typedef BOOL    (WINAPI *PFN_AllowDarkModeForWindow)(HWND, BOOL);
typedef int     (WINAPI *PFN_SetPreferredAppMode)(int);
typedef void    (WINAPI *PFN_FlushMenuThemes)(void);
typedef LONG    (WINAPI *PFN_RtlGetVersion)(OSVERSIONINFOW*);

static DWORD GetWindowsBuildNumber(void)
{
    static DWORD s_dwBuild = 0;
    static BOOL s_bChecked = FALSE;
    if (!s_bChecked) {
        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        s_bChecked = TRUE;
        if (hNtdll) {
            PFN_RtlGetVersion pfnRtl = (PFN_RtlGetVersion)(void*)GetProcAddress(hNtdll, "RtlGetVersion");
            if (pfnRtl) {
                OSVERSIONINFOW ovi;
                ZeroMemory(&ovi, sizeof(ovi));
                ovi.dwOSVersionInfoSize = sizeof(ovi);
                if (pfnRtl(&ovi) == 0) {
                    s_dwBuild = ovi.dwBuildNumber;
                }
            }
        }
    }
    return s_dwBuild;
}

static HMODULE GetUxThemeModule(void)
{
    static HMODULE s_hUx = NULL;
    static BOOL s_bLoaded = FALSE;
    if (!s_bLoaded) {
        s_bLoaded = TRUE;
        s_hUx = LoadLibraryW(L"uxtheme.dll");
    }
    return s_hUx;
}

void Theme_InitProcessDarkMode(BOOL bDarkMode)
{
    HMODULE hUx;
    PFN_SetPreferredAppMode pfnSetMode;
    PFN_FlushMenuThemes pfnFlush;

    if (GetWindowsBuildNumber() < 17763) {
        return;
    }

    hUx = GetUxThemeModule();
    if (!hUx) {
        return;
    }

    /* Ordinal 135: SetPreferredAppMode (1903+) / AllowDarkModeForApp (1809) */
    /* 1 = AllowDark, 3 = ForceLight (1903+) or 0 = Default */
    pfnSetMode = (PFN_SetPreferredAppMode)(void*)GetProcAddress(hUx, MAKEINTRESOURCEA(135));
    if (pfnSetMode) {
        pfnSetMode(bDarkMode ? 1 : 0);
    }

    /* Ordinal 136: FlushMenuThemes */
    pfnFlush = (PFN_FlushMenuThemes)(void*)GetProcAddress(hUx, MAKEINTRESOURCEA(136));
    if (pfnFlush) {
        pfnFlush();
    }
}

BOOL Theme_IsSystemDarkMode(void)
{
    HKEY hKey = NULL;
    DWORD dwVal = 1; /* Default to light if key missing */
    DWORD dwSize = sizeof(DWORD);
    DWORD dwType = REG_DWORD;
    LONG lRes;

    /* Read-only query of Windows system theme preference */
    lRes = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0,
        KEY_READ,
        &hKey
    );
    if (lRes == ERROR_SUCCESS && hKey != NULL) {
        lRes = RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, &dwType, (LPBYTE)&dwVal, &dwSize);
        RegCloseKey(hKey);
        if (lRes == ERROR_SUCCESS && dwType == REG_DWORD) {
            return (dwVal == 0);
        }
    }
    return FALSE;
}

void Theme_BuildPalette(APP_THEME* pTheme, BOOL bDarkMode)
{
    pTheme->bDarkMode = bDarkMode;

    if (bDarkMode) {
        pTheme->crBgWindow      = RGB(24, 25, 28);
        pTheme->crBgSurface     = RGB(36, 38, 44);
        pTheme->crBgHeader      = RGB(38, 40, 46);
        pTheme->crBgGridAlt     = RGB(28, 29, 33);
        pTheme->crBgToday       = RGB(35, 51, 74);
        pTheme->crBgSelected    = RGB(52, 76, 112);
        pTheme->crBgHover       = RGB(48, 51, 58);
        pTheme->crBorderSubtle  = RGB(58, 61, 70);
        pTheme->crBorderStrong  = RGB(82, 86, 98);
        pTheme->crTextPrimary   = RGB(236, 239, 244);
        pTheme->crTextSecondary = RGB(180, 186, 198);
        pTheme->crTextMuted     = RGB(130, 136, 150);
        pTheme->crAccent        = RGB(102, 157, 246);
        pTheme->crAccentText    = RGB(20, 22, 26);
        pTheme->crCurrentTime   = RGB(242, 92, 84);

        pTheme->crEventColors[0]  = RGB(102, 157, 246); /* Bleu */
        pTheme->crEventBgTint[0]  = RGB(38, 57, 87);
        pTheme->crEventColors[1]  = RGB(78, 186, 122);  /* Vert */
        pTheme->crEventBgTint[1]  = RGB(34, 66, 48);
        pTheme->crEventColors[2]  = RGB(235, 97, 88);   /* Rouge */
        pTheme->crEventBgTint[2]  = RGB(79, 39, 38);
        pTheme->crEventColors[3]  = RGB(245, 166, 66);  /* Orange */
        pTheme->crEventBgTint[3]  = RGB(82, 57, 28);
        pTheme->crEventColors[4]  = RGB(174, 129, 245); /* Violet */
        pTheme->crEventBgTint[4]  = RGB(59, 44, 87);
        pTheme->crEventColors[5]  = RGB(64, 194, 188);  /* Sarcelle */
        pTheme->crEventBgTint[5]  = RGB(30, 68, 66);
        pTheme->crEventColors[6]  = RGB(235, 117, 173); /* Rose */
        pTheme->crEventBgTint[6]  = RGB(77, 42, 60);
        pTheme->crEventColors[7]  = RGB(148, 156, 168); /* Gris */
        pTheme->crEventBgTint[7]  = RGB(52, 55, 61);
    } else {
        pTheme->crBgWindow      = RGB(244, 246, 250);
        pTheme->crBgSurface     = RGB(255, 255, 255);
        pTheme->crBgHeader      = RGB(248, 250, 252);
        pTheme->crBgGridAlt     = RGB(250, 251, 253);
        pTheme->crBgToday       = RGB(232, 240, 254);
        pTheme->crBgSelected    = RGB(214, 228, 252);
        pTheme->crBgHover       = RGB(238, 242, 248);
        pTheme->crBorderSubtle  = RGB(224, 228, 235);
        pTheme->crBorderStrong  = RGB(196, 203, 214);
        pTheme->crTextPrimary   = RGB(31, 35, 40);
        pTheme->crTextSecondary = RGB(87, 96, 106);
        pTheme->crTextMuted     = RGB(140, 149, 159);
        pTheme->crAccent        = RGB(26, 115, 232);
        pTheme->crAccentText    = RGB(255, 255, 255);
        pTheme->crCurrentTime   = RGB(217, 48, 37);

        pTheme->crEventColors[0]  = RGB(26, 115, 232);  /* Bleu */
        pTheme->crEventBgTint[0]  = RGB(226, 238, 254);
        pTheme->crEventColors[1]  = RGB(24, 128, 56);   /* Vert */
        pTheme->crEventBgTint[1]  = RGB(224, 243, 230);
        pTheme->crEventColors[2]  = RGB(217, 48, 37);   /* Rouge */
        pTheme->crEventBgTint[2]  = RGB(252, 228, 226);
        pTheme->crEventColors[3]  = RGB(227, 116, 0);   /* Orange */
        pTheme->crEventBgTint[3]  = RGB(254, 238, 218);
        pTheme->crEventColors[4]  = RGB(142, 68, 173);  /* Violet */
        pTheme->crEventBgTint[4]  = RGB(241, 228, 249);
        pTheme->crEventColors[5]  = RGB(0, 137, 123);   /* Sarcelle */
        pTheme->crEventBgTint[5]  = RGB(220, 243, 240);
        pTheme->crEventColors[6]  = RGB(216, 27, 96);   /* Rose */
        pTheme->crEventBgTint[6]  = RGB(252, 226, 236);
        pTheme->crEventColors[7]  = RGB(95, 99, 104);   /* Gris */
        pTheme->crEventBgTint[7]  = RGB(236, 238, 240);
    }
}

void Theme_ApplyToWindow(HWND hwnd, BOOL bDarkMode)
{
    static HMODULE hDwm = NULL;
    static PFN_DwmSetWindowAttribute pfnDwmSet = NULL;
    static BOOL bChecked = FALSE;
    HMODULE hUx;
    BOOL bDarkVal = bDarkMode ? TRUE : FALSE;

    if (!bChecked) {
        bChecked = TRUE;
        hDwm = LoadLibraryW(L"dwmapi.dll");
        if (hDwm) {
            pfnDwmSet = (PFN_DwmSetWindowAttribute)(void*)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        }
    }

    if (pfnDwmSet && hwnd) {
        /* Try Windows 11 / 20H1 attribute 20, fallback to 19 */
        if (FAILED(pfnDwmSet(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &bDarkVal, sizeof(bDarkVal)))) {
            pfnDwmSet(hwnd, 19, &bDarkVal, sizeof(bDarkVal));
        }
    }

    hUx = GetUxThemeModule();
    if (hUx && hwnd && GetWindowsBuildNumber() >= 17763) {
        PFN_AllowDarkModeForWindow pfnAllow =
            (PFN_AllowDarkModeForWindow)(void*)GetProcAddress(hUx, MAKEINTRESOURCEA(133));
        if (pfnAllow) {
            pfnAllow(hwnd, bDarkMode);
        }
    }
}

void Theme_ApplyToControl(HWND hwndCtrl, BOOL bDarkMode, BOOL bIsCfd)
{
    HMODULE hUx;
    PFN_SetWindowTheme pfnSetTheme;
    PFN_AllowDarkModeForWindow pfnAllow;

    if (!hwndCtrl) {
        return;
    }

    hUx = GetUxThemeModule();
    if (!hUx) {
        return;
    }

    if (GetWindowsBuildNumber() >= 17763) {
        pfnAllow = (PFN_AllowDarkModeForWindow)(void*)GetProcAddress(hUx, MAKEINTRESOURCEA(133));
        if (pfnAllow) {
            pfnAllow(hwndCtrl, bDarkMode);
        }
    }

    pfnSetTheme = (PFN_SetWindowTheme)(void*)GetProcAddress(hUx, "SetWindowTheme");
    if (pfnSetTheme) {
        if (bDarkMode) {
            pfnSetTheme(hwndCtrl, bIsCfd ? L"DarkMode_CFD" : L"DarkMode_Explorer", NULL);
        } else {
            pfnSetTheme(hwndCtrl, L"Explorer", NULL);
        }
        SendMessageW(hwndCtrl, WM_THEMECHANGED, 0, 0);
    }
}

HICON Theme_CreateCalendarIcon(DWORD dwSize, BOOL bDarkMode)
{
    BITMAPV5HEADER bi;
    void* pBits = NULL;
    HDC hdcScreen;
    HDC hdcMem;
    HBITMAP hbmColor;
    HBITMAP hbmMask;
    HBITMAP hOldBmp;
    ICONINFO ii;
    HICON hIcon = NULL;
    DWORD* pPixels;
    DWORD x, y;
    DWORD dwHeaderH = dwSize / 3;
    DWORD dwPad = (dwSize >= 32) ? 2 : 1;
    DWORD dwHeaderColor = bDarkMode ? 0xFF669DF6 : 0xFF1A73E8;
    DWORD dwBodyColor   = bDarkMode ? 0xFF282A30 : 0xFFFFFFFF;
    DWORD dwBorderColor = bDarkMode ? 0xFF4A4D58 : 0xFFC8CDD6;
    DWORD dwCellColor   = bDarkMode ? 0xFFE6E8EC : 0xFF3C4043;

    ZeroMemory(&bi, sizeof(bi));
    bi.bV5Size        = sizeof(BITMAPV5HEADER);
    bi.bV5Width       = (LONG)dwSize;
    bi.bV5Height      = -(LONG)dwSize; /* Top-down DIB */
    bi.bV5Planes      = 1;
    bi.bV5BitCount    = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask     = 0x00FF0000;
    bi.bV5GreenMask   = 0x0000FF00;
    bi.bV5BlueMask    = 0x000000FF;
    bi.bV5AlphaMask   = 0xFF000000;

    hdcScreen = GetDC(NULL);
    hbmColor = CreateDIBSection(hdcScreen, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hbmColor || !pBits) {
        ReleaseDC(NULL, hdcScreen);
        return LoadIconW(NULL, IDI_APPLICATION);
    }

    pPixels = (DWORD*)pBits;
    for (y = 0; y < dwSize; ++y) {
        for (x = 0; x < dwSize; ++x) {
            DWORD idx = y * dwSize + x;
            if (x < dwPad || x >= dwSize - dwPad || y < dwPad || y >= dwSize - dwPad) {
                pPixels[idx] = 0x00000000; /* Transparent margin */
            } else if (x == dwPad || x == dwSize - dwPad - 1 || y == dwPad || y == dwSize - dwPad - 1) {
                pPixels[idx] = (y < dwPad + dwHeaderH) ? dwHeaderColor : dwBorderColor;
            } else if (y < dwPad + dwHeaderH) {
                pPixels[idx] = dwHeaderColor;
            } else {
                pPixels[idx] = dwBodyColor;
            }
        }
    }

    hdcMem = CreateCompatibleDC(hdcScreen);
    hOldBmp = (HBITMAP)SelectObject(hdcMem, hbmColor);
    {
        DWORD dwBodyTop = dwPad + dwHeaderH + (dwSize >= 32 ? 4 : 2);
        DWORD dwBodyLeft = dwPad + (dwSize >= 32 ? 5 : 2);
        DWORD dwStepX = (dwSize >= 32 ? 7 : 4);
        DWORD dwStepY = (dwSize >= 32 ? 6 : 3);
        DWORD dwDot = (dwSize >= 32 ? 4 : 2);
        DWORD r, c;
        for (r = 0; r < 2; ++r) {
            for (c = 0; c < 3; ++c) {
                DWORD bx = dwBodyLeft + c * dwStepX;
                DWORD by = dwBodyTop + r * dwStepY;
                DWORD dy, dx;
                DWORD crDot = (r == 0 && c == 1) ? dwHeaderColor : dwCellColor;
                for (dy = 0; dy < dwDot; ++dy) {
                    for (dx = 0; dx < dwDot; ++dx) {
                        if (bx + dx < dwSize - dwPad - 1 && by + dy < dwSize - dwPad - 1) {
                            pPixels[(by + dy) * dwSize + (bx + dx)] = crDot;
                        }
                    }
                }
            }
        }
    }
    SelectObject(hdcMem, hOldBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    hbmMask = CreateBitmap((int)dwSize, (int)dwSize, 1, 1, NULL);

    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = hbmMask;
    hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);

    return hIcon ? hIcon : LoadIconW(NULL, IDI_APPLICATION);
}

static WCHAR s_szDefaultFontName[LF_FACESIZE] = AGENDA_DEFAULT_FONT_NAME;
static DWORD s_dwDefaultFontSizePt = AGENDA_DEFAULT_FONT_PT;

void Theme_SetDefaultFont(const WCHAR* pszFaceName, DWORD dwBaseSizePt)
{
    if (pszFaceName && pszFaceName[0] != L'\0') {
        wcsncpy(s_szDefaultFontName, pszFaceName, LF_FACESIZE - 1);
        s_szDefaultFontName[LF_FACESIZE - 1] = L'\0';
    } else {
        wcsncpy(s_szDefaultFontName, AGENDA_DEFAULT_FONT_NAME, LF_FACESIZE - 1);
        s_szDefaultFontName[LF_FACESIZE - 1] = L'\0';
    }

    if (dwBaseSizePt >= 8 && dwBaseSizePt <= 14) {
        s_dwDefaultFontSizePt = dwBaseSizePt;
    } else {
        s_dwDefaultFontSizePt = AGENDA_DEFAULT_FONT_PT;
    }
}

void Theme_GetDefaultFont(WCHAR* pszFaceNameOut, DWORD cchOut, DWORD* pdwBaseSizePtOut)
{
    if (pszFaceNameOut && cchOut > 0) {
        wcsncpy(pszFaceNameOut, s_szDefaultFontName, cchOut - 1);
        pszFaceNameOut[cchOut - 1] = L'\0';
    }
    if (pdwBaseSizePtOut) {
        *pdwBaseSizePtOut = s_dwDefaultFontSizePt;
    }
}

HFONT Theme_CreateAppFont(UINT uDpi, LONG lDeltaPt, int nWeight, BOOL bItalic)
{
    LOGFONTW lf;
    LONG lPt = (LONG)s_dwDefaultFontSizePt + lDeltaPt;
    if (lPt < 7) {
        lPt = 7;
    }
    if (uDpi == 0) {
        uDpi = 96;
    }

    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -MulDiv((int)lPt, (int)uDpi, 72);
    lf.lfWeight = nWeight;
    lf.lfItalic = bItalic ? 1 : 0;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcsncpy(lf.lfFaceName, s_szDefaultFontName, LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = L'\0';
    return CreateFontIndirectW(&lf);
}
