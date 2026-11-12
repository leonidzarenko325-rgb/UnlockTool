
#include "TaskManager.h"

#include <tlhelp32.h>
#include <psapi.h>

#include <algorithm>
#include <cwctype>
#include <unordered_map>

#pragma comment(lib, "Psapi.lib")

namespace TaskManager
{
    struct WindowCheckData
    {
        DWORD pid = 0;
        bool responding = true;
    };

    struct ProcessCpuSample
    {
        uint64_t creationTime = 0;
        uint64_t kernelTime = 0;
        uint64_t userTime = 0;
    };

    struct SuspendedProcess
    {
        uint64_t creationTime = 0;
    };

    using NtProcessFunction = LONG(NTAPI*)(HANDLE);

    static std::unordered_map<DWORD, ProcessCpuSample> cpuSamples;
    static std::unordered_map<DWORD, SuspendedProcess> suspendedProcesses;

    static uint64_t previousSystemTime = 0;
    static bool hasPreviousSystemTime = false;

    uint64_t FileTimeToUInt64(const FILETIME& time)
    {
        ULARGE_INTEGER value{};
        value.LowPart = time.dwLowDateTime;
        value.HighPart = time.dwHighDateTime;
        return value.QuadPart;
    }

    bool GetCreationTime(HANDLE process, uint64_t& creationTime)
    {
        FILETIME creation{}, exit{}, kernel{}, user{};

        if (!GetProcessTimes(process, &creation, &exit, &kernel, &user))
            return false;

        creationTime = FileTimeToUInt64(creation);
        return true;
    }

