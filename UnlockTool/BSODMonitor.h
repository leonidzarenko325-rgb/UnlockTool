#pragma once

#include <string>
#include <cstddef>
#include <vector>

namespace BSODMonitor
{
    struct Incident
    {
        std::wstring timestamp;
        std::wstring bugcheckCode;
        std::wstring details;
        std::wstring provider;
    };

    std::vector<Incident> GetIncidents(std::size_t maximum = 30);
    bool ExportReports(std::wstring& textPath, std::wstring& htmlPath);
    bool OpenDumpFolder();
    bool OpenEventViewer();
    bool OpenReliabilityMonitor();
}
