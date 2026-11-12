
#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace Cleanup
{
    enum class CleanupType
    {
        UserTemp,
        ThumbnailCache,
        RecycleBin,
        DnsCache
    };

    struct CleanupItem
    {
        std::wstring name;
        std::wstring description;
        std::wstring path;

        uint64_t size = 0;
        unsigned int filesCount = 0;

        uint64_t deletedSize = 0;
        unsigned int deletedFiles = 0;

        bool selected = false;
        bool scanned = false;

        CleanupType type = CleanupType::UserTemp;
    };

    std::vector<CleanupItem> GetCleanupItems();

    bool ScanItem(CleanupItem& item);

    bool CleanItem(CleanupItem& item);
}
