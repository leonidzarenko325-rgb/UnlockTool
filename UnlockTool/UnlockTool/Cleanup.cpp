
#include "Cleanup.h"

#include <Windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windns.h>

extern "C" {
    BOOL WINAPI DnsFlushResolverCache(void);
}

#include <filesystem>
#include <system_error>
#include <string>
#include <vector>
#include <cstdint>
#include <limits>
#include <cwctype>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Dnsapi.lib")

namespace fs = std::filesystem;

namespace Cleanup
{
    // ========================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ========================================================

    std::wstring GetLocalAppData()
    {
        std::vector<wchar_t> buffer(32768);

        DWORD length = GetEnvironmentVariableW(
            L"LOCALAPPDATA",
            buffer.data(),
            static_cast<DWORD>(buffer.size())
        );

        if (length == 0 || length >= buffer.size())
            return L"";

        return std::wstring(buffer.data(), length);
    }

    bool IsThumbnailDatabase(const fs::path& path)
    {
        std::wstring name = path.filename().wstring();

        for (wchar_t& ch : name)
            ch = static_cast<wchar_t>(std::towlower(ch));

        return name.rfind(L"thumbcache", 0) == 0 &&
            path.extension() == L".db" ||
            (
                name.rfind(L"thumbcache", 0) == 0 &&
                name.size() >= 3 &&
                name.substr(name.size() - 3) == L".db"
                );
    }

    bool GetExpectedPath(
        CleanupType type,
        fs::path& result
    )
    {
        if (type == CleanupType::UserTemp)
        {
            std::vector<wchar_t> buffer(32768);

            DWORD length = GetTempPathW(
                static_cast<DWORD>(buffer.size()),
                buffer.data()
            );

            if (length == 0 || length >= buffer.size())
                return false;

            result = fs::path(buffer.data());
        }
        else if (type == CleanupType::ThumbnailCache)
        {
            std::wstring localAppData = GetLocalAppData();

            if (localAppData.empty())
                return false;

            result =
                fs::path(localAppData) /
                L"Microsoft" /
                L"Windows" /
                L"Explorer";
        }
        else
        {
            return false;
        }

        std::error_code ec;

        result = fs::weakly_canonical(result, ec);

        return !ec && !result.empty();
    }

    bool IsAllowedPath(
        CleanupType type,
        const fs::path& requested,
        fs::path& expected
    )
    {
        if (!GetExpectedPath(type, expected))
            return false;

        std::error_code ec;

        fs::path canonicalRequested =
            fs::weakly_canonical(requested, ec);

        if (ec)
            return false;

        return canonicalRequested == expected;
    }

    void AddSize(
        uint64_t& destination,
        uintmax_t value
    )
    {
        const uint64_t maxValue =
            (std::numeric_limits<uint64_t>::max)();

        if (value > maxValue - destination)
            destination = maxValue;
        else
            destination += static_cast<uint64_t>(value);
    }

    void AddFileCount(unsigned int& count)
    {
        if (count < (std::numeric_limits<unsigned int>::max)())
            ++count;
    }


    // ========================================================
    // ПОЛУЧЕНИЕ СПИСКА КАТЕГОРИЙ
    // ========================================================

    std::vector<CleanupItem> GetCleanupItems()
    {
        std::vector<CleanupItem> items;

        // Временные файлы пользователя.

        fs::path tempPath;

        if (GetExpectedPath(
            CleanupType::UserTemp,
            tempPath
        ))
        {
            CleanupItem temp{};

            temp.name = L"Временные файлы пользователя";
            temp.description =
                L"Временные данные текущего пользователя";

            temp.path = tempPath.wstring();
            temp.type = CleanupType::UserTemp;

            items.push_back(temp);
        }

        // Кэш эскизов Windows.

        fs::path thumbnailPath;

        if (GetExpectedPath(
            CleanupType::ThumbnailCache,
            thumbnailPath
        ))
        {
            CleanupItem thumbnails{};

            thumbnails.name = L"Кэш эскизов";
            thumbnails.description =
                L"Миниатюры изображений и видео";

            thumbnails.path = thumbnailPath.wstring();
            thumbnails.type = CleanupType::ThumbnailCache;

            items.push_back(thumbnails);
        }

        // Корзина.

        CleanupItem recycleBin{};

        recycleBin.name = L"Корзина";
        recycleBin.description =
            L"Удалённые файлы Windows";

        recycleBin.type = CleanupType::RecycleBin;

        items.push_back(recycleBin);

        // DNS.

        CleanupItem dns{};

        dns.name = L"Кэш DNS";
        dns.description =
            L"Кэш сетевого резолвера";

        dns.type = CleanupType::DnsCache;

        items.push_back(dns);

        return items;
    }


