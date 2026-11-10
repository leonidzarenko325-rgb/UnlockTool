#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace DiskManager
{
    struct VolumeInfo
    {
        std::wstring root;
        std::wstring label;
        std::wstring fileSystem;
        std::wstring driveType;
        std::wstring model;
        std::wstring partitionStyle;
        std::wstring health;

        uint64_t totalBytes = 0;
        uint64_t freeBytes = 0;
        DWORD diskNumber = MAXDWORD;
        bool ready = false;
    };

    std::vector<VolumeInfo> EnumerateVolumes();
    bool OpenVolume(const VolumeInfo& volume);
    bool RunFileSystemScan(const VolumeInfo& volume);
    bool OpenDiskManagement();
    bool ShowBitLockerStatus(const VolumeInfo& volume);
}
