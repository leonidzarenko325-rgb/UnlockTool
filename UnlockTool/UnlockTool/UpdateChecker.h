#pragma once

#include <windows.h>
#include <string>

namespace UpdateChecker
{
    struct Result
    {
        bool success = false;
        bool updateAvailable = false;
        bool currentVersionNewer = false;
        std::wstring currentVersion;
        std::wstring latestVersion;
        std::wstring releaseName;
        std::wstring releaseNotes;
        std::wstring releaseUrl;
        std::wstring error;
    };

    // Posts a heap-allocated Result* to hwnd using completionMessage.
    // The receiver owns the pointer and must delete it.
    bool CheckAsync(
        HWND hwnd,
        UINT completionMessage,
        const std::wstring& currentVersion,
        bool interactive);
}
