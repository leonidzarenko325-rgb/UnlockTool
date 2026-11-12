#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <string>

namespace UsersPage
{
    void Open(
        HWND hwnd,
        int language,
        std::wstring& statusText
    );

    void Draw(
        Gdiplus::Graphics& graphics,
        int language
    );

    bool HandleClick(
        HWND hwnd,
        int x,
        int y,
        int language,
        std::wstring& statusText
    );
}