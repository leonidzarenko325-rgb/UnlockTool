#include "BSODMonitor.h"

#include <windows.h>
#include <winevt.h>
#include <shlobj.h>
#include <shellapi.h>

#include <algorithm>
#include <utility>
#include <cwchar>
#include <vector>

#pragma comment(lib, "Wevtapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

namespace BSODMonitor
{
    namespace
    {
        std::wstring ExtractBetween(
            const std::wstring& source,
            const std::wstring& left,
            const std::wstring& right)
        {
            const size_t begin = source.find(left);
            if (begin == std::wstring::npos)
                return L"";
            const size_t valueStart = begin + left.size();
            const size_t end = source.find(right, valueStart);
            if (end == std::wstring::npos)
                return L"";
            return source.substr(valueStart, end - valueStart);
        }

        std::wstring HtmlEscape(const std::wstring& value)
        {
            std::wstring result;
            for (wchar_t ch : value)
            {
                switch (ch)
                {
                case L'&': result += L"&amp;"; break;
                case L'<': result += L"&lt;"; break;
                case L'>': result += L"&gt;"; break;
                case L'\"': result += L"&quot;"; break;
                case L'\'': result += L"&#39;"; break;
                default: result += ch; break;
                }
            }
            return result;
        }

        bool WriteUtf8File(const std::wstring& path, const std::wstring& content)
        {
            const int required = WideCharToMultiByte(
                CP_UTF8, 0, content.c_str(), static_cast<int>(content.size()),
                nullptr, 0, nullptr, nullptr);
            if (required <= 0)
                return false;

            std::vector<char> bytes(static_cast<size_t>(required));
            if (!WideCharToMultiByte(
                CP_UTF8, 0, content.c_str(), static_cast<int>(content.size()),
                bytes.data(), required, nullptr, nullptr))
                return false;

            HANDLE file = CreateFileW(
                path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                return false;

            const BYTE bom[] = { 0xEF, 0xBB, 0xBF };
            DWORD written = 0;
            bool success = WriteFile(file, bom, sizeof(bom), &written, nullptr) &&
                written == sizeof(bom);

            if (success && !bytes.empty())
            {
                success = WriteFile(
                    file, bytes.data(), static_cast<DWORD>(bytes.size()),
                    &written, nullptr) && written == static_cast<DWORD>(bytes.size());
            }

            CloseHandle(file);
            return success;
        }

        std::wstring GetDesktopPath()
        {
            PWSTR path = nullptr;
            if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &path)))
                return L"";
            std::wstring result = path;
            CoTaskMemFree(path);
            return result;
        }

        bool OpenTarget(const wchar_t* target, const wchar_t* parameters = nullptr)
        {
            const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
                nullptr, L"open", target, parameters, nullptr, SW_SHOWNORMAL));
            return result > 32;
        }
    }

    std::vector<Incident> GetIncidents(std::size_t maximum)
    {
        std::vector<Incident> incidents;
        if (maximum == 0)
            return incidents;

        EVT_HANDLE query = EvtQuery(
            nullptr,
            L"System",
            L"*[System[(EventID=1001)]]",
            EvtQueryChannelPath | EvtQueryReverseDirection);

        if (!query)
            return incidents;

        while (incidents.size() < maximum)
        {
            EVT_HANDLE eventHandle = nullptr;
            DWORD returned = 0;
            if (!EvtNext(query, 1, &eventHandle, 0, 0, &returned) || returned == 0)
                break;

            DWORD bufferUsed = 0;
            DWORD propertyCount = 0;
            EvtRender(nullptr, eventHandle, EvtRenderEventXml,
                0, nullptr, &bufferUsed, &propertyCount);

            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && bufferUsed > 0)
            {
                std::vector<wchar_t> buffer(
                    (bufferUsed / sizeof(wchar_t)) + 2, L'\0');
                if (EvtRender(nullptr, eventHandle, EvtRenderEventXml,
                    bufferUsed, buffer.data(), &bufferUsed, &propertyCount))
                {
                    const std::wstring xml(buffer.data());
                    Incident incident{};
                    incident.provider = ExtractBetween(xml, L"<Provider Name=\"", L"\"");
                    incident.timestamp = ExtractBetween(xml, L"<TimeCreated SystemTime=\"", L"\"");
                    incident.bugcheckCode = ExtractBetween(xml, L"<Data Name=\"BugcheckCode\">", L"</Data>");

                    const bool likelyBugcheck =
                        incident.provider.find(L"WER-SystemErrorReporting") != std::wstring::npos ||
                        incident.provider.find(L"BugCheck") != std::wstring::npos ||
                        !incident.bugcheckCode.empty();

                    if (likelyBugcheck)
                    {
                        if (incident.bugcheckCode.empty())
                            incident.bugcheckCode = L"Not supplied";

                        std::wstring p1 = ExtractBetween(xml, L"<Data Name=\"BugcheckParameter1\">", L"</Data>");
                        std::wstring p2 = ExtractBetween(xml, L"<Data Name=\"BugcheckParameter2\">", L"</Data>");
                        std::wstring p3 = ExtractBetween(xml, L"<Data Name=\"BugcheckParameter3\">", L"</Data>");
                        std::wstring p4 = ExtractBetween(xml, L"<Data Name=\"BugcheckParameter4\">", L"</Data>");

                        incident.details = L"Bugcheck code: " + incident.bugcheckCode;
                        if (!p1.empty() || !p2.empty() || !p3.empty() || !p4.empty())
                        {
                            incident.details += L"\r\nParameters: ";
                            incident.details += p1 + L", " + p2 + L", " + p3 + L", " + p4;
                        }
                        incidents.push_back(std::move(incident));
                    }
                }
            }

            EvtClose(eventHandle);
        }

        EvtClose(query);
        return incidents;
    }

    bool ExportReports(std::wstring& textPath, std::wstring& htmlPath)
    {
        const std::wstring desktop = GetDesktopPath();
        if (desktop.empty())
            return false;

        const auto incidents = GetIncidents(100);
        textPath = desktop + L"\\UnlockTool-BSOD-Report.txt";
        htmlPath = desktop + L"\\UnlockTool-BSOD-Report.html";

        SYSTEMTIME generated{};
        GetLocalTime(&generated);
        wchar_t generatedText[64]{};
        swprintf_s(generatedText, L"%04u-%02u-%02u %02u:%02u:%02u",
            generated.wYear, generated.wMonth, generated.wDay,
            generated.wHour, generated.wMinute, generated.wSecond);

        std::wstring text = L"UnlockTool — BSOD report\r\n";
        text += L"Generated: " + std::wstring(generatedText) + L"\r\n";
        text += L"Incidents found: " + std::to_wstring(incidents.size()) + L"\r\n\r\n";

        std::wstring html =
            L"<!doctype html><html><head><meta charset=\"utf-8\"><title>UnlockTool BSOD report</title>"
            L"<style>body{font-family:Segoe UI,Arial;background:#111827;color:#e5e7eb;margin:32px}"
            L"article{background:#1f2937;padding:16px;margin:12px 0;border-radius:10px}"
            L"small{color:#9ca3af}</style></head><body><h1>UnlockTool — BSOD report</h1>";
        html += L"<p>Incidents found: " + std::to_wstring(incidents.size()) + L"</p>";

        if (incidents.empty())
        {
            text += L"No matching BugCheck / WER-SystemErrorReporting events were found.\r\n";
            html += L"<p>No matching BugCheck / WER-SystemErrorReporting events were found.</p>";
        }

        for (size_t i = 0; i < incidents.size(); ++i)
        {
            const auto& item = incidents[i];
            text += L"Incident " + std::to_wstring(i + 1) + L"\r\n";
            text += L"Time: " + item.timestamp + L"\r\n";
            text += L"Provider: " + item.provider + L"\r\n";
            text += item.details + L"\r\n\r\n";

            html += L"<article><h2>Incident " + std::to_wstring(i + 1) + L"</h2>";
            html += L"<p><b>Time:</b> " + HtmlEscape(item.timestamp) + L"</p>";
            html += L"<p><b>Provider:</b> " + HtmlEscape(item.provider) + L"</p>";
            html += L"<pre>" + HtmlEscape(item.details) + L"</pre></article>";
        }

        html += L"</body></html>";
        return WriteUtf8File(textPath, text) && WriteUtf8File(htmlPath, html);
    }

    bool OpenDumpFolder()
    {
        wchar_t windowsDirectory[MAX_PATH]{};
        if (!GetWindowsDirectoryW(windowsDirectory, MAX_PATH))
            return false;

        std::wstring path = windowsDirectory;
        path += L"\\Minidump";
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            !(attributes & FILE_ATTRIBUTE_DIRECTORY))
            path = windowsDirectory;
        return OpenTarget(L"explorer.exe", path.c_str());
    }

    bool OpenEventViewer()
    {
        return OpenTarget(L"eventvwr.msc");
    }

    bool OpenReliabilityMonitor()
    {
        return OpenTarget(L"perfmon.exe", L"/rel");
    }
}
