#include "DiskManager.h"

#include <windows.h>
#include <winioctl.h>
#include <ntddstor.h>
#include <shellapi.h>

#include <algorithm>
#include <cwchar>
#include <utility>
#include <vector>

#pragma comment(lib, "Shell32.lib")

namespace DiskManager
{
    namespace
    {
        std::wstring GetDriveTypeText(UINT type)
        {
            switch (type)
            {
            case DRIVE_FIXED: return L"Fixed";
            case DRIVE_REMOVABLE: return L"Removable";
            case DRIVE_REMOTE: return L"Network";
            case DRIVE_CDROM: return L"Optical";
            case DRIVE_RAMDISK: return L"RAM disk";
            default: return L"Unknown";
            }
        }

        std::wstring ReadDiskModel(DWORD diskNumber)
        {
            if (diskNumber == MAXDWORD)
                return L"Unknown";

            wchar_t devicePath[64]{};
            swprintf_s(devicePath, L"\\\\.\\PhysicalDrive%lu", diskNumber);

            HANDLE disk = CreateFileW(
                devicePath, 0,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, 0, nullptr);

            if (disk == INVALID_HANDLE_VALUE)
                return L"Access unavailable";

            STORAGE_PROPERTY_QUERY query{};
            query.PropertyId = StorageDeviceProperty;
            query.QueryType = PropertyStandardQuery;

            std::vector<BYTE> buffer(4096);
            DWORD returned = 0;
            std::wstring model = L"Unknown";

            if (DeviceIoControl(
                disk, IOCTL_STORAGE_QUERY_PROPERTY,
                &query, sizeof(query),
                buffer.data(), static_cast<DWORD>(buffer.size()),
                &returned, nullptr) &&
                returned >= sizeof(STORAGE_DEVICE_DESCRIPTOR))
            {
                const auto* descriptor =
                    reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());

                if (descriptor->ProductIdOffset > 0 &&
                    descriptor->ProductIdOffset < returned)
                {
                    const char* value = reinterpret_cast<const char*>(
                        buffer.data() + descriptor->ProductIdOffset);
                    wchar_t wide[256]{};
                    MultiByteToWideChar(CP_ACP, 0, value, -1, wide, 256);
                    if (wide[0])
                        model = wide;
                }

                if (model == L"Unknown" && descriptor->VendorIdOffset > 0 &&
                    descriptor->VendorIdOffset < returned)
                {
                    const char* value = reinterpret_cast<const char*>(
                        buffer.data() + descriptor->VendorIdOffset);
                    wchar_t wide[256]{};
                    MultiByteToWideChar(CP_ACP, 0, value, -1, wide, 256);
                    if (wide[0])
                        model = wide;
                }
            }

            CloseHandle(disk);
            return model;
        }

        std::wstring ReadDiskHealth(DWORD diskNumber)
        {
            if (diskNumber == MAXDWORD)
                return L"Unknown";

            wchar_t devicePath[64]{};
            swprintf_s(devicePath, L"\\\\.\\PhysicalDrive%lu", diskNumber);
            HANDLE disk = CreateFileW(
                devicePath, GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, 0, nullptr);

            if (disk == INVALID_HANDLE_VALUE)
                return L"Access unavailable";

            STORAGE_PREDICT_FAILURE prediction{};
            DWORD returned = 0;
            const BOOL queried = DeviceIoControl(
                disk, IOCTL_STORAGE_PREDICT_FAILURE,
                nullptr, 0,
                &prediction, sizeof(prediction),
                &returned, nullptr);
            CloseHandle(disk);

            if (!queried || returned < sizeof(prediction))
                return L"Not supported / unknown";

            return prediction.PredictFailure
                ? L"Failure predicted"
                : L"No failure predicted";
        }

        std::wstring ReadPartitionStyle(DWORD diskNumber)
        {
            if (diskNumber == MAXDWORD)
                return L"Unknown";

            wchar_t devicePath[64]{};
            swprintf_s(devicePath, L"\\\\.\\PhysicalDrive%lu", diskNumber);
            HANDLE disk = CreateFileW(
                devicePath, 0,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, 0, nullptr);

            if (disk == INVALID_HANDLE_VALUE)
                return L"Access unavailable";

            std::vector<BYTE> buffer(64 * 1024);
            DWORD returned = 0;
            std::wstring style = L"Unknown";

            if (DeviceIoControl(
                disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
                nullptr, 0,
                buffer.data(), static_cast<DWORD>(buffer.size()),
                &returned, nullptr) &&
                returned >= sizeof(DRIVE_LAYOUT_INFORMATION_EX))
            {
                const auto* layout =
                    reinterpret_cast<const DRIVE_LAYOUT_INFORMATION_EX*>(buffer.data());

                switch (layout->PartitionStyle)
                {
                case PARTITION_STYLE_GPT: style = L"GPT"; break;
                case PARTITION_STYLE_MBR: style = L"MBR"; break;
                case PARTITION_STYLE_RAW: style = L"RAW / Uninitialized"; break;
                default: style = L"Unknown"; break;
                }
            }

            CloseHandle(disk);
            return style;
        }