    // ========================================================
    // СКАНИРОВАНИЕ
    // ========================================================

    bool ScanItem(CleanupItem& item)
    {
        item.size = 0;
        item.filesCount = 0;

        item.deletedSize = 0;
        item.deletedFiles = 0;

        item.scanned = false;

        // Корзина.

        if (item.type == CleanupType::RecycleBin)
        {
            SHQUERYRBINFO info{};

            info.cbSize = sizeof(info);

            const HRESULT hr = SHQueryRecycleBinW(
                nullptr,
                &info
            );

            if (FAILED(hr))
                return false;

            item.size = info.i64Size > 0
                ? static_cast<uint64_t>(info.i64Size)
                : 0;

            const ULONGLONG itemCount =
                info.i64NumItems > 0
                ? static_cast<ULONGLONG>(info.i64NumItems)
                : 0;

            item.filesCount = static_cast<unsigned int>(
                itemCount >
                (std::numeric_limits<unsigned int>::max)()
                ? (std::numeric_limits<unsigned int>::max)()
                : itemCount
                );

            item.scanned = true;

            return true;
        }

        // DNS.

        if (item.type == CleanupType::DnsCache)
        {
            // Windows не предоставляет поддерживаемого API
            // для получения размера и количества DNS-записей.

            item.scanned = true;

            return true;
        }

        // Файловые категории.

        if (item.type != CleanupType::UserTemp &&
            item.type != CleanupType::ThumbnailCache)
            return false;

        if (item.path.empty())
            return false;

        fs::path root;

        if (!IsAllowedPath(
            item.type,
            fs::path(item.path),
            root
        ))
            return false;

        std::error_code ec;

        if (!fs::is_directory(root, ec) || ec)
            return false;

        // Кэш эскизов.

        if (item.type == CleanupType::ThumbnailCache)
        {
            fs::directory_iterator it(
                root,
                fs::directory_options::skip_permission_denied,
                ec
            );

            fs::directory_iterator end;

            if (ec)
                return false;

            while (it != end)
            {
                const fs::path current = it->path();

                if (IsThumbnailDatabase(current))
                {
                    std::error_code statusError;

                    auto status = fs::symlink_status(
                        current,
                        statusError
                    );

                    if (!statusError &&
                        fs::is_regular_file(status))
                    {
                        std::error_code sizeError;

                        uintmax_t size = fs::file_size(
                            current,
                            sizeError
                        );

                        if (!sizeError)
                        {
                            AddSize(item.size, size);
                            AddFileCount(item.filesCount);
                        }
                    }
                }

                it.increment(ec);

                if (ec)
                    return false;
            }
        }

        // Временные файлы пользователя.

        else
        {
            fs::recursive_directory_iterator it(
                root,
                fs::directory_options::skip_permission_denied,
                ec
            );

            fs::recursive_directory_iterator end;

            if (ec)
                return false;

            while (it != end)
            {
                const fs::path current = it->path();

                std::error_code statusError;

                auto status = fs::symlink_status(
                    current,
                    statusError
                );

                if (!statusError &&
                    fs::is_regular_file(status))
                {
                    std::error_code sizeError;

                    uintmax_t size = fs::file_size(
                        current,
                        sizeError
                    );

                    if (!sizeError)
                    {
                        AddSize(item.size, size);
                        AddFileCount(item.filesCount);
                    }
                }

                it.increment(ec);

                if (ec)
                    return false;
            }
        }

        item.scanned = true;

        return true;
    }


