#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace TaskManager
{
    struct ProcessInfo
    {
        DWORD pid = 0;

        std::wstring name;
        std::wstring exePath;

        uint64_t memoryUsage = 0;
        bool memoryAvailable = false;

        double cpuUsage = 0.0;
        bool cpuAvailable = false;

        bool isResponding = true;
        bool isCritical = false;

        bool isSuspended = false;
        bool suspensionStateAvailable = false;
    };

    std::vector<ProcessInfo> GetProcesses();

    bool GetProcessInfo(DWORD pid, ProcessInfo& info);

    bool EndProcess(DWORD pid);

    bool SuspendProcess(DWORD pid);

    bool ResumeProcess(DWORD pid);

    std::vector<ProcessInfo> SearchProcesses(
        const std::vector<ProcessInfo>& processes,
        const std::wstring& query
    );
}