#include "BSODMonitorPage.h"
#include "BSODMonitor.h"

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <algorithm>

using namespace Gdiplus;

namespace BSODMonitorPage
{
    namespace
    {
        std::vector<BSODMonitor::Incident> incidents;
        int selectedIncident = -1;
        RECT refreshRect{}, reliabilityRect{}, eventViewerRect{}, dumpsRect{}, exportRect{};
        RECT incidentRows[8]{};

        const Color PANEL(255, 23, 29, 43);
        const Color CARD(255, 27, 34, 50);
        const Color ACCENT(255, 65, 125, 255);
        const Color TEXT_MAIN(255, 240, 244, 252);
        const Color TEXT_SECONDARY(255, 145, 158, 181);
        const Color TEXT_MUTED(255, 105, 119, 143);
        const Color BORDER(255, 43, 53, 73);
        const Color WARNING(255, 225, 165, 75);

        bool Contains(const RECT& r, int x, int y)
        {
            return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
        }

        void FillCard(Graphics& g, float x, float y, float w, float h, Color color)
        {
            GraphicsPath path;
            const float radius = 9.0f;
            path.AddArc(x, y, radius * 2, radius * 2, 180, 90);
            path.AddArc(x + w - radius * 2, y, radius * 2, radius * 2, 270, 90);
            path.AddArc(x + w - radius * 2, y + h - radius * 2, radius * 2, radius * 2, 0, 90);
            path.AddArc(x, y + h - radius * 2, radius * 2, radius * 2, 90, 90);
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

        void DrawButton(Graphics& g, const RECT& r, const std::wstring& text)
        {
            FillCard(g, static_cast<float>(r.left), static_cast<float>(r.top),
                static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top),
                Color(255, 42, 85, 153));
            DrawText(g, text, static_cast<float>(r.left + 4), static_cast<float>(r.top),
                static_cast<float>(r.right - r.left - 8),
                static_cast<float>(r.bottom - r.top), 9, TEXT_MAIN, true);
        }

        void Refresh(std::wstring& statusText, int language)
        {
            incidents = BSODMonitor::GetIncidents(30);
            selectedIncident = incidents.empty() ? -1 : 0;
            statusText = language == 0
                ? L"Журнал BugCheck обновлён"
                : L"BugCheck event history refreshed";
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
        DrawText(g, L"BSOD Monitor", 270, 98, 500, 30, 17, TEXT_MAIN, true);
        DrawText(g, ru ? L"История системных сбоев и BugCheck из журнала Windows"
                       : L"System crash and BugCheck history from Windows Event Log",
            270, 128, 650, 20, 10, TEXT_SECONDARY);

        refreshRect = { 270, 165, 390, 205 };
        reliabilityRect = { 400, 165, 560, 205 };
        eventViewerRect = { 570, 165, 700, 205 };
        dumpsRect = { 710, 165, 840, 205 };
        exportRect = { 850, 165, 1060, 205 };
        DrawButton(g, refreshRect, ru ? L"Обновить" : L"Refresh");
        DrawButton(g, reliabilityRect, ru ? L"Надёжность" : L"Reliability");
        DrawButton(g, eventViewerRect, ru ? L"Журнал событий" : L"Event Viewer");
        DrawButton(g, dumpsRect, ru ? L"Минидампы" : L"Minidumps");
        DrawButton(g, exportRect, ru ? L"Экспорт TXT + HTML" : L"Export TXT + HTML");

        FillCard(g, 270, 220, 790, 400, PANEL);
        std::wstring count = ru ? L"Обнаружено инцидентов: " : L"Incidents detected: ";
        count += std::to_wstring(incidents.size());
        DrawText(g, count, 290, 232, 730, 28, 12, TEXT_MAIN, true);

        if (incidents.empty())
        {
            DrawText(g, ru
                ? L"Записи BugCheck не найдены. Для расширенной диагностики откройте Reliability Monitor или Event Viewer."
                : L"No matching BugCheck events found. Open Reliability Monitor or Event Viewer for additional diagnostics.",
                300, 300, 700, 60, 12, TEXT_SECONDARY);
            return;
        }

        const int visible = (std::min)(8, static_cast<int>(incidents.size()));
        for (int i = 0; i < visible; ++i)
        {
            const int y = 270 + i * 41;
            incidentRows[i] = { 285, y, 1045, y + 36 };
            FillCard(g, 285, static_cast<float>(y), 760, 36,
                selectedIncident == i ? Color(255, 38, 58, 88) : CARD);
            DrawText(g, incidents[i].timestamp, 298, static_cast<float>(y),
                240, 36, 9, TEXT_SECONDARY);
            DrawText(g, L"BugCheck " + incidents[i].bugcheckCode, 545,
                static_cast<float>(y), 180, 36, 10, WARNING, true);
            DrawText(g, incidents[i].provider, 735,
                static_cast<float>(y), 295, 36, 9, TEXT_MUTED);
        }

        if (selectedIncident >= 0 && selectedIncident < visible)
        {
            DrawText(g, incidents[selectedIncident].details,
                290, 600, 740, 16, 8, TEXT_MUTED);
        }
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
        if (Contains(reliabilityRect, x, y))
        {
            statusText = BSODMonitor::OpenReliabilityMonitor()
                ? (ru ? L"Открыт монитор стабильности" : L"Reliability Monitor opened")
                : (ru ? L"Не удалось открыть монитор стабильности" : L"Could not open Reliability Monitor");
            return true;
        }
        if (Contains(eventViewerRect, x, y))
        {
            statusText = BSODMonitor::OpenEventViewer()
                ? (ru ? L"Открыт журнал событий" : L"Event Viewer opened")
                : (ru ? L"Не удалось открыть журнал событий" : L"Could not open Event Viewer");
            return true;
        }
        if (Contains(dumpsRect, x, y))
        {
            statusText = BSODMonitor::OpenDumpFolder()
                ? (ru ? L"Открыта папка минидампов" : L"Minidump folder opened")
                : (ru ? L"Не удалось открыть папку минидампов" : L"Could not open minidump folder");
            return true;
        }
        if (Contains(exportRect, x, y))
        {
            std::wstring txt, html;
            if (BSODMonitor::ExportReports(txt, html))
            {
                statusText = ru
                    ? L"Отчёты сохранены на Рабочий стол"
                    : L"Reports saved to Desktop";
                ShellExecuteW(hwnd, L"open", html.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            else
            {
                statusText = ru
                    ? L"Не удалось создать отчёты"
                    : L"Could not create reports";
            }
            return true;
        }
        for (int i = 0; i < 8 && i < static_cast<int>(incidents.size()); ++i)
        {
            if (Contains(incidentRows[i], x, y))
            {
                selectedIncident = i;
                statusText = ru ? L"Инцидент выбран" : L"Incident selected";
                return true;
            }
        }
        return false;
    }
}
