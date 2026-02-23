#ifndef INCLUDED_ModernUiTheme_H
#define INCLUDED_ModernUiTheme_H

#include <windows.h>
#include <cstddef>

namespace UiBuilder
{
namespace ModernTheme
{
        void applyToDialog(HWND dialog);
        void styleToolbar(HWND dialog, const UINT *controlIds, std::size_t count);
        void applyAccentToControls(HWND dialog, const UINT *controlIds, std::size_t count);

        HBRUSH handleCtlColor(HWND control, HDC hdc, UINT message);
        void paintDialog(HWND dialog, HDC hdc);
        void paintToolbar(HWND dialog, HDC hdc, const RECT &toolbarRect);

        COLORREF textColor();
        COLORREF subtleTextColor();
        COLORREF panelColor();
        COLORREF toolbarColor();
}
}

#endif // INCLUDED_ModernUiTheme_H