    // ========================================================
    // ОЧИСТКА
    // ========================================================

    bool CleanItem(CleanupItem& item)
    {
        if (!item.scanned)
            return false;

        // Корзина.

        if (item.type == CleanupType::RecycleBin)
        {
            const HRESULT hr = SHEmptyRecycleBinW(
                nullptr,
                nullptr,
                SHERB_NOCONFIRMATION |
                SHERB_NOPROGRESSUI |
                SHERB_NOSOUND
            );

            if (FAILED(hr))
                return false;

            item.deletedSize = item.size;
            item.deletedFiles = item.filesCount;

            item.size = 0;
            item.filesCount = 0;

            return true;
        }

        // DNS.

        if (item.type == CleanupType::DnsCache)
        {
            if (!DnsFlushResolverCache())
                return false;

            item.deletedSize = 0;
            item.deletedFiles = 0;

            return true;
        }

        // Файловые категории.

        if (item.type != CleanupType::UserTemp &&
            item.type != CleanupType::ThumbnailCache)
            return false;

        if (item.path.empty())
            return false;

        fs::path root;

        if (!IsAllowedPath(
            item.type,
            fs::path(item.path),
            root
        ))
            return false;

        std::error_code ec;

        if (!fs::is_directory(root, ec) || ec)
            return false;

        item.deletedSize = 0;
        item.deletedFiles = 0;

        // Собираем файлы перед удалением.

        std::vector<fs::path> files;

        bool traversalComplete = true;

        if (item.type == CleanupType::ThumbnailCache)
        {
            fs::directory_iterator it(
                root,
                fs::directory_options::skip_permission_denied,
                ec
            );

            fs::directory_iterator end;

            if (ec)
                return false;

            while (it != end)
            {
                const fs::path current = it->path();

                if (IsThumbnailDatabase(current))
                {
                    std::error_code statusError;

                    auto status = fs::symlink_status(
                        current,
                        statusError
                    );

                    if (!statusError &&
                        fs::is_regular_file(status))
                    {
                        files.push_back(current);
                    }
                }

                it.increment(ec);

                if (ec)
                {
                    traversalComplete = false;
                    break;
                }
            }
        }
        else
        {
            fs::recursive_directory_iterator it(
                root,
                fs::directory_options::skip_permission_denied,
                ec
            );

            fs::recursive_directory_iterator end;

            if (ec)
                return false;

            while (it != end)
            {
                const fs::path current = it->path();

                std::error_code statusError;

                auto status = fs::symlink_status(
                    current,
                    statusError
                );

                if (!statusError &&
                    fs::is_regular_file(status))
                {
                    files.push_back(current);
                }

                it.increment(ec);

                if (ec)
                {
                    traversalComplete = false;
                    break;
                }
            }
        }

        // Удаляем найденные файлы.

        for (const fs::path& file : files)
        {
            std::error_code statusError;

            auto status = fs::symlink_status(
                file,
                statusError
            );

            if (statusError ||
                !fs::is_regular_file(status))
                continue;

            std::error_code sizeError;

            uintmax_t size = fs::file_size(
                file,
                sizeError
            );

            if (sizeError)
                continue;

            std::error_code deleteError;

            bool removed = fs::remove(
                file,
                deleteError
            );

            if (removed && !deleteError)
            {
                AddSize(item.deletedSize, size);
                AddFileCount(item.deletedFiles);
            }
        }

        // Сохраняем статистику удаления.

        const uint64_t savedSize = item.deletedSize;
        const unsigned int savedFiles = item.deletedFiles;

        // Повторное сканирование.

        const bool scanResult = ScanItem(item);

        // Восстанавливаем статистику удаления.

        item.deletedSize = savedSize;
        item.deletedFiles = savedFiles;

        // Успех только при полном обходе и успешном сканировании.

        return traversalComplete && scanResult;
    }
}
