#include "UpdateChecker.h"

#include <winhttp.h>
#include <algorithm>
#include <cwctype>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace
{
    constexpr wchar_t GITHUB_HOST[] = L"api.github.com";
    constexpr wchar_t RELEASE_PATH[] =
        L"/repos/leonidzarenko325-rgb/UnlockTool/releases/latest";

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
            return {};

        const int length = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()),
            nullptr, 0);

        if (length <= 0)
            return {};

        std::wstring result(static_cast<size_t>(length), L'\0');

        if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()),
            result.data(), length) <= 0)
        {
            return {};
        }

        return result;
    }

    void AppendCodePoint(std::wstring& output, unsigned int codePoint)
    {
        if constexpr (sizeof(wchar_t) == 2)
        {
            if (codePoint <= 0xFFFF)
            {
                output.push_back(static_cast<wchar_t>(codePoint));
            }
            else
            {
                codePoint -= 0x10000;
                output.push_back(static_cast<wchar_t>(
                    0xD800 + (codePoint >> 10)));
                output.push_back(static_cast<wchar_t>(
                    0xDC00 + (codePoint & 0x3FF)));
            }
        }
        else
        {
            output.push_back(static_cast<wchar_t>(codePoint));
        }
    }

    int HexValue(wchar_t ch)
    {
        if (ch >= L'0' && ch <= L'9') return ch - L'0';
        if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
        if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
        return -1;
    }

    bool ReadHex4(const std::wstring& text, size_t position, unsigned int& value)
    {
        if (position + 4 > text.size())
            return false;

        value = 0;
        for (size_t i = 0; i < 4; ++i)
        {
            const int digit = HexValue(text[position + i]);
            if (digit < 0)
                return false;
            value = (value << 4) | static_cast<unsigned int>(digit);
        }
        return true;
    }

    std::wstring GetJsonString(const std::wstring& json, const std::wstring& key)
    {
        const std::wstring token = L"\"" + key + L"\"";
        size_t position = json.find(token);
        if (position == std::wstring::npos)
            return {};

        position = json.find(L':', position + token.size());
        if (position == std::wstring::npos)
            return {};

        ++position;
        while (position < json.size() && std::iswspace(json[position]))
            ++position;

        if (position >= json.size() || json[position] != L'"')
            return {};

        ++position;
        std::wstring output;

        while (position < json.size())
        {
            const wchar_t ch = json[position++];

            if (ch == L'"')
                return output;

            if (ch != L'\\')
            {
                output.push_back(ch);
                continue;
            }

            if (position >= json.size())
                return {};

            const wchar_t escaped = json[position++];

            switch (escaped)
            {
            case L'"': output.push_back(L'"'); break;
            case L'\\': output.push_back(L'\\'); break;
            case L'/': output.push_back(L'/'); break;
            case L'b': output.push_back(L'\b'); break;
            case L'f': output.push_back(L'\f'); break;
            case L'n': output.push_back(L'\n'); break;
            case L'r': output.push_back(L'\r'); break;
            case L't': output.push_back(L'\t'); break;
            case L'u':
            {
                unsigned int codePoint = 0;
                if (!ReadHex4(json, position, codePoint))
                    return {};
                position += 4;

                if (codePoint >= 0xD800 && codePoint <= 0xDBFF &&
                    position + 6 <= json.size() &&
                    json[position] == L'\\' &&
                    json[position + 1] == L'u')
                {
                    unsigned int low = 0;
                    if (ReadHex4(json, position + 2, low) &&
                        low >= 0xDC00 && low <= 0xDFFF)
                    {
                        codePoint = 0x10000 +
                            ((codePoint - 0xD800) << 10) +
                            (low - 0xDC00);
                        position += 6;
                    }
                }

                AppendCodePoint(output, codePoint);
                break;
            }
            default:
                return {};
            }
        }

        return {};
    }

    std::vector<unsigned long long> ParseVersion(const std::wstring& version)
    {
        std::wstring normalized = version;
        while (!normalized.empty() &&
            (normalized.front() == L'v' || normalized.front() == L'V'))
        {
            normalized.erase(normalized.begin());
        }

        std::vector<unsigned long long> parts;
        size_t position = 0;

        while (position < normalized.size())
        {
            while (position < normalized.size() &&
                !std::iswdigit(normalized[position]))
            {
                if (normalized[position] == L'.')
                    break;
                ++position;
            }

            if (position >= normalized.size() || normalized[position] == L'.')
            {
                if (position < normalized.size() && normalized[position] == L'.')
                {
                    parts.push_back(0);
                    ++position;
                    continue;
                }
                break;
            }

            unsigned long long number = 0;
            while (position < normalized.size() &&
                std::iswdigit(normalized[position]))
            {
                const unsigned int digit =
                    static_cast<unsigned int>(normalized[position] - L'0');

                if (number <= (ULLONG_MAX - digit) / 10)
                    number = number * 10 + digit;

                ++position;
            }

            parts.push_back(number);

            if (position < normalized.size() && normalized[position] == L'.')
                ++position;
            else
                break;
        }

        while (!parts.empty() && parts.back() == 0)
            parts.pop_back();

        return parts;
    }

    bool IsNewerVersion(
        const std::wstring& currentVersion,
        const std::wstring& latestVersion)
    {
        const auto current = ParseVersion(currentVersion);
        const auto latest = ParseVersion(latestVersion);

        if (latest.empty())
            return false;

        const size_t count = (std::max)(current.size(), latest.size());

        for (size_t i = 0; i < count; ++i)
        {
            const unsigned long long currentPart =
                i < current.size() ? current[i] : 0;
            const unsigned long long latestPart =
                i < latest.size() ? latest[i] : 0;

            if (latestPart > currentPart)
                return true;
            if (latestPart < currentPart)
                return false;
        }

        return false;
    }

    bool DownloadLatestRelease(std::wstring& response, std::wstring& error)
    {
        HINTERNET session = WinHttpOpen(
            L"UnlockTool Update Checker/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (!session)
        {
            error = L"Не удалось инициализировать WinHTTP.";
            return false;
        }

        WinHttpSetTimeouts(session, 5000, 5000, 10000, 10000);

        HINTERNET connection = WinHttpConnect(
            session, GITHUB_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);

        if (!connection)
        {
            error = L"Не удалось подключиться к GitHub.";
            WinHttpCloseHandle(session);
            return false;
        }

        HINTERNET request = WinHttpOpenRequest(
            connection,
            L"GET",
            RELEASE_PATH,
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);

        if (!request)
        {
            error = L"Не удалось создать HTTP-запрос.";
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return false;
        }

        const wchar_t headers[] =
            L"Accept: application/vnd.github+json\r\n"
            L"X-GitHub-Api-Version: 2022-11-28\r\n";

        bool success = false;

        if (WinHttpSendRequest(
            request,
            headers,
            static_cast<DWORD>(-1L),
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0) &&
            WinHttpReceiveResponse(request, nullptr))
        {
            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);

            if (WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusSize,
                WINHTTP_NO_HEADER_INDEX))
            {
                if (statusCode == HTTP_STATUS_OK)
                {
                    std::string body;
                    DWORD available = 0;

                    while (WinHttpQueryDataAvailable(request, &available) &&
                        available > 0)
                    {
                        if (body.size() + available > 4 * 1024 * 1024)
                        {
                            error = L"Ответ GitHub слишком большой.";
                            break;
                        }

                        std::vector<char> buffer(available);
                        DWORD downloaded = 0;

                        if (!WinHttpReadData(
                            request,
                            buffer.data(),
                            available,
                            &downloaded))
                        {
                            error = L"Не удалось прочитать ответ GitHub.";
                            break;
                        }

                        if (downloaded == 0)
                            break;

                        body.append(buffer.data(), downloaded);
                    }

                    if (error.empty())
                    {
                        response = Utf8ToWide(body);
                        success = !response.empty();
                        if (!success)
                            error = L"GitHub вернул пустой или некорректный ответ.";
                    }
                }
                else if (statusCode == 404)
                {
                    error = L"В репозитории пока нет опубликованного GitHub Release.";
                }
                else if (statusCode == 403)
                {
                    error = L"GitHub ограничил запросы API. Попробуйте позже.";
                }
                else
                {
                    error = L"GitHub API вернул HTTP " +
                        std::to_wstring(statusCode) + L".";
                }
            }
            else
            {
                error = L"Не удалось получить HTTP-статус GitHub.";
            }
        }
        else
        {
            error = L"Не удалось выполнить запрос к GitHub. Проверьте подключение к интернету.";
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return success;
    }

    std::unique_ptr<UpdateChecker::Result> CheckLatest(
        const std::wstring& currentVersion)
    {
        auto result = std::make_unique<UpdateChecker::Result>();
        result->currentVersion = currentVersion;

        std::wstring json;
        if (!DownloadLatestRelease(json, result->error))
            return result;

        result->latestVersion = GetJsonString(json, L"tag_name");
        result->releaseName = GetJsonString(json, L"name");
        result->releaseNotes = GetJsonString(json, L"body");
        result->releaseUrl = GetJsonString(json, L"html_url");

        if (result->latestVersion.empty() || result->releaseUrl.empty())
        {
            result->error = L"GitHub вернул неполные сведения о релизе.";
            return result;
        }

        result->success = true;
        result->updateAvailable = IsNewerVersion(
            result->currentVersion,
            result->latestVersion);

        result->currentVersionNewer = !result->updateAvailable &&
            IsNewerVersion(result->latestVersion, result->currentVersion);

        return result;
    }
}

bool UpdateChecker::CheckAsync(
    HWND hwnd,
    UINT completionMessage,
    const std::wstring& currentVersion,
    bool interactive)
{
    if (!IsWindow(hwnd) || completionMessage < WM_APP)
        return false;

    try
    {
        std::thread([hwnd, completionMessage, currentVersion, interactive]()
        {
            auto result = CheckLatest(currentVersion);
            UpdateChecker::Result* payload = result.release();

            if (!PostMessageW(
                hwnd,
                completionMessage,
                interactive ? 1 : 0,
                reinterpret_cast<LPARAM>(payload)))
            {
                delete payload;
            }
        }).detach();
    }
    catch (...)
    {
        return false;
    }

    return true;
}