    std::wstring ToLower(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), towlower);
        return value;
    }

    std::wstring GetProcessNameFromPath(const std::wstring& path)
    {
        size_t position = path.find_last_of(L"\\/");

        if (position == std::wstring::npos)
            return ToLower(path);

        return ToLower(path.substr(position + 1));
    }

    BOOL CALLBACK CheckProcessWindow(HWND hwnd, LPARAM lParam)
    {
        auto* data = reinterpret_cast<WindowCheckData*>(lParam);

        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);

        if (windowPid == data->pid && IsWindowVisible(hwnd))
        {
            if (IsHungAppWindow(hwnd))
            {
                data->responding = false;
                return FALSE;
            }
        }

        return TRUE;
    }

    bool IsProcessResponding(DWORD pid)
    {
        WindowCheckData data;
        data.pid = pid;

        EnumWindows(
            CheckProcessWindow,
            reinterpret_cast<LPARAM>(&data)
        );

        return data.responding;
    }

    bool IsCriticalProcess(DWORD pid, const std::wstring& name)
    {
        static const std::vector<std::wstring> criticalProcesses =
        {
            L"system",
            L"smss.exe",
            L"csrss.exe",
            L"wininit.exe",
            L"winlogon.exe",
            L"services.exe",
            L"lsass.exe",
            L"svchost.exe",
            L"registry",
            L"memory compression"
        };

        std::wstring lowerName = ToLower(name);

        if (pid == 0 || pid == 4)
            return true;

        if (pid == GetCurrentProcessId())
            return true;

        return std::find(
            criticalProcesses.begin(),
            criticalProcesses.end(),
            lowerName
        ) != criticalProcesses.end();
    }

    bool IsWindowsCritical(HANDLE process)
    {
        using IsProcessCriticalFunction = BOOL(WINAPI*)(HANDLE, PBOOL);

        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");

        if (!kernel32)
            return false;

        auto function =
            reinterpret_cast<IsProcessCriticalFunction>(
                GetProcAddress(kernel32, "IsProcessCritical")
                );

        if (!function)
            return false;

        BOOL critical = FALSE;

        return function(process, &critical) && critical;
    }

    bool IsProtectedProcess(
        HANDLE process,
        DWORD pid,
        const std::wstring& processName
    )
    {
        if (!process)
            return true;

        // Защита системных PID и самого UnlockTool.
        if (pid == 0 || pid == 4 || pid == GetCurrentProcessId())
            return true;

        // Защита известных критичных процессов.
        if (IsCriticalProcess(pid, processName))
            return true;

        // Дополнительная проверка средствами Windows.
        if (IsWindowsCritical(process))
            return true;

        return false;
    }

    bool GetActualProcessName(HANDLE process, std::wstring& processName)
    {
        wchar_t path[MAX_PATH * 4]{};
        DWORD pathSize = MAX_PATH * 4;

        if (!QueryFullProcessImageNameW(
            process,
            0,
            path,
            &pathSize))
        {
            return false;
        }

        processName = GetProcessNameFromPath(path);
        return true;
    }

    void UpdateCpuUsage(
        ProcessInfo& info,
        HANDLE process,
        uint64_t systemDelta
    )
    {
        FILETIME creationTime{}, exitTime{}, kernelTime{}, userTime{};

        if (!GetProcessTimes(
            process,
            &creationTime,
            &exitTime,
            &kernelTime,
            &userTime))
        {
            cpuSamples.erase(info.pid);
            info.cpuAvailable = false;
            return;
        }

        ProcessCpuSample current{};

        current.creationTime = FileTimeToUInt64(creationTime);
        current.kernelTime = FileTimeToUInt64(kernelTime);
        current.userTime = FileTimeToUInt64(userTime);

        auto previous = cpuSamples.find(info.pid);

        if (previous != cpuSamples.end() &&
            previous->second.creationTime == current.creationTime &&
            systemDelta > 0)
        {
            uint64_t previousProcessTime =
                previous->second.kernelTime +
                previous->second.userTime;

            uint64_t currentProcessTime =
                current.kernelTime +
                current.userTime;

            if (currentProcessTime >= previousProcessTime)
            {
                uint64_t processDelta =
                    currentProcessTime - previousProcessTime;

                info.cpuUsage =
                    (static_cast<double>(processDelta) /
                        static_cast<double>(systemDelta)) * 100.0;

                info.cpuUsage =
                    (std::max)(0.0, (std::min)(100.0, info.cpuUsage));

                info.cpuAvailable = true;
            }
        }

        cpuSamples[info.pid] = current;
    }

    void UpdateSuspensionState(ProcessInfo& info, HANDLE process)
    {
        info.suspensionStateAvailable = true;
        info.isSuspended = false;

        uint64_t creationTime = 0;

        if (!GetCreationTime(process, creationTime))
        {
            info.suspensionStateAvailable = false;
            return;
        }

        auto found = suspendedProcesses.find(info.pid);

        if (found == suspendedProcesses.end())
            return;

        if (found->second.creationTime != creationTime)
        {
            suspendedProcesses.erase(found);
            return;
        }

        info.isSuspended = true;
    }

    void FillProcessDetails(ProcessInfo& info, uint64_t systemDelta)
    {
        HANDLE queryProcess = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            info.pid
        );

        if (queryProcess)
        {
            wchar_t path[MAX_PATH * 4]{};
            DWORD pathSize = MAX_PATH * 4;

            if (QueryFullProcessImageNameW(
                queryProcess,
                0,
                path,
                &pathSize))
            {
                info.exePath = path;
            }

            UpdateCpuUsage(info, queryProcess, systemDelta);
            UpdateSuspensionState(info, queryProcess);

            info.isCritical =
                IsCriticalProcess(info.pid, info.name) ||
                IsWindowsCritical(queryProcess);

            CloseHandle(queryProcess);
        }
        else
        {
            cpuSamples.erase(info.pid);
            info.cpuAvailable = false;
            info.suspensionStateAvailable = false;

            info.isCritical = IsCriticalProcess(info.pid, info.name);
        }

        HANDLE memoryProcess = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            FALSE,
            info.pid
        );

        if (!memoryProcess)
        {
            memoryProcess = OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                FALSE,
                info.pid
            );
        }

        if (memoryProcess)
        {
            PROCESS_MEMORY_COUNTERS_EX memoryInfo{};
            memoryInfo.cb = sizeof(memoryInfo);

            if (GetProcessMemoryInfo(
                memoryProcess,
                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryInfo),
                sizeof(memoryInfo)))
            {
                info.memoryUsage =
                    static_cast<uint64_t>(memoryInfo.WorkingSetSize);

                info.memoryAvailable = true;
            }

            CloseHandle(memoryProcess);
        }

        info.isResponding = IsProcessResponding(info.pid);
    }

    std::vector<ProcessInfo> GetProcesses()
    {
        std::vector<ProcessInfo> processes;

        FILETIME idleTime{}, kernelTime{}, userTime{};
        uint64_t systemDelta = 0;

        if (GetSystemTimes(&idleTime, &kernelTime, &userTime))
        {
            uint64_t currentSystemTime =
                FileTimeToUInt64(kernelTime) +
                FileTimeToUInt64(userTime);

            if (hasPreviousSystemTime &&
                currentSystemTime > previousSystemTime)
            {
                systemDelta = currentSystemTime - previousSystemTime;
            }

            previousSystemTime = currentSystemTime;
            hasPreviousSystemTime = true;
        }

        HANDLE snapshot = CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0
        );

        if (snapshot == INVALID_HANDLE_VALUE)
            return processes;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                ProcessInfo info;

                info.pid = entry.th32ProcessID;
                info.name = entry.szExeFile;

                FillProcessDetails(info, systemDelta);
                processes.push_back(info);

            } while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return processes;
    }

    bool GetProcessInfo(DWORD pid, ProcessInfo& info)
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0
        );

        if (snapshot == INVALID_HANDLE_VALUE)
            return false;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(PROCESSENTRY32W);

        bool found = false;

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (entry.th32ProcessID == pid)
                {
                    info.pid = entry.th32ProcessID;
                    info.name = entry.szExeFile;
                    found = true;
                    break;
                }

            } while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);

        if (!found)
            return false;

        FillProcessDetails(info, 0);
        return true;
    }

    std::vector<ProcessInfo> SearchProcesses(
        const std::vector<ProcessInfo>& processes,
        const std::wstring& query
    )
    {
        std::vector<ProcessInfo> results;
        std::wstring searchQuery = ToLower(query);

        for (const auto& process : processes)
        {
            std::wstring processName = ToLower(process.name);
            std::wstring processPid = std::to_wstring(process.pid);

            if (searchQuery.empty() ||
                processName.find(searchQuery) != std::wstring::npos ||
                processPid.find(searchQuery) != std::wstring::npos)
            {
                results.push_back(process);
            }
        }

        return results;
    }

    bool EndProcess(DWORD pid)
    {
        // Не разрешаем завершать системные PID и сам UnlockTool.
        if (pid == 0 || pid == 4 || pid == GetCurrentProcessId())
            return false;

        HANDLE process = OpenProcess(
            PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            pid
        );

        if (!process)
            return false;

        std::wstring actualName;

        if (!GetActualProcessName(process, actualName))
        {
            CloseHandle(process);
            return false;
        }

        // Повторная проверка защиты непосредственно перед завершением.
        // Критичные процессы Windows завершать запрещено.
        if (IsProtectedProcess(process, pid, actualName))
        {
            CloseHandle(process);
            return false;
        }

        uint64_t creationTime = 0;

        if (!GetCreationTime(process, creationTime))
        {
            CloseHandle(process);
            return false;
        }

        bool result = TerminateProcess(process, 1) != FALSE;

        CloseHandle(process);

        if (result)
        {
            auto found = suspendedProcesses.find(pid);

            if (found != suspendedProcesses.end() &&
                found->second.creationTime == creationTime)
            {
                suspendedProcesses.erase(found);
            }

            cpuSamples.erase(pid);
        }

        return result;
    }

    bool SuspendProcess(DWORD pid)
    {
        HANDLE process = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION |
            PROCESS_SUSPEND_RESUME,
            FALSE,
            pid
        );

        if (!process)
            return false;

        std::wstring actualName;

        if (!GetActualProcessName(process, actualName) ||
            IsProtectedProcess(process, pid, actualName))
        {
            CloseHandle(process);
            return false;
        }

        uint64_t creationTime = 0;

        if (!GetCreationTime(process, creationTime))
        {
            CloseHandle(process);
            return false;
        }

        auto existing = suspendedProcesses.find(pid);

        if (existing != suspendedProcesses.end() &&
            existing->second.creationTime == creationTime)
        {
            CloseHandle(process);
            return false;
        }

        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");

        if (!ntdll)
        {
            CloseHandle(process);
            return false;
        }

        auto ntSuspendProcess =
            reinterpret_cast<NtProcessFunction>(
                GetProcAddress(ntdll, "NtSuspendProcess")
                );

        if (!ntSuspendProcess)
        {
            CloseHandle(process);
            return false;
        }

        LONG result = ntSuspendProcess(process);
        CloseHandle(process);

        if (result < 0)
            return false;

        suspendedProcesses[pid] = { creationTime };
        return true;
    }

    bool ResumeProcess(DWORD pid)
    {
        auto existing = suspendedProcesses.find(pid);

        if (existing == suspendedProcesses.end())
            return false;

        HANDLE process = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION |
            PROCESS_SUSPEND_RESUME,
            FALSE,
            pid
        );

        if (!process)
        {
            suspendedProcesses.erase(existing);
            return false;
        }

        uint64_t creationTime = 0;

        if (!GetCreationTime(process, creationTime) ||
            creationTime != existing->second.creationTime)
        {
            CloseHandle(process);
            suspendedProcesses.erase(existing);
            return false;
        }

        std::wstring actualName;

        if (!GetActualProcessName(process, actualName) ||
            IsProtectedProcess(process, pid, actualName))
        {
            CloseHandle(process);
            return false;
        }

        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");

        if (!ntdll)
        {
            CloseHandle(process);
            return false;
        }

        auto ntResumeProcess =
            reinterpret_cast<NtProcessFunction>(
                GetProcAddress(ntdll, "NtResumeProcess")
                );

        if (!ntResumeProcess)
        {
            CloseHandle(process);
            return false;
        }

        LONG result = ntResumeProcess(process);
        CloseHandle(process);

        if (result < 0)
            return false;

        suspendedProcesses.erase(pid);
        return true;
    }
}