        DWORD GetPhysicalDiskNumber(const std::wstring& root)
        {
            const std::wstring volumeDevice = L"\\\\.\\" + root.substr(0, 2);
            HANDLE volume = CreateFileW(
                volumeDevice.c_str(), 0,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, 0, nullptr);

            if (volume == INVALID_HANDLE_VALUE)
                return MAXDWORD;

            std::vector<BYTE> buffer(
                sizeof(VOLUME_DISK_EXTENTS) + sizeof(DISK_EXTENT) * 7);
            DWORD returned = 0;
            DWORD diskNumber = MAXDWORD;

            if (DeviceIoControl(
                volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                nullptr, 0,
                buffer.data(), static_cast<DWORD>(buffer.size()),
                &returned, nullptr) &&
                returned >= sizeof(VOLUME_DISK_EXTENTS))
            {
                const auto* extents =
                    reinterpret_cast<const VOLUME_DISK_EXTENTS*>(buffer.data());
                if (extents->NumberOfDiskExtents > 0)
                    diskNumber = extents->Extents[0].DiskNumber;
            }

            CloseHandle(volume);
            return diskNumber;
        }

        std::wstring RunElevated(const std::wstring& parameters)
        {
            const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
                nullptr, L"runas", L"cmd.exe", parameters.c_str(),
                nullptr, SW_SHOWNORMAL));
            return result > 32 ? L"OK" : L"FAILED";
        }
    }

    std::vector<VolumeInfo> EnumerateVolumes()
    {
        std::vector<VolumeInfo> volumes;
        const DWORD mask = GetLogicalDrives();
        if (!mask)
            return volumes;

        for (int index = 0; index < 26; ++index)
        {
            if (!(mask & (1u << index)))
                continue;

            wchar_t rootBuffer[] = { static_cast<wchar_t>(L'A' + index), L':', L'\\', L'\0' };
            const UINT type = GetDriveTypeW(rootBuffer);

            VolumeInfo info{};
            info.root = rootBuffer;
            info.driveType = GetDriveTypeText(type);

            wchar_t label[MAX_PATH]{};
            wchar_t fileSystem[MAX_PATH]{};
            DWORD serial = 0, maxComponent = 0, flags = 0;

            if (!GetVolumeInformationW(
                rootBuffer, label, MAX_PATH, &serial,
                &maxComponent, &flags, fileSystem, MAX_PATH))
            {
                info.label = L"No media / inaccessible";
                info.fileSystem = L"—";
                info.health = L"Not available";
                info.ready = false;
                volumes.push_back(std::move(info));
                continue;
            }

            info.label = label[0] ? label : L"Local volume";
            info.fileSystem = fileSystem;

            ULARGE_INTEGER available{}, total{}, free{};
            if (GetDiskFreeSpaceExW(rootBuffer, &available, &total, &free))
            {
                info.totalBytes = total.QuadPart;
                info.freeBytes = free.QuadPart;
            }

            info.diskNumber = GetPhysicalDiskNumber(info.root);
            info.model = ReadDiskModel(info.diskNumber);
            info.partitionStyle = ReadPartitionStyle(info.diskNumber);
            info.health = ReadDiskHealth(info.diskNumber);
            info.ready = true;

            volumes.push_back(std::move(info));
        }

        return volumes;
    }

    bool OpenVolume(const VolumeInfo& volume)
    {
        if (volume.root.empty())
            return false;
        const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
            nullptr, L"open", volume.root.c_str(), nullptr,
            nullptr, SW_SHOWNORMAL));
        return result > 32;
    }

    bool RunFileSystemScan(const VolumeInfo& volume)
    {
        if (volume.root.size() < 2 || volume.root[1] != L':')
            return false;
        const std::wstring command = L"/k chkdsk " + volume.root.substr(0, 2) + L" /scan";
        return RunElevated(command) == L"OK";
    }

    bool OpenDiskManagement()
    {
        const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
            nullptr, L"open", L"diskmgmt.msc", nullptr,
            nullptr, SW_SHOWNORMAL));
        return result > 32;
    }

    bool ShowBitLockerStatus(const VolumeInfo& volume)
    {
        if (volume.root.size() < 2 || volume.root[1] != L':')
            return false;
        const std::wstring command = L"/k manage-bde -status " + volume.root.substr(0, 2);
        return RunElevated(command) == L"OK";
    }
}
