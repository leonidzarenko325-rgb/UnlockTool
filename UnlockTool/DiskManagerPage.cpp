#include "DiskManagerPage.h"
#include "DiskManager.h"

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <algorithm>

using namespace Gdiplus;

namespace DiskManagerPage
{
    namespace
    {
        std::vector<DiskManager::VolumeInfo> volumes;
        int selectedVolume = -1;
        RECT refreshRect{}, openRect{}, checkRect{}, managementRect{}, bitLockerRect{};
        RECT volumeRows[7]{};

        const Color PANEL(255, 23, 29, 43);
        const Color CARD(255, 27, 34, 50);
        const Color ACCENT(255, 65, 125, 255);
        const Color ACCENT_LIGHT(255, 100, 175, 255);
        const Color TEXT_MAIN(255, 240, 244, 252);
        const Color TEXT_SECONDARY(255, 145, 158, 181);
        const Color TEXT_MUTED(255, 105, 119, 143);
        const Color BORDER(255, 43, 53, 73);
        const Color SUCCESS(255, 70, 190, 135);
        const Color WARNING(255, 225, 165, 75);

        bool Contains(const RECT& r, int x, int y)
        {
            return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
        }

        std::wstring FormatBytes(uint64_t bytes)
        {
            const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
            double value = static_cast<double>(bytes);
            int unit = 0;
            while (value >= 1024.0 && unit < 4)
            {
                value /= 1024.0;
                ++unit;
            }
            wchar_t buffer[64]{};
            swprintf_s(buffer, L"%.1f %s", value, units[unit]);
            return buffer;
        }

        void FillCard(Graphics& g, float x, float y, float w, float h, Color color)
        {
            GraphicsPath path;
            const float r = (std::min)(10.0f, (std::min)(w * 0.5f, h * 0.5f));
            path.AddArc(x, y, r * 2, r * 2, 180, 90);
            path.AddArc(x + w - r * 2, y, r * 2, r * 2, 270, 90);
            path.AddArc(x + w - r * 2, y + h - r * 2, r * 2, r * 2, 0, 90);
            path.AddArc(x, y + h - r * 2, r * 2, r * 2, 90, 90);
            path.CloseFigure();
            SolidBrush brush(color);
            g.FillPath(&brush, &path);
        }

        void DrawText(Graphics& g, const std::wstring& text,
            float x, float y, float w, float h, float size,
            Color color, bool bold = false)
        {
            Font font(L"Segoe UI", size,
                bold ? FontStyleBold : FontStyleRegular, UnitPixel);
            SolidBrush brush(color);
            RectF rect(x, y, w, h);
            StringFormat format;
            format.SetLineAlignment(StringAlignmentCenter);
            format.SetTrimming(StringTrimmingEllipsisCharacter);
            g.DrawString(text.c_str(), -1, &font, rect, &format, &brush);
        }

        void DrawButton(Graphics& g, const RECT& r, const std::wstring& label, bool enabled = true)
        {
            FillCard(g, static_cast<float>(r.left), static_cast<float>(r.top),
                static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top),
                enabled ? Color(255, 42, 85, 153) : CARD);
            DrawText(g, label, static_cast<float>(r.left + 5), static_cast<float>(r.top),
                static_cast<float>(r.right - r.left - 10),
                static_cast<float>(r.bottom - r.top), 10, enabled ? TEXT_MAIN : TEXT_MUTED, true);
        }

