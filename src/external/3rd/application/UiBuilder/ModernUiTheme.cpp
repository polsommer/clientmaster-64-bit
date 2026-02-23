#include "FirstUiBuilder.h"

#include "ModernUiTheme.h"

#include <commctrl.h>
#include <uxtheme.h>

namespace UiBuilder
{
namespace ModernTheme
{
namespace
{
        HFONT   s_dialogFont      = 0;
        HFONT   s_accentFont      = 0;
        HBRUSH  s_dialogBrush     = 0;
        HBRUSH  s_toolbarBrush    = 0;
        COLORREF s_panelColor     = RGB(37, 41, 46);
        COLORREF s_toolbarColor   = RGB(48, 52, 58);
        COLORREF s_textColor      = RGB(240, 240, 240);
        COLORREF s_subtleText     = RGB(200, 204, 208);
        bool     s_initialized    = false;

        typedef HRESULT (WINAPI *SetWindowThemeProc)(HWND, LPCWSTR, LPCWSTR);
        SetWindowThemeProc s_setWindowTheme = 0;

        void ensureInitialized(HWND referenceWindow)
        {
                if (s_initialized)
                        return;

                s_initialized = true;

                s_dialogBrush  = CreateSolidBrush(s_panelColor);
                s_toolbarBrush = CreateSolidBrush(s_toolbarColor);

                HWND refWindow = referenceWindow ? referenceWindow : GetDesktopWindow();
                HDC dc         = GetDC(refWindow);
                const int dpi  = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;

                if (dc)
                        ReleaseDC(refWindow, dc);

                const int fontHeight = -MulDiv(10, dpi, 72);

                s_dialogFont = CreateFont(fontHeight, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Segoe UI"));

                if (!s_dialogFont)
                        s_dialogFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

                LOGFONT lf;
                memset(&lf, 0, sizeof(lf));

                if (GetObject(s_dialogFont, sizeof(lf), &lf) != 0)
                {
                        lf.lfWeight = FW_SEMIBOLD;
                        s_accentFont = CreateFontIndirect(&lf);
                }

                if (!s_accentFont)
                        s_accentFont = s_dialogFont;

                HMODULE uxTheme = LoadLibrary(TEXT("uxtheme.dll"));

                if (uxTheme)
                        s_setWindowTheme = reinterpret_cast<SetWindowThemeProc>(GetProcAddress(uxTheme, "SetWindowTheme"));
        }

        BOOL CALLBACK applyFontProc(HWND hwnd, LPARAM lParam)
        {
                const HFONT font = reinterpret_cast<HFONT>(lParam);
                SendMessage(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

                if (s_setWindowTheme)
                {
                        wchar_t className[64];

                        if (GetClassNameW(hwnd, className, sizeof(className) / sizeof(className[0])))
                        {
                                if (lstrcmpiW(className, L"SysTreeView32") == 0 ||
                                        lstrcmpiW(className, L"SysListView32") == 0 ||
                                        lstrcmpiW(className, L"SysTabControl32") == 0 ||
                                        lstrcmpiW(className, L"SysHeader32") == 0)
                                {
                                        s_setWindowTheme(hwnd, L"Explorer", NULL);
                                }
                        }
                }

                return TRUE;
        }
}

void applyToDialog(HWND dialog)
{
        if (!dialog)
                return;

        ensureInitialized(dialog);

        SendMessage(dialog, WM_SETFONT, reinterpret_cast<WPARAM>(s_dialogFont), TRUE);
        EnumChildWindows(dialog, applyFontProc, reinterpret_cast<LPARAM>(s_dialogFont));
}

void styleToolbar(HWND dialog, const UINT *controlIds, std::size_t count)
{
        if (!dialog || !controlIds)
                return;

        ensureInitialized(dialog);

        for (std::size_t i = 0; i < count; ++i)
        {
                const HWND control = GetDlgItem(dialog, controlIds[i]);

                if (!control)
                        continue;

                SendMessage(control, WM_SETFONT, reinterpret_cast<WPARAM>(s_accentFont), TRUE);

                if (s_setWindowTheme)
                        s_setWindowTheme(control, L"Explorer", NULL);
        }
}

void applyAccentToControls(HWND dialog, const UINT *controlIds, std::size_t count)
{
        if (!dialog || !controlIds)
                return;

        ensureInitialized(dialog);

        for (std::size_t i = 0; i < count; ++i)
        {
                const HWND control = GetDlgItem(dialog, controlIds[i]);

                if (control)
                        SendMessage(control, WM_SETFONT, reinterpret_cast<WPARAM>(s_accentFont), TRUE);
        }
}

HBRUSH handleCtlColor(HWND control, HDC hdc, UINT message)
{
        ensureInitialized(control);

        switch (message)
        {
        case WM_CTLCOLORDLG:
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, s_textColor);
                return s_dialogBrush;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLOREDIT:
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, s_textColor);
                return s_dialogBrush;

        case WM_CTLCOLORBTN:
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, s_textColor);
                return s_toolbarBrush;

        default:
                break;
        }

        return 0;
}

void paintDialog(HWND dialog, HDC hdc)
{
        ensureInitialized(dialog);

        RECT rc;
        GetClientRect(dialog, &rc);
        FillRect(hdc, &rc, s_dialogBrush);
}

void paintToolbar(HWND dialog, HDC hdc, const RECT &toolbarRect)
{
        ensureInitialized(dialog);
        FillRect(hdc, &toolbarRect, s_toolbarBrush);
}

COLORREF textColor()
{
        ensureInitialized(0);
        return s_textColor;
}

COLORREF subtleTextColor()
{
        ensureInitialized(0);
        return s_subtleText;
}

COLORREF panelColor()
{
        ensureInitialized(0);
        return s_panelColor;
}

COLORREF toolbarColor()
{
        ensureInitialized(0);
        return s_toolbarColor;
}

}
}