        void Refresh(std::wstring& statusText, int language)
        {
            volumes = DiskManager::EnumerateVolumes();
            if (volumes.empty())
                selectedVolume = -1;
            else if (selectedVolume < 0 || selectedVolume >= static_cast<int>(volumes.size()))
                selectedVolume = 0;

            statusText = language == 0
                ? L"Список томов обновлён"
                : L"Volume list refreshed";
        }
    }

    void Open(HWND hwnd, int language, std::wstring& statusText)
    {
        Refresh(statusText, language);
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    void Draw(Graphics& g, int language)
    {
        const bool ru = language == 0;
        DrawText(g, ru ? L"Диспетчер дисков" : L"Disk Manager",
            270, 98, 500, 30, 17, TEXT_MAIN, true);
        DrawText(g, ru ? L"Тома, файловые системы и сведения о физических дисках"
                       : L"Volumes, file systems and physical disk information",
            270, 128, 650, 20, 10, TEXT_SECONDARY);

        refreshRect = { 900, 105, 1060, 145 };
        DrawButton(g, refreshRect, ru ? L"Обновить" : L"Refresh");

        FillCard(g, 270, 165, 790, 455, PANEL);
        DrawText(g, ru ? L"ТОМА" : L"VOLUMES", 290, 178, 220, 24, 9, TEXT_MUTED, true);
        DrawText(g, ru ? L"СВОЙСТВА ВЫБРАННОГО ТОМА" : L"SELECTED VOLUME DETAILS",
            550, 178, 480, 24, 9, TEXT_MUTED, true);

        for (int i = 0; i < 7; ++i)
        {
            const int y = 210 + i * 43;
            volumeRows[i] = { 285, y, 525, y + 38 };
            if (i >= static_cast<int>(volumes.size()))
                continue;

            const auto& v = volumes[i];
            FillCard(g, 285, static_cast<float>(y), 240, 38,
                selectedVolume == i ? Color(255, 38, 58, 88) : CARD);
            DrawText(g, v.root + L"  " + v.label, 296, static_cast<float>(y + 2),
                218, 18, 11, TEXT_MAIN, true);
            DrawText(g, v.ready ? FormatBytes(v.totalBytes) : v.driveType,
                296, static_cast<float>(y + 18), 218, 16, 9, TEXT_SECONDARY);
        }

        if (selectedVolume >= 0 && selectedVolume < static_cast<int>(volumes.size()))
        {
            const auto& v = volumes[selectedVolume];
            const std::wstring rows[] =
            {
                (ru ? L"Том: " : L"Volume: ") + v.root,
                (ru ? L"Метка: " : L"Label: ") + v.label,
                (ru ? L"Тип: " : L"Type: ") + v.driveType,
                (ru ? L"Файловая система: " : L"File system: ") + v.fileSystem,
                (ru ? L"Объём: " : L"Capacity: ") + FormatBytes(v.totalBytes),
                (ru ? L"Свободно: " : L"Free: ") + FormatBytes(v.freeBytes),
                (ru ? L"Диск: " : L"Disk: ") + v.model,
                (ru ? L"Разметка: " : L"Partition style: ") + v.partitionStyle,
                (ru ? L"Здоровье: " : L"Health: ") + v.health
            };
            for (int i = 0; i < 9; ++i)
                DrawText(g, rows[i], 550, 210 + i * 31, 470, 26, 10,
                    i == 8 ? WARNING : TEXT_SECONDARY, i == 0);

            const double used = v.totalBytes > 0
                ? static_cast<double>(v.totalBytes - v.freeBytes) / static_cast<double>(v.totalBytes)
                : 0.0;
            FillCard(g, 550, 500, 470, 8, CARD);
            if (used > 0.0)
                FillCard(g, 550, 500, static_cast<float>(470.0 * used), 8, ACCENT);
        }
        else
        {
            DrawText(g, ru ? L"Подключённые тома не найдены" : L"No mounted volumes found",
                550, 260, 450, 40, 12, TEXT_SECONDARY);
        }

        openRect = { 285, 565, 435, 605 };
        checkRect = { 445, 565, 595, 605 };
        managementRect = { 605, 565, 790, 605 };
        bitLockerRect = { 800, 565, 1045, 605 };
        const bool hasSelection = selectedVolume >= 0 && selectedVolume < static_cast<int>(volumes.size());
        DrawButton(g, openRect, ru ? L"Открыть / разблокировать" : L"Open / unlock", hasSelection);
        DrawButton(g, checkRect, ru ? L"Проверить ФС" : L"Check file system", hasSelection);
        DrawButton(g, managementRect, ru ? L"Управление дисками" : L"Disk Management");
        DrawButton(g, bitLockerRect, ru ? L"Статус BitLocker" : L"BitLocker status", hasSelection);

        DrawText(g, ru
            ? L"Изменение букв томов доступно через Управление дисками. Состояние здоровья зависит от поддержки накопителя."
            : L"Drive-letter changes are available in Disk Management. Health reporting depends on drive support.",
            285, 610, 755, 18, 8, TEXT_MUTED);
    }

    bool HandleClick(HWND hwnd, int x, int y, int language, std::wstring& statusText)
    {
        const bool ru = language == 0;
        if (Contains(refreshRect, x, y))
        {
            Refresh(statusText, language);
            InvalidateRect(hwnd, nullptr, FALSE);
            return true;
        }

        for (int i = 0; i < 7 && i < static_cast<int>(volumes.size()); ++i)
        {
            if (Contains(volumeRows[i], x, y))
            {
                selectedVolume = i;
                statusText = (ru ? L"Выбран том " : L"Selected volume ") + volumes[i].root;
                InvalidateRect(hwnd, nullptr, FALSE);
                return true;
            }
        }

        if (Contains(managementRect, x, y))
        {
            statusText = DiskManager::OpenDiskManagement()
                ? (ru ? L"Открыто Управление дисками" : L"Disk Management opened")
                : (ru ? L"Не удалось открыть Управление дисками" : L"Could not open Disk Management");
            return true;
        }

        if (selectedVolume < 0 || selectedVolume >= static_cast<int>(volumes.size()))
            return false;

        const auto& volume = volumes[selectedVolume];
        if (Contains(openRect, x, y))
        {
            statusText = DiskManager::OpenVolume(volume)
                ? (ru ? L"Открыт том" : L"Volume opened")
                : (ru ? L"Не удалось открыть том" : L"Could not open volume");
            return true;
        }

        if (Contains(checkRect, x, y))
        {
            const int answer = MessageBoxW(hwnd,
                ru ? L"Запустить проверку файловой системы без отключения тома?"
                   : L"Run an online file-system scan without dismounting the volume?",
                L"UnlockTool", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
            if (answer == IDYES)
            {
                statusText = DiskManager::RunFileSystemScan(volume)
                    ? (ru ? L"Проверка запущена в отдельном окне" : L"Scan launched in a separate window")
                    : (ru ? L"Не удалось запустить проверку" : L"Could not start scan");
            }
            return true;
        }

        if (Contains(bitLockerRect, x, y))
        {
            statusText = DiskManager::ShowBitLockerStatus(volume)
                ? (ru ? L"Запрошен статус BitLocker" : L"BitLocker status requested")
                : (ru ? L"Не удалось получить статус BitLocker" : L"Could not query BitLocker status");
            return true;
        }

        return false;
    }
}
