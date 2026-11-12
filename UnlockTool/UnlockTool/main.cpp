#include <windows.h>
#include "resource.h"
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <gdiplus.h>

#include <string>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <memory>

#include "Cleanup.h"
#include "TaskManager.h"
#include "DiskManagerPage.h"
#include "BSODMonitorPage.h"
#include "BSODMonitor.h"
#include "UpdateChecker.h"

// UsersPage
#if __has_include("Users/UsersPage.h")
#include "Users/UsersPage.h"
#elif __has_include("UsersPage.h")
#include "UsersPage.h"
#else
#error "UsersPage.h not found."
#endif

// RegistryPage
#if __has_include("Registry/RegistryPage.h")
#include "Registry/RegistryPage.h"
#elif __has_include("RegistryPage.h")
#include "RegistryPage.h"
#else
#error "RegistryPage.h not found."
#endif
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Ole32.lib")

using namespace Gdiplus;

// ============================================================
// ОСНОВНЫЕ ПАРАМЕТРЫ
// ============================================================

const wchar_t CLASS_NAME[] = L"UnlockToolWindow";
constexpr wchar_t APP_VERSION[] = L"1.0.1";
constexpr UINT UPDATE_CHECK_COMPLETED_MESSAGE = WM_APP + 41;

constexpr int WINDOW_WIDTH = 1100;
constexpr int WINDOW_HEIGHT = 720;

constexpr int SIDEBAR_WIDTH = 235;
constexpr int HEADER_HEIGHT = 82;
constexpr int STATUS_HEIGHT = 34;

constexpr int MENU_COUNT = 10;

HWND g_hwnd = nullptr;
ULONG_PTR g_gdiplusToken = 0;

int currentPage = 0;
int hoveredMenu = -1;
int hoveredCard = -1;
int hoveredCleanupItem = -1;

int currentLanguage = 0;

RECT menuRects[MENU_COUNT]{};
RECT cardRects[4]{};
RECT languageRects[2]{};

RECT cleanupItemRects[4]{};
RECT cleanupScanRect{};
RECT cleanupCleanRect{};

bool cleanupSelected[4] = { true, true, false, false };
bool cleanupScanned[4]{};

bool cleanupScanFinished = false;

uint64_t cleanupSizes[4]{};
unsigned int cleanupFileCounts[4]{};

uint64_t cleanupDeletedSizes[4]{};
unsigned int cleanupDeletedFiles[4]{};

std::wstring statusText;
wchar_t settingsPath[MAX_PATH]{};
NOTIFYICONDATAW bsodTrayIcon{};
bool bsodTrayIconAdded = false;
bool autoCheckUpdates = true;
bool updateCheckRunning = false;

// ============================================================
// TASK MANAGER
// ============================================================

std::vector<TaskManager::ProcessInfo> taskProcesses;

RECT taskRefreshRect{};
RECT taskStartupRect{};
RECT taskEndRect{};
RECT taskSuspendRect{};
RECT taskResumeRect{};

bool taskProcessesLoaded = false;

constexpr int TASK_MAX_VISIBLE_PROCESSES = 13;

int taskScrollOffset = 0;

DWORD taskSelectedPid = 0;

bool taskScrollbarDragging = false;
int taskScrollbarDragGrabOffset = 0;

constexpr int TASK_SCROLLBAR_X = 1032;
constexpr int TASK_SCROLLBAR_Y = 220;
constexpr int TASK_SCROLLBAR_HEIGHT = 350;
constexpr int TASK_SCROLLBAR_WIDTH = 8;

constexpr UINT_PTR TASK_REFRESH_TIMER_ID = 1;
constexpr UINT TASK_REFRESH_INTERVAL = 1500;
// ============================================================
// PAGE TRANSITION
// ============================================================

constexpr UINT_PTR PAGE_TRANSITION_TIMER_ID = 2;
constexpr UINT PAGE_TRANSITION_DURATION = 180;
constexpr UINT PAGE_TRANSITION_INTERVAL = 16;

Gdiplus::Bitmap* pageTransitionBitmap = nullptr;
HBITMAP pageTransitionHbitmap = nullptr;

ULONGLONG pageTransitionStart = 0;

// ============================================================
// ЦВЕТОВАЯ ПАЛИТРА
// ============================================================

Color BG_COLOR(255, 13, 16, 25);
Color SIDEBAR_COLOR(255, 17, 21, 32);
Color HEADER_COLOR(255, 19, 24, 36);

Color PANEL_COLOR(255, 23, 29, 43);
Color CARD_COLOR(255, 27, 34, 50);
Color CARD_HOVER(255, 35, 46, 67);

Color ACCENT(255, 65, 125, 255);
Color ACCENT_LIGHT(255, 100, 175, 255);

Color TEXT_MAIN(255, 240, 244, 252);
Color TEXT_SECONDARY(255, 145, 158, 181);
Color TEXT_MUTED(255, 105, 119, 143);

Color BORDER_COLOR(255, 43, 53, 73);
Color BORDER_HOVER(255, 67, 105, 165);

Color SUCCESS_COLOR(255, 70, 190, 135);
Color WARNING_COLOR(255, 225, 165, 75);

// ============================================================
// ЛОКАЛИЗАЦИЯ
// ============================================================

const wchar_t* menuNames[2][MENU_COUNT] =
{
    {
        L"Главная",
        L"Диспетчер задач",
        L"Пользователи",
        L"Диагностика",
        L"Восстановление",
        L"Очистка системы",
        L"BSOD Monitor",
        L"Настройки",
        L"Реестр",
        L"Диспетчер дисков"
    },
    {
        L"Home",
        L"Task Manager",
        L"Users",
        L"Diagnostics",
        L"Recovery",
        L"System Cleanup",
        L"BSOD Monitor",
        L"Settings",
        L"Registry",
        L"Disk Manager"
    }
};

const wchar_t* menuIcons[MENU_COUNT] =
{
    L"⌂", L"▦", L"♙", L"⌕",
    L"↻", L"♲", L"!", L"⚙", L"▤", L"▱"
};

const wchar_t* pageDescriptions[2][MENU_COUNT] =
{
    {
        L"Центр управления UnlockTool",
        L"Просмотр процессов и ресурсов системы",
        L"Управление локальными пользователями Windows",
        L"Проверка состояния компьютера",
        L"Инструменты восстановления Windows",
        L"Очистка временных файлов и кэша",
        L"Мониторинг критических ошибок системы",
        L"Настройка приложения",
        L"Проверка важных параметров реестра Windows",
        L"Информация о дисках, томах и файловых системах"
    },
    {
        L"UnlockTool Control Center",
        L"Processes and system resources",
        L"Manage local Windows users",
        L"Check your computer",
        L"Windows recovery tools",
        L"Temporary files and cache cleanup",
        L"Monitor critical system errors",
        L"Application settings",
        L"Check important Windows registry parameters",
        L"Disk, volume and file-system information"
    }
};

const wchar_t* cardTitles[2][MENU_COUNT][4] =
{
    {
        { L"Система", L"Быстрый доступ", L"Состояние", L"Инструменты" },
        { L"Процессы", L"Производительность", L"Память", L"Поиск процессов" },
        { L"Список пользователей", L"Добавить пользователя", L"Изменить пароль", L"Удалить пользователя" },
        { L"Информация о системе", L"Проверка компонентов", L"Службы Windows", L"Журнал событий" },
        { L"Microsoft Store", L"Windows Update", L"Системные файлы", L"Параметры Windows" },
        { L"Временные файлы", L"Кэш эскизов", L"Корзина", L"Кэш DNS" },
        { L"История BSOD", L"Анализ ошибок", L"Минидампы", L"Резервные копии" },
        { L"Обновления", L"Уведомления", L"Безопасность", L"О программе" },
        { L"Winlogon", L"Параметры запуска", L"Настройка Windows", L"Состояние реестра" },
        { L"Тома", L"Свойства диска", L"Файловая система", L"BitLocker" }
    },
    {
        { L"System", L"Quick access", L"Status", L"Tools" },
        { L"Processes", L"Performance", L"Memory", L"Process search" },
        { L"User list", L"Add user", L"Change password", L"Delete user" },
        { L"System information", L"Component check", L"Windows services", L"Event Viewer" },
        { L"Microsoft Store", L"Windows Update", L"System files", L"Windows settings" },
        { L"Temporary files", L"Thumbnail cache", L"Recycle Bin", L"DNS cache" },
        { L"BSOD history", L"Error analysis", L"Minidumps", L"Backups" },
        { L"Updates", L"Notifications", L"Security", L"About" },
        { L"Winlogon", L"Startup parameters", L"Windows setup", L"Registry status" },
        { L"Volumes", L"Disk properties", L"File system", L"BitLocker" }
    }
};

const wchar_t* cardDescriptions[2][MENU_COUNT][4] =
{
    {
        { L"Общая информация о компьютере", L"Переход к основным инструментам", L"Состояние приложения", L"Доступные функции" },
        { L"Просмотр запущенных процессов", L"Загрузка процессора и памяти", L"Использование оперативной памяти", L"Быстрый поиск процесса" },
        { L"Просмотр локальных учётных записей", L"Создание новой учётной записи", L"Настройка пароля пользователя", L"Удаление с подтверждением" },
        { L"Сведения об оборудовании и ОС", L"Проверка системных компонентов", L"Просмотр состояния служб", L"Системные события и ошибки" },
        { L"Средства восстановления Store", L"Средства восстановления обновлений", L"Проверка целостности Windows", L"Восстановление системных параметров" },
        { L"Временные данные текущего пользователя", L"Миниатюры изображений и видео", L"Удалённые файлы Windows", L"Кэш сетевого резолвера" },
        { L"Список обнаруженных инцидентов", L"Исследование причин сбоев", L"Работа с файлами дампов", L"Экспорт и импорт истории" },
        { L"Проверка новых версий UnlockTool", L"Параметры уведомлений", L"Параметры безопасности", L"Версия и сведения о программе" },
        { L"Проверка Winlogon", L"Параметры запуска Windows", L"Проверка настройки системы", L"Общее состояние реестра" },
        { L"Список подключённых томов", L"Модель, разметка и объём", L"Проверка файловой системы", L"Проверка состояния шифрования" }
    },
    {
        { L"Computer information", L"Open main tools", L"Application status", L"Available features" },
        { L"View running processes", L"CPU and memory usage", L"RAM usage", L"Find a process" },
        { L"View local accounts", L"Create a new account", L"Configure user password", L"Delete user with confirmation" },
        { L"Hardware and OS information", L"Check system components", L"View service status", L"System events and errors" },
        { L"Store recovery tools", L"Update recovery tools", L"Check Windows integrity", L"Restore system settings" },
        { L"Current user's temporary data", L"Image and video thumbnails", L"Deleted files in Windows", L"Network resolver cache" },
        { L"Detected incidents", L"Investigate system crashes", L"Work with dump files", L"Export and import history" },
        { L"Check for new UnlockTool versions", L"Notification preferences", L"Security settings", L"Version and app information" },
        { L"Winlogon validation", L"Windows startup parameters", L"System configuration check", L"Registry health status" },
        { L"Mounted volume list", L"Model, partition style and capacity", L"File-system scan", L"Encryption status" }
    }
};

const wchar_t* languageTitle[2] =
{
    L"Язык интерфейса",
    L"Interface language"
};

const wchar_t* languageDescription[2] =
{
    L"Выберите язык приложения",
    L"Choose the application language"
};

const wchar_t* languageNames[2][2] =
{
    { L"Русский", L"English" },
    { L"Русский", L"English" }
};

const wchar_t* heroTitle[2] =
{
    L"Добро пожаловать в UnlockTool",
    L"Welcome to UnlockTool"
};

const wchar_t* heroDescription[2] =
{
    L"Управляйте системой Windows в одном приложении.",
    L"Manage your Windows system in one application."
};

const wchar_t* startButtonText[2] =
{
    L"Начать",
    L"Get started"
};

const wchar_t* readyText[2] =
{
    L"Готово",
    L"Ready"
};

// ============================================================
// НАСТРОЙКИ
// ============================================================

void InitializeSettings()
{
    wchar_t localAppData[MAX_PATH]{};

    if (SUCCEEDED(SHGetFolderPathW(
        nullptr,
        CSIDL_LOCAL_APPDATA,
        nullptr,
        SHGFP_TYPE_CURRENT,
        localAppData)))
    {
        std::wstring folder =
            std::wstring(localAppData) + L"\\UnlockTool";

        CreateDirectoryW(folder.c_str(), nullptr);

        std::wstring file = folder + L"\\settings.ini";

        lstrcpynW(settingsPath, file.c_str(), MAX_PATH);
    }

    if (settingsPath[0] == L'\0')
        return;

    currentLanguage = GetPrivateProfileIntW(
        L"General",
        L"Language",
        0,
        settingsPath
    );

    if (currentLanguage < 0 || currentLanguage > 1)
        currentLanguage = 0;

    autoCheckUpdates = GetPrivateProfileIntW(
        L"Updates",
        L"AutoCheck",
        1,
        settingsPath) != 0;
}

void SaveLanguage()
{
    if (settingsPath[0] == L'\0')
        return;

    wchar_t value[8]{};

    wsprintfW(value, L"%d", currentLanguage);

    WritePrivateProfileStringW(
        L"General",
        L"Language",
        value,
        settingsPath
    );
}

void SaveAutoCheckUpdates()
{
    if (settingsPath[0] == L'\0')
        return;

    WritePrivateProfileStringW(
        L"Updates",
        L"AutoCheck",
        autoCheckUpdates ? L"1" : L"0",
        settingsPath);
}

// ============================================================
// CLEANUP: ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================

Cleanup::CleanupItem* FindCleanupItem(
    std::vector<Cleanup::CleanupItem>& items,
    Cleanup::CleanupType type)
{
    for (auto& item : items)
    {
        if (item.type == type)
            return &item;
    }

    return nullptr;
}

int GetCleanupIndex(Cleanup::CleanupType type)
{
    switch (type)
    {
    case Cleanup::CleanupType::UserTemp:
        return 0;

    case Cleanup::CleanupType::ThumbnailCache:
        return 1;

    case Cleanup::CleanupType::RecycleBin:
        return 2;

    case Cleanup::CleanupType::DnsCache:
        return 3;
    }

    return -1;
}

bool IsCleanupCategorySupported(int index)
{
    return index >= 0 && index < 4;
}

bool HasSelectedCleanupCategory()
{
    for (int i = 0; i < 4; ++i)
    {
        if (cleanupSelected[i])
            return true;
    }

    return false;
}

bool HasScannedCleanupCategory()
{
    for (int i = 0; i < 4; ++i)
    {
        if (cleanupSelected[i] && cleanupScanned[i])
            return true;
    }

    return false;
}

void ResetCleanupResults()
{
    cleanupScanFinished = false;

    for (int i = 0; i < 4; ++i)
    {
        cleanupScanned[i] = false;

        cleanupSizes[i] = 0;
        cleanupFileCounts[i] = 0;

        cleanupDeletedSizes[i] = 0;
        cleanupDeletedFiles[i] = 0;
    }
}

// ============================================================
// ФОРМАТИРОВАНИЕ РАЗМЕРА
// ============================================================

std::wstring FormatBytes(uint64_t bytes)
{
    const wchar_t* units[] =
    {
        L"Б", L"КБ", L"МБ", L"ГБ", L"ТБ"
    };

    double value = static_cast<double>(bytes);
    int unit = 0;

    while (value >= 1024.0 && unit < 4)
    {
        value /= 1024.0;
        ++unit;
    }

    wchar_t buffer[64]{};

    swprintf_s(
        buffer,
        L"%.2f %s",
        value,
        units[unit]
    );

    return buffer;
}

// ============================================================
// TASK MANAGER: ПОИСК ВЫБРАННОГО ПРОЦЕССА
// ============================================================

TaskManager::ProcessInfo* GetSelectedTaskProcess()
{
    if (taskSelectedPid == 0)
        return nullptr;

    for (auto& process : taskProcesses)
    {
        if (process.pid == taskSelectedPid)
            return &process;
    }

    return nullptr;
}

// ============================================================
// TASK MANAGER: ОБНОВЛЕНИЕ СПИСКА
// ============================================================

void RefreshTaskProcesses(HWND hwnd, bool showStatus = true)
{
    if (showStatus)
    {
        statusText = currentLanguage == 0
            ? L"Обновление списка процессов..."
            : L"Refreshing process list...";

        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
    }

    taskProcesses = TaskManager::GetProcesses();

    taskProcessesLoaded = true;

    int maxOffset = (std::max)(
        0,
        static_cast<int>(taskProcesses.size())
        - TASK_MAX_VISIBLE_PROCESSES
        );

    taskScrollOffset = (std::max)(
        0,
        (std::min)(taskScrollOffset, maxOffset)
        );

    // Сохраняем выбор по PID, а не по позиции строки.

    if (taskSelectedPid != 0 &&
        GetSelectedTaskProcess() == nullptr)
    {
        taskSelectedPid = 0;
    }

    if (showStatus)
    {
        statusText = currentLanguage == 0
            ? L"Список процессов обновлён"
            : L"Process list refreshed";
    }

    InvalidateRect(hwnd, nullptr, FALSE);
}

// ============================================================
// TASK MANAGER: ГЕОМЕТРИЯ ПОЛЗУНКА
// ============================================================

bool GetTaskScrollbarGeometry(
    int& thumbHeight,
    int& thumbY,
    int& maxOffset)
{
    int totalProcesses =
        static_cast<int>(taskProcesses.size());

    maxOffset = (std::max)(
        0,
        totalProcesses - TASK_MAX_VISIBLE_PROCESSES
        );

    if (maxOffset <= 0)
    {
        thumbHeight = TASK_SCROLLBAR_HEIGHT;
        thumbY = TASK_SCROLLBAR_Y;
        return false;
    }

    thumbHeight = (std::max)(
        35,
        TASK_SCROLLBAR_HEIGHT *
        TASK_MAX_VISIBLE_PROCESSES /
        totalProcesses
        );

    int availableTrack =
        TASK_SCROLLBAR_HEIGHT - thumbHeight;

    thumbY = TASK_SCROLLBAR_Y +
        static_cast<int>(
            static_cast<double>(taskScrollOffset) /
            maxOffset *
            availableTrack
            );

    return true;
}

// ============================================================
// ГРАФИЧЕСКИЕ ФУНКЦИИ
// ============================================================

void MakeRoundedPath(
    GraphicsPath& path,
    float x,
    float y,
    float width,
    float height,
    float radius)
{
    path.Reset();

    radius = (std::max)(
        0.0f,
        (std::min)(
            radius,
            (std::min)(width, height) / 2.0f
            )
        );

    if (radius <= 0.0f)
    {
        path.AddRectangle(RectF(x, y, width, height));
        return;
    }

    float d = radius * 2.0f;

    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + width - d, y, d, d, 270, 90);
    path.AddArc(x + width - d, y + height - d, d, d, 0, 90);
    path.AddArc(x, y + height - d, d, d, 90, 90);

    path.CloseFigure();
}

void FillRounded(
    Graphics& graphics,
    float x,
    float y,
    float width,
    float height,
    float radius,
    Color color)
{
    GraphicsPath path;

    MakeRoundedPath(path, x, y, width, height, radius);

    SolidBrush brush(color);

    graphics.FillPath(&brush, &path);
}

void DrawRounded(
    Graphics& graphics,
    float x,
    float y,
    float width,
    float height,
    float radius,
    Color color,
    float thickness = 1.0f)
{
    GraphicsPath path;

    MakeRoundedPath(path, x, y, width, height, radius);

    Pen pen(color, thickness);

    graphics.DrawPath(&pen, &path);
}

void DrawGradient(
    Graphics& graphics,
    float x,
    float y,
    float width,
    float height,
    float radius,
    Color color1,
    Color color2)
{
    GraphicsPath path;

    MakeRoundedPath(path, x, y, width, height, radius);

    LinearGradientBrush brush(
        PointF(x, y),
        PointF(x + width, y + height),
        color1,
        color2
    );

    graphics.FillPath(&brush, &path);
}

void DrawShadow(
    Graphics& graphics,
    float x,
    float y,
    float width,
    float height,
    float radius)
{
    for (int i = 5; i >= 1; --i)
    {
        GraphicsPath path;

        MakeRoundedPath(
            path,
            x,
            y + i * 2,
            width,
            height,
            radius
        );

        SolidBrush brush(Color(7, 0, 0, 0));

        graphics.FillPath(&brush, &path);
    }
}

void DrawAccentLine(
    Graphics& graphics,
    float x1,
    float y1,
    float x2,
    float y2,
    Color color,
    float thickness = 2.0f)
{
    Pen pen(color, thickness);

    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);

    graphics.DrawLine(&pen, x1, y1, x2, y2);
}

// ============================================================
// ТЕКСТ
// ============================================================

void DrawTextAligned(
    Graphics& graphics,
    const std::wstring& text,
    float x,
    float y,
    float width,
    float height,
    float fontSize,
    Color color,
    StringAlignment horizontal,
    bool bold = false,
    const wchar_t* fontName = L"Segoe UI")
{
    Font font(
        fontName,
        fontSize,
        bold ? FontStyleBold : FontStyleRegular,
        UnitPixel
    );

    SolidBrush brush(color);

    RectF rect(x, y, width, height);

    StringFormat format;

    format.SetAlignment(horizontal);
    format.SetLineAlignment(StringAlignmentCenter);

    graphics.DrawString(
        text.c_str(),
        -1,
        &font,
        rect,
        &format,
        &brush
    );
}

void DrawTextLeft(
    Graphics& graphics,
    const std::wstring& text,
    float x,
    float y,
    float width,
    float height,
    float fontSize,
    Color color,
    bool bold = false)
{
    DrawTextAligned(
        graphics,
        text,
        x,
        y,
        width,
        height,
        fontSize,
        color,
        StringAlignmentNear,
        bold
    );
}

void DrawTextCenter(
    Graphics& graphics,
    const std::wstring& text,
    float x,
    float y,
    float width,
    float height,
    float fontSize,
    Color color,
    bool bold = false,
    const wchar_t* fontName = L"Segoe UI")
{
    DrawTextAligned(
        graphics,
        text,
        x,
        y,
        width,
        height,
        fontSize,
        color,
        StringAlignmentCenter,
        bold,
        fontName
    );
}

// ============================================================
// ЛОГОТИП
// ============================================================

void DrawLogo(Graphics& graphics)
{
    DrawShadow(graphics, 22, 22, 43, 43, 13);

    DrawGradient(
        graphics,
        22, 22, 43, 43, 13,
        ACCENT,
        ACCENT_LIGHT
    );

    DrawTextCenter(
        graphics,
        L"U",
        22, 21, 43, 43,
        25, TEXT_MAIN, true
    );

    DrawTextLeft(
        graphics,
        L"UnlockTool",
        77, 19, 135, 25,
        19, TEXT_MAIN, true
    );

    DrawTextLeft(
        graphics,
        L"WINDOWS UTILITY",
        78, 43, 135, 18,
        9, TEXT_SECONDARY, true
    );
}

// ============================================================
// БОКОВАЯ ПАНЕЛЬ
// ============================================================

void DrawSidebar(Graphics& graphics)
{
    SolidBrush sidebarBrush(SIDEBAR_COLOR);

    graphics.FillRectangle(
        &sidebarBrush,
        0, 0,
        SIDEBAR_WIDTH,
        WINDOW_HEIGHT
    );

    DrawLogo(graphics);

    Pen separator(BORDER_COLOR, 1.0f);

    graphics.DrawLine(
        &separator,
        18, 85,
        SIDEBAR_WIDTH - 18, 85
    );

    DrawTextLeft(
        graphics,
        currentLanguage == 0 ? L"РАЗДЕЛЫ" : L"SECTIONS",
        24, 105, 180, 24,
        10, TEXT_MUTED, true
    );

    int y = 132;

    for (int i = 0; i < MENU_COUNT; ++i)
    {
        menuRects[i] = {
            12,
            y,
            SIDEBAR_WIDTH - 12,
            y + 42
        };

        if (currentPage == i)
        {
            FillRounded(
                graphics,
                12, y,
                SIDEBAR_WIDTH - 24,
                42, 10,
                Color(255, 34, 57, 94)
            );

            DrawAccentLine(
                graphics,
                14, y + 11,
                14, y + 33,
                ACCENT_LIGHT,
                3.0f
            );
        }
        else if (hoveredMenu == i)
        {
            FillRounded(
                graphics,
                12, y,
                SIDEBAR_WIDTH - 24,
                42, 10,
                Color(255, 30, 38, 55)
            );
        }

        Color textColor =
            currentPage == i
            ? TEXT_MAIN
            : TEXT_SECONDARY;

        DrawTextCenter(
            graphics,
            menuIcons[i],
            22, y, 30, 42,
            19, textColor,
            currentPage == i,
            L"Segoe UI Symbol"
        );

        DrawTextLeft(
            graphics,
            menuNames[currentLanguage][i],
            58, y, 155, 42,
            13, textColor,
            currentPage == i
        );

        y += 47;
    }

    Pen bottomSeparator(BORDER_COLOR, 1.0f);

    graphics.DrawLine(
        &bottomSeparator,
        18,
        WINDOW_HEIGHT - 100,
        SIDEBAR_WIDTH - 18,
        WINDOW_HEIGHT - 100
    );

    FillRounded(
        graphics,
        22,
        WINDOW_HEIGHT - 83,
        30,
        30,
        9,
        Color(255, 31, 48, 72)
    );

    DrawTextCenter(
        graphics,
        L"U",
        22,
        WINDOW_HEIGHT - 83,
        30,
        30,
        16,
        ACCENT_LIGHT,
        true
    );

    DrawTextLeft(
        graphics,
        L"UnlockTool",
        62,
        WINDOW_HEIGHT - 82,
        140,
        18,
        12,
        TEXT_MAIN,
        true
    );

    DrawTextLeft(
        graphics,
        L"Windows 11 Utility",
        62,
        WINDOW_HEIGHT - 62,
        145,
        17,
        10,
        TEXT_SECONDARY
    );
}

// ============================================================
// ВЕРХНЯЯ ПАНЕЛЬ
// ============================================================

void DrawHeader(Graphics& graphics)
{
    SolidBrush headerBrush(HEADER_COLOR);

    graphics.FillRectangle(
        &headerBrush,
        SIDEBAR_WIDTH,
        0,
        WINDOW_WIDTH - SIDEBAR_WIDTH,
        HEADER_HEIGHT
    );

    DrawTextLeft(
        graphics,
        menuNames[currentLanguage][currentPage],
        SIDEBAR_WIDTH + 30,
        11, 550, 31,
        21, TEXT_MAIN, true
    );

    DrawTextLeft(
        graphics,
        pageDescriptions[currentLanguage][currentPage],
        SIDEBAR_WIDTH + 31,
        43, 650, 22,
        12, TEXT_SECONDARY
    );

    FillRounded(
        graphics,
        WINDOW_WIDTH - 145,
        24, 110, 32,
        9,
        Color(255, 27, 43, 68)
    );

    DrawRounded(
        graphics,
        WINDOW_WIDTH - 145,
        24, 110, 32,
        9,
        Color(255, 51, 82, 133)
    );

    DrawTextCenter(
        graphics,
        std::wstring(L"VERSION ") + APP_VERSION,
        WINDOW_WIDTH - 145,
        24, 110, 32,
        10, ACCENT_LIGHT, true
    );

    Pen separator(BORDER_COLOR, 1.0f);

    graphics.DrawLine(
        &separator,
        SIDEBAR_WIDTH,
        HEADER_HEIGHT - 1,
        WINDOW_WIDTH,
        HEADER_HEIGHT - 1
    );
}

// ============================================================
// ГЛАВНЫЙ БАННЕР
// ============================================================

void DrawHero(Graphics& graphics)
{
    DrawShadow(graphics, 270, 108, 790, 150, 18);

    DrawGradient(
        graphics,
        270, 108, 790, 150, 18,
        Color(255, 33, 64, 112),
        Color(255, 24, 39, 68)
    );

    DrawRounded(
        graphics,
        270, 108, 790, 150,
        18,
        Color(255, 52, 82, 135)
    );

    FillRounded(
        graphics,
        1010, 128,
        5, 108,
        3,
        Color(100, 100, 175, 255)
    );

    DrawTextLeft(
        graphics,
        heroTitle[currentLanguage],
        300, 126, 680, 37,
        23, TEXT_MAIN, true
    );

    DrawTextLeft(
        graphics,
        heroDescription[currentLanguage],
        301, 166, 680, 28,
        13, Color(255, 205, 216, 235)
    );

    DrawGradient(
        graphics,
        300, 210, 112, 30,
        8,
        ACCENT,
        ACCENT_LIGHT
    );

    DrawTextCenter(
        graphics,
        startButtonText[currentLanguage],
        300, 210, 112, 30,
        12, TEXT_MAIN, true
    );
}

// ============================================================
// КАРТОЧКИ
// ============================================================

void DrawCards(Graphics& graphics, int startY)
{
    const int left = 270;
    const int gap = 18;

    const int cardWidth = 380;
    const int cardHeight = 145;

    for (int i = 0; i < 4; ++i)
    {
        int column = i % 2;
        int row = i / 2;

        int x = left + column * (cardWidth + gap);
        int y = startY + row * (cardHeight + gap);

        cardRects[i] = {
            x, y,
            x + cardWidth,
            y + cardHeight
        };

        bool hovered = hoveredCard == i;

        DrawShadow(
            graphics,
            x, y,
            cardWidth,
            cardHeight,
            14
        );

        FillRounded(
            graphics,
            x, y,
            cardWidth,
            cardHeight,
            14,
            hovered ? CARD_HOVER : CARD_COLOR
        );

        DrawRounded(
            graphics,
            x, y,
            cardWidth,
            cardHeight,
            14,
            hovered ? BORDER_HOVER : BORDER_COLOR
        );

        FillRounded(
            graphics,
            x + 20, y + 20,
            38, 38,
            10,
            Color(255, 34, 65, 112)
        );

        DrawTextCenter(
            graphics,
            std::to_wstring(i + 1),
            x + 20, y + 20,
            38, 38,
            16, ACCENT_LIGHT, true
        );

        DrawTextLeft(
            graphics,
            cardTitles[currentLanguage][currentPage][i],
            x + 20, y + 68,
            cardWidth - 40, 27,
            16, TEXT_MAIN, true
        );

        DrawTextLeft(
            graphics,
            cardDescriptions[currentLanguage][currentPage][i],
            x + 20, y + 98,
            cardWidth - 40, 30,
            11, TEXT_SECONDARY
        );

        DrawTextCenter(
            graphics,
            L"›",
            x + cardWidth - 34,
            y + 20,
            18, 28,
            20,
            hovered ? ACCENT_LIGHT : TEXT_MUTED,
            true
        );
    }
}

// ============================================================
// ВЫБОР ЯЗЫКА
// ============================================================

void DrawLanguageSelector(Graphics& graphics)
{
    DrawShadow(graphics, 270, 115, 790, 100, 14);

    FillRounded(
        graphics,
        270, 115, 790, 100,
        14,
        PANEL_COLOR
    );

    DrawRounded(
        graphics,
        270, 115, 790, 100,
        14,
        BORDER_COLOR
    );

    DrawTextLeft(
        graphics,
        languageTitle[currentLanguage],
        292, 128, 350, 28,
        17, TEXT_MAIN, true
    );

    DrawTextLeft(
        graphics,
        languageDescription[currentLanguage],
        292, 158, 350, 25,
        12, TEXT_SECONDARY
    );

    for (int i = 0; i < 2; ++i)
    {
        int x = 720 + i * 145;

        languageRects[i] = {
            x, 145,
            x + 125, 190
        };

        if (currentLanguage == i)
        {
            DrawGradient(
                graphics,
                x, 145, 125, 45,
                10,
                ACCENT,
                ACCENT_LIGHT
            );
        }
        else
        {
            FillRounded(
                graphics,
                x, 145, 125, 45,
                10,
                Color(255, 31, 39, 56)
            );

            DrawRounded(
                graphics,
                x, 145, 125, 45,
                10,
                BORDER_COLOR
            );
        }

        DrawTextCenter(
            graphics,
            languageNames[currentLanguage][i],
            x, 145, 125, 45,
            13, TEXT_MAIN, true
        );
    }
}

// ============================================================
// ЧЕКБОКС CLEANUP
// ============================================================

void DrawCleanupCheckbox(
    Graphics& graphics,
    int x,
    int y,
    bool checked,
    bool enabled,
    bool hovered)
{
    const float size = 22.0f;

    if (!enabled)
    {
        FillRounded(
            graphics,
            x, y,
            size, size,
            6,
            Color(255, 32, 38, 51)
        );

        DrawRounded(
            graphics,
            x, y,
            size, size,
            6,
            Color(255, 52, 61, 78)
        );

        return;
    }

    if (checked)
    {
        Color first = hovered
            ? Color(255, 105, 155, 255)
            : ACCENT;

        Color second = hovered
            ? Color(255, 130, 205, 255)
            : ACCENT_LIGHT;

        DrawGradient(
            graphics,
            x, y,
            size, size,
            6,
            first,
            second
        );

        DrawRounded(
            graphics,
            x, y,
            size, size,
            6,
            second,
            1.0f
        );

        Pen checkPen(
            Color(255, 255, 255, 255),
            2.4f
        );

        checkPen.SetStartCap(LineCapRound);
        checkPen.SetEndCap(LineCapRound);
        checkPen.SetLineJoin(LineJoinRound);

        graphics.DrawLine(
            &checkPen,
            x + 5.0f, y + 11.5f,
            x + 9.0f, y + 15.0f
        );

        graphics.DrawLine(
            &checkPen,
            x + 9.0f, y + 15.0f,
            x + 17.0f, y + 6.5f
        );
    }
    else
    {
        Color background = hovered
            ? Color(255, 44, 55, 77)
            : Color(255, 27, 34, 49);

        Color border = hovered
            ? ACCENT_LIGHT
            : Color(255, 78, 92, 116);

        FillRounded(
            graphics,
            x, y,
            size, size,
            6,
            background
        );

        DrawRounded(
            graphics,
            x, y,
            size, size,
            6,
            border,
            1.3f
        );
    }
}

// ============================================================
// КНОПКА CLEANUP
// ============================================================

void DrawCleanupButton(
    Graphics& graphics,
    const RECT& rect,
    const std::wstring& text,
    bool primary,
    bool enabled)
{
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    if (primary)
    {
        if (enabled)
        {
            DrawGradient(
                graphics,
                rect.left,
                rect.top,
                width,
                height,
                10,
                ACCENT,
                ACCENT_LIGHT
            );
        }
        else
        {
            FillRounded(
                graphics,
                rect.left,
                rect.top,
                width,
                height,
                10,
                Color(255, 39, 48, 66)
            );
        }
    }
    else
    {
        FillRounded(
            graphics,
            rect.left,
            rect.top,
            width,
            height,
            10,
            enabled
            ? Color(255, 112, 49, 57)
            : Color(255, 45, 39, 48)
        );

        DrawRounded(
            graphics,
            rect.left,
            rect.top,
            width,
            height,
            10,
            enabled
            ? Color(255, 175, 77, 85)
            : Color(255, 75, 57, 63)
        );
    }

    DrawTextCenter(
        graphics,
        text,
        rect.left,
        rect.top,
        width,
        height,
        12,
        enabled ? TEXT_MAIN : TEXT_MUTED,
        true
    );
}

// ============================================================
// TASK MANAGER: ИНТЕРФЕЙС
// ============================================================

void DrawTaskManagerPage(Graphics& graphics)
{
    DrawTextLeft(
        graphics,
        currentLanguage == 0
        ? L"Запущенные процессы"
        : L"Running processes",
        270, 103, 350, 28,
        17, TEXT_MAIN, true
    );

    std::wstring countText =
        currentLanguage == 0
        ? L"Всего процессов: "
        : L"Total processes: ";

    countText += std::to_wstring(taskProcesses.size());

    DrawTextLeft(
        graphics,
        countText,
        270, 132, 350, 20,
        10, TEXT_SECONDARY
    );

    taskEndRect = { 550, 105, 630, 145 };
    taskSuspendRect = { 635, 105, 715, 145 };
    taskResumeRect = { 720, 105, 800, 145 };
    taskStartupRect = { 805, 105, 895, 145 };
    taskRefreshRect = { 900, 105, 1060, 145 };

    TaskManager::ProcessInfo* selected =
        GetSelectedTaskProcess();

    bool hasSelection = selected != nullptr;

    bool canSuspend =
        hasSelection &&
        !selected->isCritical &&
        !selected->isSuspended;

    bool canResume =
        hasSelection &&
        !selected->isCritical &&
        selected->isSuspended;

    bool canEnd =
        hasSelection &&
        !selected->isCritical;

    DrawCleanupButton(
        graphics,
        taskEndRect,
        currentLanguage == 0
        ? L"Завершить"
        : L"End task",
        false,
        canEnd
    );

    DrawCleanupButton(
        graphics,
        taskSuspendRect,
        currentLanguage == 0
        ? L"Пауза"
        : L"Suspend",
        true,
        canSuspend
    );

    DrawCleanupButton(
        graphics,
        taskResumeRect,
        currentLanguage == 0
        ? L"Продолжить"
        : L"Resume",
        true,
        canResume
    );

    DrawCleanupButton(
        graphics,
        taskStartupRect,
        currentLanguage == 0
        ? L"Автозагрузка"
        : L"Startup",
        true,
        true
    );

    DrawCleanupButton(
        graphics,
        taskRefreshRect,
        currentLanguage == 0
        ? L"Обновить"
        : L"Refresh",
        true,
        true
    );

    FillRounded(
        graphics,
        270, 165,
        790, 455,
        13,
        PANEL_COLOR
    );

    DrawRounded(
        graphics,
        270, 165,
        790, 455,
        13,
        BORDER_COLOR
    );

    FillRounded(
        graphics,
        282, 177,
        766, 38,
        8,
        Color(255, 29, 39, 57)
    );

    const int columns[] =
    {
        292, 500, 570, 650, 755, 890
    };

    const wchar_t* headers[2][6] =
    {
        {
            L"ПРОЦЕСС",
            L"PID",
            L"CPU",
            L"RAM",
            L"СТАТУС",
            L"КРИТИЧНОСТЬ"
        },
        {
            L"PROCESS",
            L"PID",
            L"CPU",
            L"RAM",
            L"STATUS",
            L"CRITICAL"
        }
    };

    const int headerWidths[] =
    {
        200, 65, 70, 100, 130, 130
    };

    for (int i = 0; i < 6; ++i)
    {
        DrawTextLeft(
            graphics,
            headers[currentLanguage][i],
            columns[i],
            177,
            headerWidths[i],
            38,
            9,
            TEXT_MUTED,
            true
        );
    }

    if (!taskProcessesLoaded)
    {
        DrawTextCenter(
            graphics,
            currentLanguage == 0
            ? L"Список процессов ещё не загружен"
            : L"Process list has not been loaded",
            290, 330,
            750, 50,
            13, TEXT_SECONDARY
        );

        return;
    }

    if (taskProcesses.empty())
    {
        DrawTextCenter(
            graphics,
            currentLanguage == 0
            ? L"Процессы не найдены"
            : L"No processes found",
            290, 330,
            750, 50,
            13, TEXT_SECONDARY
        );

        return;
    }

    int totalProcesses =
        static_cast<int>(taskProcesses.size());

    int maxOffset = (std::max)(
        0,
        totalProcesses - TASK_MAX_VISIBLE_PROCESSES
        );

    taskScrollOffset = (std::max)(
        0,
        (std::min)(taskScrollOffset, maxOffset)
        );

    int visibleCount = (std::min)(
        TASK_MAX_VISIBLE_PROCESSES,
        totalProcesses - taskScrollOffset
        );

    for (int i = 0; i < visibleCount; ++i)
    {
        int processIndex = taskScrollOffset + i;

        const auto& process = taskProcesses[processIndex];

        int y = 220 + i * 27;

        bool isSelected =
            process.pid == taskSelectedPid;

        if (isSelected)
        {
            FillRounded(
                graphics,
                282, y,
                740, 26,
                5,
                Color(255, 39, 75, 125)
            );

            DrawRounded(
                graphics,
                282, y,
                740, 26,
                5,
                Color(255, 72, 132, 220)
            );
        }
        else if (i % 2 == 0)
        {
            FillRounded(
                graphics,
                282, y,
                740, 26,
                5,
                Color(255, 27, 34, 49)
            );
        }

        std::wstring processName = process.name;

        if (processName.length() > 25)
        {
            processName =
                processName.substr(0, 22) + L"...";
        }

        Color rowTextColor =
            isSelected ? TEXT_MAIN : TEXT_SECONDARY;

        DrawTextLeft(
            graphics,
            processName,
            columns[0],
            y,
            200,
            26,
            10,
            TEXT_MAIN,
            isSelected
        );

        DrawTextLeft(
            graphics,
            std::to_wstring(process.pid),
            columns[1],
            y,
            65,
            26,
            10,
            rowTextColor
        );

        std::wstring cpuText = L"—";

        if (process.cpuAvailable)
        {
            wchar_t buffer[32]{};

            swprintf_s(
                buffer,
                L"%.1f%%",
                process.cpuUsage
            );

            cpuText = buffer;
        }

        DrawTextLeft(
            graphics,
            cpuText,
            columns[2],
            y,
            70,
            26,
            10,
            process.cpuAvailable
            ? ACCENT_LIGHT
            : TEXT_MUTED
        );

        std::wstring memoryText =
            process.memoryAvailable
            ? FormatBytes(process.memoryUsage)
            : L"—";

        DrawTextLeft(
            graphics,
            memoryText,
            columns[3],
            y,
            100,
            26,
            10,
            process.memoryAvailable
            ? rowTextColor
            : TEXT_MUTED
        );

        std::wstring processStatus;

        Color processStatusColor = SUCCESS_COLOR;

        if (process.isSuspended)
        {
            processStatus = currentLanguage == 0
                ? L"Приостановлен"
                : L"Suspended";

            processStatusColor = WARNING_COLOR;
        }
        else if (!process.isResponding)
        {
            processStatus = currentLanguage == 0
                ? L"Не отвечает"
                : L"Not responding";

            processStatusColor = WARNING_COLOR;
        }
        else
        {
            processStatus = currentLanguage == 0
                ? L"Работает"
                : L"Running";
        }

        DrawTextLeft(
            graphics,
            processStatus,
            columns[4],
            y,
            130,
            26,
            10,
            processStatusColor
        );

        DrawTextLeft(
            graphics,
            process.isCritical
            ? L"TRUE"
            : L"FALSE",
            columns[5],
            y,
            100,
            26,
            10,
            process.isCritical
            ? WARNING_COLOR
            : rowTextColor,
            true
        );
    }

    // Полоса прокрутки.

    FillRounded(
        graphics,
        TASK_SCROLLBAR_X,
        TASK_SCROLLBAR_Y,
        TASK_SCROLLBAR_WIDTH,
        TASK_SCROLLBAR_HEIGHT,
        4,
        Color(255, 35, 44, 61)
    );

    int thumbHeight = TASK_SCROLLBAR_HEIGHT;
    int thumbY = TASK_SCROLLBAR_Y;
    int scrollbarMaxOffset = 0;

    GetTaskScrollbarGeometry(
        thumbHeight,
        thumbY,
        scrollbarMaxOffset
    );

    FillRounded(
        graphics,
        TASK_SCROLLBAR_X,
        thumbY,
        TASK_SCROLLBAR_WIDTH,
        thumbHeight,
        4,
        taskScrollbarDragging ? ACCENT_LIGHT : ACCENT
    );

    int firstProcess = taskScrollOffset + 1;
    int lastProcess = taskScrollOffset + visibleCount;

    std::wstring footerText;

    if (currentLanguage == 0)
    {
        footerText =
            L"Показано " +
            std::to_wstring(firstProcess) +
            L"–" +
            std::to_wstring(lastProcess) +
            L" из " +
            std::to_wstring(totalProcesses) +
            L". Выберите процесс для управления.";
    }
    else
    {
        footerText =
            L"Showing " +
            std::to_wstring(firstProcess) +
            L"–" +
            std::to_wstring(lastProcess) +
            L" of " +
            std::to_wstring(totalProcesses) +
            L". Select a process to manage it.";
    }

    DrawTextLeft(
        graphics,
        footerText,
        285, 586,
        730, 20,
        9, TEXT_MUTED
    );
}

// ============================================================
// ОЧИСТКА СИСТЕМЫ
// ============================================================

void DrawCleanupPage(Graphics& graphics)
{
    const wchar_t* names[2][4] =
    {
        {
            L"Временные файлы пользователя",
            L"Кэш эскизов Windows",
            L"Корзина",
            L"Кэш DNS"
        },
        {
            L"User temporary files",
            L"Windows thumbnail cache",
            L"Recycle Bin",
            L"DNS cache"
        }
    };

    const wchar_t* details[2][4] =
    {
        {
            L"Временные данные текущего пользователя",
            L"Миниатюры изображений и видео",
            L"Удалённые файлы Windows",
            L"Кэш сетевого резолвера"
        },
        {
            L"Temporary data of the current user",
            L"Image and video thumbnails",
            L"Deleted files in Windows",
            L"Network resolver cache"
        }
    };

    DrawTextLeft(
        graphics,
        currentLanguage == 0
        ? L"Выберите категории для очистки"
        : L"Select cleanup categories",
        270, 100, 700, 28,
        17, TEXT_MAIN, true
    );

    DrawTextLeft(
        graphics,
        currentLanguage == 0
        ? L"Доступные инструменты очистки"
        : L"Available cleanup tools",
        270, 126, 700, 18,
        10, TEXT_MUTED
    );

    for (int i = 0; i < 4; ++i)
    {
        int x = 270 + (i % 2) * 395;
        int y = 160 + (i / 2) * 130;

        cleanupItemRects[i] = {
            x, y,
            x + 375, y + 112
        };

        bool supported = IsCleanupCategorySupported(i);
        bool hovered = hoveredCleanupItem == i;

        FillRounded(
            graphics,
            x, y,
            375, 112,
            13,
            hovered && supported
            ? CARD_HOVER
            : CARD_COLOR
        );

        DrawRounded(
            graphics,
            x, y,
            375, 112,
            13,
            hovered && supported
            ? BORDER_HOVER
            : BORDER_COLOR
        );

        DrawCleanupCheckbox(
            graphics,
            x + 18,
            y + 21,
            cleanupSelected[i],
            supported,
            hovered
        );

        Color titleColor = supported
            ? TEXT_MAIN
            : TEXT_SECONDARY;

        DrawTextLeft(
            graphics,
            names[currentLanguage][i],
            x + 52, y + 12,
            305, 28,
            14, titleColor, true
        );

        DrawTextLeft(
            graphics,
            details[currentLanguage][i],
            x + 52, y + 40,
            305, 24,
            10, TEXT_SECONDARY
        );

        std::wstring result;
        Color resultColor = TEXT_SECONDARY;

        if (!supported)
        {
            result = currentLanguage == 0
                ? L"В разработке"
                : L"Coming soon";

            resultColor = TEXT_MUTED;
        }
        else if (cleanupScanned[i])
        {
            if (i == 3)
            {
                result = currentLanguage == 0
                    ? L"Готов к очистке"
                    : L"Ready to flush";
            }
            else
            {
                result =
                    (currentLanguage == 0
                        ? L"Найдено: "
                        : L"Found: ")
                    + FormatBytes(cleanupSizes[i])
                    + L"  •  "
                    + std::to_wstring(cleanupFileCounts[i])
                    + (currentLanguage == 0
                        ? L" файлов"
                        : L" files");
            }

            resultColor = ACCENT_LIGHT;
        }
        else
        {
            result = currentLanguage == 0
                ? L"Ожидает сканирования"
                : L"Waiting for scan";
        }

        DrawTextLeft(
            graphics,
            result,
            x + 52, y + 75,
            305, 23,
            10, resultColor
        );

        FillRounded(
            graphics,
            x + 345,
            y + 17,
            8, 8,
            4,
            supported
            ? SUCCESS_COLOR
            : TEXT_MUTED
        );
    }

    cleanupScanRect = {
        270, 440, 455, 485
    };

    DrawCleanupButton(
        graphics,
        cleanupScanRect,
        currentLanguage == 0
        ? L"Сканировать"
        : L"Scan",
        true,
        true
    );

    bool canClean =
        cleanupScanFinished &&
        HasScannedCleanupCategory();

    if (canClean)
    {
        cleanupCleanRect = {
            470, 440, 675, 485
        };

        DrawCleanupButton(
            graphics,
            cleanupCleanRect,
            currentLanguage == 0
            ? L"Очистить выбранное"
            : L"Clean selected",
            false,
            true
        );
    }
    else
    {
        cleanupCleanRect = {
            0, 0, 0, 0
        };
    }

    DrawTextLeft(
        graphics,
        currentLanguage == 0
        ? L"Корзина очищается без возможности восстановления; DNS-кэш сбрасывается штатной службой Windows."
        : L"Recycle Bin contents are permanently removed; DNS cache is flushed by the Windows resolver service.",
        270, 505, 760, 27,
        11, TEXT_SECONDARY
    );

    DrawTextLeft(
        graphics,
        currentLanguage == 0
        ? L"Перед удалением будет показано подтверждение."
        : L"A confirmation will be shown before deletion.",
        270, 530, 760, 24,
        10, TEXT_MUTED
    );
}

// ============================================================
// СТРОКА СОСТОЯНИЯ
// ============================================================

void DrawStatusBar(Graphics& graphics)
{
    SolidBrush brush(Color(255, 17, 21, 32));

    graphics.FillRectangle(
        &brush,
        SIDEBAR_WIDTH,
        WINDOW_HEIGHT - STATUS_HEIGHT,
        WINDOW_WIDTH - SIDEBAR_WIDTH,
        STATUS_HEIGHT
    );

    Pen separator(BORDER_COLOR, 1.0f);

    graphics.DrawLine(
        &separator,
        SIDEBAR_WIDTH,
        WINDOW_HEIGHT - STATUS_HEIGHT,
        WINDOW_WIDTH,
        WINDOW_HEIGHT - STATUS_HEIGHT
    );

    FillRounded(
        graphics,
        SIDEBAR_WIDTH + 24,
        WINDOW_HEIGHT - STATUS_HEIGHT + 13,
        7, 7, 4,
        SUCCESS_COLOR
    );

    DrawTextLeft(
        graphics,
        statusText,
        SIDEBAR_WIDTH + 42,
        WINDOW_HEIGHT - STATUS_HEIGHT,
        650,
        STATUS_HEIGHT,
        11,
        TEXT_SECONDARY
    );

    DrawTextLeft(
        graphics,
        L"UnlockTool",
        WINDOW_WIDTH - 130,
        WINDOW_HEIGHT - STATUS_HEIGHT,
        100,
        STATUS_HEIGHT,
        10,
        TEXT_MUTED
    );
}
// ============================================================
// PAGE TRANSITION: СОЗДАНИЕ СНИМКА ПРЕДЫДУЩЕЙ СТРАНИЦЫ
// ============================================================

void ClearPageTransition()
{
    delete pageTransitionBitmap;
    pageTransitionBitmap = nullptr;

    if (pageTransitionHbitmap)
    {
        DeleteObject(pageTransitionHbitmap);
        pageTransitionHbitmap = nullptr;
    }
}

void BeginPageTransition(HWND hwnd)
{
    ClearPageTransition();

    RECT rc{};
    GetClientRect(hwnd, &rc);

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    if (width <= 0 || height <= 0)
        return;

    HDC windowDC = GetDC(hwnd);

    if (!windowDC)
        return;

    HDC memoryDC = CreateCompatibleDC(windowDC);

    if (!memoryDC)
    {
        ReleaseDC(hwnd, windowDC);
        return;
    }

    pageTransitionHbitmap =
        CreateCompatibleBitmap(windowDC, width, height);

    if (!pageTransitionHbitmap)
    {
        DeleteDC(memoryDC);
        ReleaseDC(hwnd, windowDC);
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(
        memoryDC,
        pageTransitionHbitmap
    );

    bool captured = BitBlt(
        memoryDC,
        0,
        0,
        width,
        height,
        windowDC,
        0,
        0,
        SRCCOPY
    );

    SelectObject(memoryDC, oldBitmap);

    DeleteDC(memoryDC);
    ReleaseDC(hwnd, windowDC);

    if (!captured)
    {
        ClearPageTransition();
        return;
    }

    pageTransitionBitmap =
        Gdiplus::Bitmap::FromHBITMAP(
            pageTransitionHbitmap,
            nullptr
        );

    if (!pageTransitionBitmap ||
        pageTransitionBitmap->GetLastStatus() != Gdiplus::Ok)
    {
        ClearPageTransition();
        return;
    }

    pageTransitionStart = GetTickCount64();

    if (!SetTimer(
        hwnd,
        PAGE_TRANSITION_TIMER_ID,
        PAGE_TRANSITION_INTERVAL,
        nullptr))
    {
        ClearPageTransition();
    }
}
// ============================================================
// ОТРИСОВКА ОКНА
// ============================================================

void PaintInterface(HWND hwnd)
{
    PAINTSTRUCT ps{};

    HDC hdc = BeginPaint(hwnd, &ps);

    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);

    int width = (std::max)(1L, clientRect.right);
    int height = (std::max)(1L, clientRect.bottom);

    Bitmap bitmap(
        width,
        height,
        PixelFormat32bppARGB
    );

    Graphics graphics(&bitmap);

    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    graphics.SetTextRenderingHint(
        TextRenderingHintClearTypeGridFit
    );

    graphics.Clear(BG_COLOR);

    DrawSidebar(graphics);
    DrawHeader(graphics);

    if (currentPage == 0)
    {
        DrawHero(graphics);
        DrawCards(graphics, 285);
    }
    else if (currentPage == 1)
    {
        DrawTaskManagerPage(graphics);
    }
    else if (currentPage == 2)
    {
        UsersPage::Draw(graphics, currentLanguage);
    }
    else if (currentPage == 6)
    {
        BSODMonitorPage::Draw(graphics, currentLanguage);
    }
    else if (currentPage == 8)
    {
        RegistryPage::Draw(graphics, currentLanguage);
    }
    else if (currentPage == 9)
    {
        DiskManagerPage::Draw(graphics, currentLanguage);
    }
    else if (currentPage == 5)
    {
        DrawCleanupPage(graphics);
    }
    else if (currentPage == 7)
    {
        DrawLanguageSelector(graphics);
        DrawCards(graphics, 240);
    }
    else
    {
        DrawCards(graphics, 125);
    }

    DrawStatusBar(graphics);

    // ============================================================
 // PAGE TRANSITION: ОТРИСОВКА В БУФЕРЕ
 // ============================================================

    if (pageTransitionBitmap)
    {
        ULONGLONG elapsed =
            GetTickCount64() - pageTransitionStart;

        if (elapsed < PAGE_TRANSITION_DURATION)
        {
            float alpha =
                1.0f -
                static_cast<float>(elapsed) /
                PAGE_TRANSITION_DURATION;

            Gdiplus::ColorMatrix colorMatrix{};

            colorMatrix.m[0][0] = 1.0f;
            colorMatrix.m[1][1] = 1.0f;
            colorMatrix.m[2][2] = 1.0f;
            colorMatrix.m[3][3] = alpha;
            colorMatrix.m[4][4] = 1.0f;

            Gdiplus::ImageAttributes attributes;

            attributes.SetColorMatrix(&colorMatrix);

            graphics.DrawImage(
                pageTransitionBitmap,
                Gdiplus::Rect(0, 0, width, height),
                0,
                0,
                width,
                height,
                Gdiplus::UnitPixel,
                &attributes
            );
        }
    }

    // ============================================================
    // ВЫВОД ГОТОВОГО КАДРА НА ЭКРАН
    // ============================================================

    Graphics output(hdc);

    output.DrawImage(
        &bitmap,
        0, 0
    );

    EndPaint(hwnd, &ps);
}

// ============================================================
// НАВЕДЕНИЕ МЫШИ
// ============================================================

void UpdateHover(HWND hwnd, int x, int y)
{
    int oldMenu = hoveredMenu;
    int oldCard = hoveredCard;
    int oldCleanup = hoveredCleanupItem;

    hoveredMenu = -1;
    hoveredCard = -1;
    hoveredCleanupItem = -1;

    for (int i = 0; i < MENU_COUNT; ++i)
    {
        RECT r = menuRects[i];

        if (x >= r.left && x <= r.right &&
            y >= r.top && y <= r.bottom)
        {
            hoveredMenu = i;
        }
    }

    if (currentPage == 5)
    {
        for (int i = 0; i < 4; ++i)
        {
            RECT r = cleanupItemRects[i];

            if (IsCleanupCategorySupported(i) &&
                x >= r.left && x <= r.right &&
                y >= r.top && y <= r.bottom)
            {
                hoveredCleanupItem = i;
            }
        }
    }
    else if (currentPage != 1 && currentPage != 2 &&
        currentPage != 6 && currentPage != 9)
    {
        for (int i = 0; i < 4; ++i)
        {
            RECT r = cardRects[i];

            if (x >= r.left && x <= r.right &&
                y >= r.top && y <= r.bottom)
            {
                hoveredCard = i;
            }
        }
    }

    if (oldMenu != hoveredMenu ||
        oldCard != hoveredCard ||
        oldCleanup != hoveredCleanupItem)
    {
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    TRACKMOUSEEVENT event{};

    event.cbSize = sizeof(event);
    event.dwFlags = TME_LEAVE;
    event.hwndTrack = hwnd;

    TrackMouseEvent(&event);
}

// ============================================================
// TASK MANAGER: ЗАВЕРШЕНИЕ ПРОЦЕССА
// ============================================================

void EndSelectedTaskProcess(HWND hwnd)
{
    TaskManager::ProcessInfo* process =
        GetSelectedTaskProcess();

    if (!process)
    {
        statusText = currentLanguage == 0
            ? L"Сначала выберите процесс"
            : L"Select a process first";

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (process->isCritical)
    {
        statusText = currentLanguage == 0
            ? L"Защищённый системный процесс"
            : L"Protected system process";

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    std::wstring message =
        currentLanguage == 0
        ? L"Завершить процесс "
        : L"End process ";

    message += process->name;
    message += L"?";

    message += currentLanguage == 0
        ? L"\n\nНесохранённые данные приложения могут быть потеряны."
        : L"\n\nUnsaved application data may be lost.";

    int answer = MessageBoxW(
        hwnd,
        message.c_str(),
        L"UnlockTool",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
    );

    if (answer != IDYES)
        return;

    DWORD pid = process->pid;

    if (TaskManager::EndProcess(pid))
    {
        taskSelectedPid = 0;

        statusText = currentLanguage == 0
            ? L"Процесс завершён"
            : L"Process terminated";

        RefreshTaskProcesses(hwnd, false);
    }
    else
    {
        statusText = currentLanguage == 0
            ? L"Не удалось завершить процесс"
            : L"Could not terminate process";
    }

    InvalidateRect(hwnd, nullptr, FALSE);
}

// ============================================================
// TASK MANAGER: ПРИОСТАНОВКА ПРОЦЕССА
// ============================================================

void SuspendSelectedTaskProcess(HWND hwnd)
{
    TaskManager::ProcessInfo* process =
        GetSelectedTaskProcess();

    if (!process)
    {
        statusText = currentLanguage == 0
            ? L"Сначала выберите процесс"
            : L"Select a process first";

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (process->isCritical)
    {
        statusText = currentLanguage == 0
            ? L"Защищённый системный процесс"
            : L"Protected system process";

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (process->isSuspended)
    {
        statusText = currentLanguage == 0
            ? L"Процесс уже приостановлен"
            : L"Process is already suspended";

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    DWORD pid = process->pid;

    if (TaskManager::SuspendProcess(pid))
    {
        statusText = currentLanguage == 0
            ? L"Процесс приостановлен"
            : L"Process suspended";

        RefreshTaskProcesses(hwnd, false);
    }
    else
    {
        statusText = currentLanguage == 0
            ? L"Не удалось приостановить процесс"
            : L"Could not suspend process";
    }

    InvalidateRect(hwnd, nullptr, FALSE);
}

// ============================================================
// TASK MANAGER: ВОЗОБНОВЛЕНИЕ ПРОЦЕССА
// ============================================================

void ResumeSelectedTaskProcess(HWND hwnd)
{
    TaskManager::ProcessInfo* process =
        GetSelectedTaskProcess();

    if (!process)
    {
        statusText = currentLanguage == 0
            ? L"Сначала выберите процесс"
            : L"Select a process first";

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (!process->isSuspended)
    {
        statusText = currentLanguage == 0
            ? L"Процесс не приостановлен"
            : L"Process is not suspended";

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    DWORD pid = process->pid;

    if (TaskManager::ResumeProcess(pid))
    {
        statusText = currentLanguage == 0
            ? L"Процесс возобновлён"
            : L"Process resumed";

        RefreshTaskProcesses(hwnd, false);
    }
    else
    {
        statusText = currentLanguage == 0
            ? L"Не удалось возобновить процесс"
            : L"Could not resume process";
    }

    InvalidateRect(hwnd, nullptr, FALSE);
}

// ============================================================
// СКАНИРОВАНИЕ ВЫБРАННЫХ КАТЕГОРИЙ
// ============================================================

void ScanTemporaryFiles(HWND hwnd)
{
    if (!HasSelectedCleanupCategory())
    {
        statusText = currentLanguage == 0
            ? L"Выберите категорию для сканирования"
            : L"Select a category to scan";

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    ResetCleanupResults();

    statusText = currentLanguage == 0
        ? L"Выполняется сканирование..."
        : L"Scanning...";

    InvalidateRect(hwnd, nullptr, FALSE);
    UpdateWindow(hwnd);

    auto items = Cleanup::GetCleanupItems();

    bool anySuccess = false;

    for (int i = 0; i < 4; ++i)
    {
        if (!cleanupSelected[i])
            continue;

        const Cleanup::CleanupType types[4] =
        {
            Cleanup::CleanupType::UserTemp,
            Cleanup::CleanupType::ThumbnailCache,
            Cleanup::CleanupType::RecycleBin,
            Cleanup::CleanupType::DnsCache
        };
        Cleanup::CleanupType type = types[i];

        Cleanup::CleanupItem* item =
            FindCleanupItem(items, type);

        if (!item)
            continue;

        if (Cleanup::ScanItem(*item))
        {
            cleanupSizes[i] = item->size;
            cleanupFileCounts[i] = item->filesCount;

            cleanupScanned[i] = true;

            anySuccess = true;
        }
    }

    cleanupScanFinished = anySuccess;

    if (anySuccess)
    {
        statusText = currentLanguage == 0
            ? L"Сканирование завершено"
            : L"Scan completed";
    }
    else
    {
        statusText = currentLanguage == 0
            ? L"Не удалось просканировать выбранные категории"
            : L"Could not scan selected categories";
    }

    InvalidateRect(hwnd, nullptr, FALSE);
}

// ============================================================
// ОЧИСТКА ВЫБРАННЫХ КАТЕГОРИЙ
// ============================================================

void CleanTemporaryFiles(HWND hwnd)
{
    if (!cleanupScanFinished ||
        !HasScannedCleanupCategory())
    {
        return;
    }

    const bool recycleBinSelected =
        cleanupSelected[2] && cleanupScanned[2];

    std::wstring confirmation = currentLanguage == 0
        ? L"Вы действительно хотите очистить выбранные категории?\n\nЗанятые и недоступные файлы будут пропущены."
        : L"Do you really want to clean the selected categories?\n\nFiles in use or inaccessible files will be skipped.";

    if (recycleBinSelected)
    {
        confirmation += currentLanguage == 0
            ? L"\n\nВНИМАНИЕ: файлы из Корзины будут удалены без возможности восстановления."
            : L"\n\nWARNING: Recycle Bin contents will be permanently deleted.";
    }

    const int answer = MessageBoxW(
        hwnd,
        confirmation.c_str(),
        L"UnlockTool",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
    );

    if (answer != IDYES)
        return;

    auto items = Cleanup::GetCleanupItems();

    uint64_t totalDeletedSize = 0;
    unsigned int totalDeletedFiles = 0;

    bool anySuccess = false;

    for (int i = 0; i < 4; ++i)
    {
        if (!cleanupSelected[i] || !cleanupScanned[i])
            continue;

        const Cleanup::CleanupType types[4] =
        {
            Cleanup::CleanupType::UserTemp,
            Cleanup::CleanupType::ThumbnailCache,
            Cleanup::CleanupType::RecycleBin,
            Cleanup::CleanupType::DnsCache
        };
        Cleanup::CleanupType type = types[i];

        Cleanup::CleanupItem* item =
            FindCleanupItem(items, type);

        if (!item)
            continue;

        if (!Cleanup::ScanItem(*item))
            continue;

        if (!Cleanup::CleanItem(*item))
            continue;

        cleanupDeletedSizes[i] = item->deletedSize;
        cleanupDeletedFiles[i] = item->deletedFiles;

        cleanupSizes[i] = item->size;
        cleanupFileCounts[i] = item->filesCount;

        cleanupScanned[i] = true;

        totalDeletedSize += item->deletedSize;
        totalDeletedFiles += item->deletedFiles;

        anySuccess = true;
    }

    if (!anySuccess)
    {
        statusText = currentLanguage == 0
            ? L"Не удалось выполнить очистку"
            : L"Cleanup failed";

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    std::wstring message =
        currentLanguage == 0
        ? L"Удалено файлов: "
        : L"Deleted files: ";

    message += std::to_wstring(totalDeletedFiles);

    message += currentLanguage == 0
        ? L"\nОсвобождено: "
        : L"\nFreed space: ";

    message += FormatBytes(totalDeletedSize);

    if (cleanupSelected[3] && cleanupScanned[3])
    {
        message += currentLanguage == 0
            ? L"\nКэш DNS сброшен."
            : L"\nDNS cache flushed.";
    }

    statusText = message;

    MessageBoxW(
        hwnd,
        message.c_str(),
        L"UnlockTool",
        MB_OK | MB_ICONINFORMATION
    );

    InvalidateRect(hwnd, nullptr, FALSE);
}

// ============================================================
// BSOD MONITOR: STARTUP NOTIFICATION STATE
// ============================================================

void RemoveBsodTrayIcon()
{
    if (bsodTrayIconAdded)
    {
        Shell_NotifyIconW(NIM_DELETE, &bsodTrayIcon);
        bsodTrayIconAdded = false;
    }
}

void MarkBsodIncidentsViewed()
{
    if (settingsPath[0] == L'\0')
        return;

    const auto latest = BSODMonitor::GetIncidents(1);
    if (!latest.empty())
    {
        WritePrivateProfileStringW(
            L"BSODMonitor",
            L"LastSeenTimestamp",
            latest.front().timestamp.c_str(),
            settingsPath);
    }

    RemoveBsodTrayIcon();
}

void CheckBsodStartupNotification(HWND hwnd, HINSTANCE instance)
{
    if (settingsPath[0] == L'\0')
        return;

    const auto latest = BSODMonitor::GetIncidents(1);
    if (latest.empty() || latest.front().timestamp.empty())
        return;

    wchar_t lastSeen[128]{};
    GetPrivateProfileStringW(
        L"BSODMonitor",
        L"LastSeenTimestamp",
        L"",
        lastSeen,
        128,
        settingsPath);

    if (latest.front().timestamp == lastSeen)
        return;

    bsodTrayIcon = NOTIFYICONDATAW{};
    bsodTrayIcon.cbSize = sizeof(bsodTrayIcon);
    bsodTrayIcon.hWnd = hwnd;
    bsodTrayIcon.uID = 2;
    bsodTrayIcon.uFlags = NIF_ICON | NIF_TIP;
    bsodTrayIcon.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!bsodTrayIcon.hIcon)
        bsodTrayIcon.hIcon = LoadIconW(nullptr, IDI_WARNING);
    wcscpy_s(bsodTrayIcon.szTip, L"UnlockTool — BSOD Monitor");

    if (Shell_NotifyIconW(NIM_ADD, &bsodTrayIcon))
    {
        bsodTrayIconAdded = true;
        bsodTrayIcon.uFlags = NIF_INFO;
        wcscpy_s(bsodTrayIcon.szInfoTitle,
            currentLanguage == 0 ? L"UnlockTool: обнаружен BSOD" : L"UnlockTool: BSOD detected");
        wcscpy_s(bsodTrayIcon.szInfo,
            currentLanguage == 0
                ? L"В журнале Windows обнаружена новая запись BugCheck. Откройте BSOD Monitor для просмотра."
                : L"A new BugCheck event was found in Windows Event Log. Open BSOD Monitor to review it.");
        bsodTrayIcon.dwInfoFlags = NIIF_WARNING;
        bsodTrayIcon.uTimeout = 8000;
        Shell_NotifyIconW(NIM_MODIFY, &bsodTrayIcon);
    }
}

// ============================================================
// SYSTEM TOOLS: SAFE LAUNCH HELPERS
// ============================================================

bool IsWinREEnvironment()
{
    HKEY key = nullptr;
    const LONG result = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\MiniNT",
        0,
        KEY_QUERY_VALUE,
        &key);

    if (result == ERROR_SUCCESS)
    {
        RegCloseKey(key);
        return true;
    }

    return false;
}

bool LaunchSystemItem(
    HWND hwnd,
    const wchar_t* target,
    const wchar_t* parameters = nullptr,
    const wchar_t* verb = L"open")
{
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
        hwnd, verb, target, parameters, nullptr, SW_SHOWNORMAL));

    if (result > 32)
    {
        statusText = currentLanguage == 0
            ? L"Системный инструмент запущен"
            : L"System tool launched";
        return true;
    }

    statusText = currentLanguage == 0
        ? L"Не удалось запустить системный инструмент"
        : L"Could not launch the system tool";
    MessageBoxW(hwnd, statusText.c_str(), L"UnlockTool", MB_OK | MB_ICONERROR);
    return false;
}

bool LaunchElevatedCommand(HWND hwnd, const std::wstring& command)
{
    const std::wstring parameters = L"/k \"" + command + L"\"";
    return LaunchSystemItem(hwnd, L"cmd.exe", parameters.c_str(), L"runas");
}

void ShowRecoveryCommandMenu(HWND hwnd)
{
    if (IsWinREEnvironment())
    {
        MessageBoxW(
            hwnd,
            currentLanguage == 0
                ? L"Обнаружена среда восстановления Windows. Команды /Online будут воздействовать на текущую среду WinRE, а не на установленную Windows. Автоматический выбор офлайн-образа пока не подключён."
                : L"Windows Recovery Environment detected. /Online commands would target the current WinRE image, not the installed Windows system. Offline image selection is not connected yet.",
            L"UnlockTool",
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    AppendMenuW(menu, MF_STRING, 1,
        currentLanguage == 0 ? L"SFC — проверка и восстановление" : L"SFC — scan and repair");
    AppendMenuW(menu, MF_STRING, 2,
        currentLanguage == 0 ? L"DISM — восстановление хранилища" : L"DISM — repair component store");
    AppendMenuW(menu, MF_STRING, 3,
        currentLanguage == 0 ? L"CHKDSK — проверка тома" : L"CHKDSK — scan volume");

    POINT point{};
    GetCursorPos(&point);
    const UINT command = TrackPopupMenu(
        menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
        point.x, point.y, 0, hwnd, nullptr);
    DestroyMenu(menu);

    if (command == 1)
        LaunchElevatedCommand(hwnd, L"sfc /scannow");
    else if (command == 2)
        LaunchElevatedCommand(hwnd, L"DISM /Online /Cleanup-Image /RestoreHealth");
    else if (command == 3)
    {
        wchar_t windowsDirectory[MAX_PATH]{};
        GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
        std::wstring drive = windowsDirectory;
        if (drive.size() >= 2 && drive[1] == L':')
            LaunchElevatedCommand(hwnd, L"chkdsk " + drive.substr(0, 2) + L" /scan");
        else
            statusText = currentLanguage == 0
                ? L"Не удалось определить системный том"
                : L"Could not determine the system volume";
    }
}

bool BackupAndEnableUAC(HWND hwnd)
{
    if (IsWinREEnvironment())
    {
        MessageBoxW(
            hwnd,
            currentLanguage == 0
                ? L"В среде WinRE нельзя безопасно менять UAC текущего образа. Сначала выберите установленную Windows."
                : L"UAC cannot be safely changed in the current WinRE image. Select the installed Windows system first.",
            L"UnlockTool",
            MB_OK | MB_ICONINFORMATION);
        return false;
    }

    HKEY key = nullptr;
    const wchar_t* keyPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
    LONG error = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE, keyPath, 0,
        KEY_QUERY_VALUE | KEY_SET_VALUE, &key);
    if (error != ERROR_SUCCESS)
    {
        statusText = currentLanguage == 0 ? L"Не удалось открыть раздел UAC" : L"Could not open the UAC registry key";
        return false;
    }

    DWORD oldValue = 0;
    DWORD type = REG_NONE;
    DWORD size = sizeof(oldValue);
    error = RegQueryValueExW(
        key, L"EnableLUA", nullptr, &type,
        reinterpret_cast<LPBYTE>(&oldValue), &size);
    const bool existed = error == ERROR_SUCCESS;
    if (existed && (type != REG_DWORD || size != sizeof(oldValue)))
    {
        RegCloseKey(key);
        statusText = currentLanguage == 0 ? L"Неизвестный тип параметра EnableLUA" : L"Unsupported EnableLUA registry type";
        return false;
    }
    if (!existed && error != ERROR_FILE_NOT_FOUND)
    {
        RegCloseKey(key);
        statusText = currentLanguage == 0 ? L"Не удалось прочитать EnableLUA" : L"Could not read EnableLUA";
        return false;
    }
    if (existed && oldValue == 1)
    {
        RegCloseKey(key);
        statusText = currentLanguage == 0 ? L"UAC уже включён" : L"UAC is already enabled";
        return true;
    }

    const int answer = MessageBoxW(
        hwnd,
        currentLanguage == 0
            ? L"Будет создан .reg-файл с исходным значением EnableLUA, затем UAC будет включён. Для применения потребуется перезагрузка. Продолжить?"
            : L"A .reg backup of the original EnableLUA value will be created, then UAC will be enabled. A restart is required. Continue?",
        L"UnlockTool",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (answer != IDYES)
    {
        RegCloseKey(key);
        return false;
    }

    PWSTR localAppDataRaw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppDataRaw)))
    {
        RegCloseKey(key);
        statusText = currentLanguage == 0 ? L"Не удалось получить LocalAppData" : L"Could not locate LocalAppData";
        return false;
    }
    std::wstring backupDirectory = localAppDataRaw;
    CoTaskMemFree(localAppDataRaw);
    backupDirectory += L"\\UnlockTool";
    CreateDirectoryW(backupDirectory.c_str(), nullptr);
    backupDirectory += L"\\RegistryBackups";
    CreateDirectoryW(backupDirectory.c_str(), nullptr);
    const std::wstring backupPath = backupDirectory + L"\\EnableUAC-backup.reg";

    std::wstring regText = L"Windows Registry Editor Version 5.00\r\n\r\n";
    regText += L"[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System]\r\n";
    if (existed)
    {
        wchar_t valueText[32]{};
        swprintf_s(valueText, L"%08X", oldValue);
        regText += L"\"EnableLUA\"=dword:" + std::wstring(valueText) + L"\r\n";
    }
    else
    {
        regText += L"\"EnableLUA\"=-\r\n";
    }

    HANDLE backupFile = CreateFileW(
        backupPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (backupFile == INVALID_HANDLE_VALUE)
    {
        RegCloseKey(key);
        statusText = currentLanguage == 0 ? L"Не удалось создать резервную копию UAC" : L"Could not create UAC backup";
        return false;
    }
    const BYTE bom[] = { 0xFF, 0xFE };
    DWORD written = 0;
    bool backupOk = WriteFile(backupFile, bom, sizeof(bom), &written, nullptr) && written == sizeof(bom);
    if (backupOk)
    {
        const DWORD bytes = static_cast<DWORD>(regText.size() * sizeof(wchar_t));
        backupOk = WriteFile(backupFile, regText.data(), bytes, &written, nullptr) && written == bytes;
    }
    CloseHandle(backupFile);

    if (!backupOk)
    {
        RegCloseKey(key);
        statusText = currentLanguage == 0 ? L"Не удалось записать резервную копию UAC" : L"Could not write UAC backup";
        return false;
    }

    const DWORD enabled = 1;
    error = RegSetValueExW(
        key, L"EnableLUA", 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&enabled), sizeof(enabled));
    if (error == ERROR_SUCCESS)
        RegFlushKey(key);
    RegCloseKey(key);

    if (error != ERROR_SUCCESS)
    {
        statusText = currentLanguage == 0 ? L"Не удалось включить UAC" : L"Could not enable UAC";
        return false;
    }

    statusText = currentLanguage == 0
        ? L"UAC включён. Резервная копия сохранена; требуется перезагрузка."
        : L"UAC enabled. Backup saved; restart required.";
    MessageBoxW(hwnd, statusText.c_str(), L"UnlockTool", MB_OK | MB_ICONINFORMATION);
    return true;
}

void ShowAdvancedRecoveryMenu(HWND hwnd)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    AppendMenuW(menu, MF_STRING, 1,
        currentLanguage == 0 ? L"Открыть параметры восстановления" : L"Open recovery settings");
    AppendMenuW(menu, MF_STRING, 2,
        currentLanguage == 0 ? L"Перезагрузить в среду восстановления" : L"Restart to Windows Recovery Environment");
    AppendMenuW(menu, MF_STRING, 3,
        currentLanguage == 0 ? L"Включить UAC" : L"Enable UAC");

    POINT point{};
    GetCursorPos(&point);
    const UINT command = TrackPopupMenu(
        menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
        point.x, point.y, 0, hwnd, nullptr);
    DestroyMenu(menu);

    if (command == 1)
    {
        LaunchSystemItem(hwnd, L"ms-settings:recovery");
    }
    else if (command == 2)
    {
        if (IsWinREEnvironment())
        {
            MessageBoxW(
                hwnd,
                currentLanguage == 0
                    ? L"UnlockTool уже запущен в среде восстановления Windows."
                    : L"UnlockTool is already running in Windows Recovery Environment.",
                L"UnlockTool",
                MB_OK | MB_ICONINFORMATION);
            return;
        }

        const int answer = MessageBoxW(
            hwnd,
            currentLanguage == 0
                ? L"Компьютер будет перезагружен в дополнительные параметры запуска Windows. Сохраните открытые документы. Продолжить?"
                : L"The computer will restart into Windows Advanced Startup. Save open documents before continuing. Proceed?",
            L"UnlockTool",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

        if (answer == IDYES)
            LaunchSystemItem(hwnd, L"shutdown.exe", L"/r /o /t 0", L"runas");
    }
    else if (command == 3)
    {
        BackupAndEnableUAC(hwnd);
    }
}

bool IsStartupEnabled()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_QUERY_VALUE,
        &key) != ERROR_SUCCESS)
        return false;

    DWORD type = 0;
    DWORD size = 0;
    const LONG query = RegQueryValueExW(
        key, L"UnlockTool", nullptr, &type, nullptr, &size);
    RegCloseKey(key);
    return query == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ);
}

bool SetStartupEnabled(bool enabled)
{
    HKEY key = nullptr;
    DWORD disposition = 0;
    const LONG open = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, &disposition);
    if (open != ERROR_SUCCESS)
        return false;

    LONG result = ERROR_SUCCESS;
    if (enabled)
    {
        wchar_t executable[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
        {
            RegCloseKey(key);
            return false;
        }

        const std::wstring command = L"\"" + std::wstring(executable) + L"\"";
        const DWORD bytes = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
        result = RegSetValueExW(
            key, L"UnlockTool", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()), bytes);
    }
    else
    {
        result = RegDeleteValueW(key, L"UnlockTool");
        if (result == ERROR_FILE_NOT_FOUND)
            result = ERROR_SUCCESS;
    }

    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

void ShowNotificationOptionsMenu(HWND hwnd)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    AppendMenuW(menu, MF_STRING, 1,
        currentLanguage == 0 ? L"Параметры уведомлений Windows" : L"Windows notification settings");

    const bool enabled = IsStartupEnabled();
    AppendMenuW(menu, MF_STRING, 2,
        enabled
            ? (currentLanguage == 0 ? L"Отключить запуск UnlockTool при входе" : L"Disable UnlockTool at sign-in")
            : (currentLanguage == 0 ? L"Запускать UnlockTool при входе" : L"Start UnlockTool at sign-in"));

    POINT point{};
    GetCursorPos(&point);
    const UINT command = TrackPopupMenu(
        menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
        point.x, point.y, 0, hwnd, nullptr);
    DestroyMenu(menu);

    if (command == 1)
    {
        LaunchSystemItem(hwnd, L"ms-settings:notifications");
    }
    else if (command == 2)
    {
        if (!enabled)
        {
            const int answer = MessageBoxW(
                hwnd,
                currentLanguage == 0
                    ? L"UnlockTool будет запускаться при входе в Windows, чтобы проверять новые записи BSOD и показывать уведомления. Включить?"
                    : L"UnlockTool will start when you sign in to Windows to check for new BSOD events and show notifications. Enable it?",
                L"UnlockTool",
                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
            if (answer != IDYES)
                return;
        }

        const bool success = SetStartupEnabled(!enabled);
        statusText = success
            ? (enabled
                ? (currentLanguage == 0 ? L"Автозапуск отключён" : L"Startup launch disabled")
                : (currentLanguage == 0 ? L"Автозапуск включён" : L"Startup launch enabled"))
            : (currentLanguage == 0 ? L"Не удалось изменить автозапуск" : L"Could not change startup setting");
    }
}

void StartUpdateCheck(HWND hwnd, bool interactive)
{
    if (updateCheckRunning)
    {
        if (interactive)
        {
            MessageBoxW(
                hwnd,
                currentLanguage == 0
                    ? L"Проверка обновлений уже выполняется."
                    : L"An update check is already running.",
                L"UnlockTool",
                MB_OK | MB_ICONINFORMATION);
        }
        return;
    }

    updateCheckRunning = true;
    statusText = currentLanguage == 0
        ? L"Проверка обновлений GitHub..."
        : L"Checking GitHub for updates...";

    if (!UpdateChecker::CheckAsync(
        hwnd,
        UPDATE_CHECK_COMPLETED_MESSAGE,
        APP_VERSION,
        interactive))
    {
        updateCheckRunning = false;
        statusText = currentLanguage == 0
            ? L"Не удалось запустить проверку обновлений"
            : L"Could not start the update check";

        if (interactive)
        {
            MessageBoxW(
                hwnd,
                statusText.c_str(),
                L"UnlockTool",
                MB_OK | MB_ICONERROR);
        }
    }

    InvalidateRect(hwnd, nullptr, FALSE);
}

void ShowUpdateOptionsMenu(HWND hwnd)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    AppendMenuW(
        menu,
        MF_STRING,
        1,
        currentLanguage == 0
            ? L"Проверить обновления сейчас"
            : L"Check for updates now");

    AppendMenuW(
        menu,
        MF_STRING | (autoCheckUpdates ? MF_CHECKED : 0),
        2,
        currentLanguage == 0
            ? L"Автоматически проверять при запуске"
            : L"Check automatically at startup");

    POINT point{};
    GetCursorPos(&point);

    const UINT command = TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_RETURNCMD,
        point.x,
        point.y,
        0,
        hwnd,
        nullptr);

    DestroyMenu(menu);

    if (command == 1)
    {
        StartUpdateCheck(hwnd, true);
    }
    else if (command == 2)
    {
        autoCheckUpdates = !autoCheckUpdates;
        SaveAutoCheckUpdates();

        statusText = autoCheckUpdates
            ? (currentLanguage == 0
                ? L"Автоматическая проверка обновлений включена"
                : L"Automatic update checks enabled")
            : (currentLanguage == 0
                ? L"Автоматическая проверка обновлений отключена"
                : L"Automatic update checks disabled");

        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void HandleUpdateCheckCompleted(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<UpdateChecker::Result> result(
        reinterpret_cast<UpdateChecker::Result*>(lParam));

    updateCheckRunning = false;

    if (!result)
        return;

    const bool interactive = wParam != 0;

    if (!result->success)
    {
        statusText = currentLanguage == 0
            ? L"Не удалось проверить обновления"
            : L"Could not check for updates";

        if (interactive)
        {
            const std::wstring message = currentLanguage == 0
                ? L"Не удалось проверить обновления. Проверьте подключение к интернету или повторите попытку позже.\n\nПодробности: " + result->error
                : std::wstring(L"Could not check for updates. Check your internet connection or try again later.");

            MessageBoxW(
                hwnd,
                message.c_str(),
                L"UnlockTool Update Checker",
                MB_OK | MB_ICONWARNING);
        }
    }
    else if (result->updateAvailable)
    {
        statusText = currentLanguage == 0
            ? L"Доступно обновление UnlockTool"
            : L"An UnlockTool update is available";

        std::wstring message =
            std::wstring(currentLanguage == 0
                ? L"Доступна новая версия UnlockTool!\n\n"
                : L"A new version of UnlockTool is available!\n\n") +
            (currentLanguage == 0 ? L"Установлена версия: " : L"Installed version: ") +
            result->currentVersion + L"\n" +
            (currentLanguage == 0 ? L"Новая версия: " : L"Latest version: ") +
            result->latestVersion;

        if (!result->releaseName.empty())
            message += L"\n\n" + result->releaseName;

        if (!result->releaseNotes.empty())
        {
            std::wstring notes = result->releaseNotes;
            if (notes.size() > 1800)
                notes = notes.substr(0, 1800) + L"...";

            message += L"\n\n" +
                std::wstring(currentLanguage == 0
                    ? L"Описание релиза:\n"
                    : L"Release notes:\n") +
                notes;
        }

        message += currentLanguage == 0
            ? L"\n\nОткрыть страницу релиза на GitHub?"
            : L"\n\nOpen the GitHub release page?";

        const int answer = MessageBoxW(
            hwnd,
            message.c_str(),
            L"UnlockTool Update Checker",
            MB_YESNO | MB_ICONINFORMATION);

        if (answer == IDYES && !result->releaseUrl.empty())
        {
            ShellExecuteW(
                hwnd,
                L"open",
                result->releaseUrl.c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL);
        }
    }
    else if (result->currentVersionNewer)
    {
        statusText = currentLanguage == 0
            ? L"Используется версия новее последнего публичного релиза"
            : L"This build is newer than the latest public release";

        if (interactive)
        {
            const std::wstring message = currentLanguage == 0
                ? L"Установленная версия " + result->currentVersion +
                    L" новее последнего публичного релиза " + result->latestVersion +
                    L". Возможно, это ещё не опубликованная сборка."
                : L"Installed version " + result->currentVersion +
                    L" is newer than the latest public release " + result->latestVersion +
                    L". This may be an unpublished build.";

            MessageBoxW(
                hwnd,
                message.c_str(),
                L"UnlockTool Update Checker",
                MB_OK | MB_ICONINFORMATION);
        }
    }
    else
    {
        statusText = currentLanguage == 0
            ? L"Установлена последняя версия UnlockTool"
            : L"UnlockTool is up to date";

        if (interactive)
        {
            const std::wstring message =
                (currentLanguage == 0
                    ? L"У вас установлена последняя версия UnlockTool: "
                    : L"You have the latest version of UnlockTool: ") +
                result->currentVersion;

            MessageBoxW(
                hwnd,
                message.c_str(),
                L"UnlockTool Update Checker",
                MB_OK | MB_ICONINFORMATION);
        }
    }

    InvalidateRect(hwnd, nullptr, FALSE);
}

bool HandleUtilityCardClick(HWND hwnd, int page, int card)
{
    if (page == 3) // Diagnostics
    {
        switch (card)
        {
        case 0: return LaunchSystemItem(hwnd, L"msinfo32.exe");
        case 1: return LaunchSystemItem(hwnd, L"cmd.exe", L"/k sfc /verifyonly", L"runas");
        case 2: return LaunchSystemItem(hwnd, L"services.msc");
        case 3: return LaunchSystemItem(hwnd, L"eventvwr.msc");
        }
    }
    else if (page == 4) // Recovery
    {
        switch (card)
        {
        case 0: return LaunchSystemItem(hwnd, L"wsreset.exe");
        case 1: return LaunchSystemItem(hwnd, L"ms-settings:windowsupdate");
        case 2:
            ShowRecoveryCommandMenu(hwnd);
            return true;
        case 3:
            ShowAdvancedRecoveryMenu(hwnd);
            return true;
        }
    }
    else if (page == 6) // BSOD Monitor shortcuts
    {
        switch (card)
        {
        case 0: return LaunchSystemItem(hwnd, L"perfmon.exe", L"/rel");
        case 1: return LaunchSystemItem(hwnd, L"eventvwr.msc");
        case 2:
        {
            wchar_t windowsDirectory[MAX_PATH]{};
            GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
            std::wstring dumpDirectory = windowsDirectory;
            dumpDirectory += L"\\Minidump";
            const DWORD attributes = GetFileAttributesW(dumpDirectory.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES ||
                !(attributes & FILE_ATTRIBUTE_DIRECTORY))
                dumpDirectory = windowsDirectory;
            return LaunchSystemItem(hwnd, L"explorer.exe", dumpDirectory.c_str());
        }
        case 3: return LaunchSystemItem(hwnd, L"ms-settings:backup");
        }
    }
    else if (page == 7) // Settings
    {
        switch (card)
        {
        case 0:
            ShowUpdateOptionsMenu(hwnd);
            return true;
        case 1:
            ShowNotificationOptionsMenu(hwnd);
            return true;
        case 2: return LaunchSystemItem(hwnd, L"windowsdefender:");
        case 3:
            MessageBoxW(
                hwnd,
                currentLanguage == 0
                    ? L"UnlockTool 2.0.1\nСредство обслуживания и восстановления Windows\n\nC++ / Win32 API"
                    : L"UnlockTool 2.0.1\nWindows maintenance and recovery utility\n\nC++ / Win32 API",
                L"About UnlockTool",
                MB_OK | MB_ICONINFORMATION);
            statusText = currentLanguage == 0 ? L"О программе" : L"About UnlockTool";
            return true;
        }
    }

    return false;
}

// ============================================================
// ОБРАБОТКА КЛИКОВ
// ============================================================

void HandleClick(HWND hwnd, int x, int y)
{
    // Переключение разделов.

    for (int i = 0; i < MENU_COUNT; ++i)
    {
        RECT r = menuRects[i];

        if (x >= r.left && x <= r.right &&
            y >= r.top && y <= r.bottom)
        {
            if (taskScrollbarDragging)
            {
                taskScrollbarDragging = false;

                if (GetCapture() == hwnd)
                    ReleaseCapture();
            }

            if (currentPage != i)
            {
                BeginPageTransition(hwnd);
            }

            currentPage = i;

            hoveredCard = -1;
            hoveredCleanupItem = -1;

            if (currentPage == 1)
            {
                SetTimer(
                    hwnd,
                    TASK_REFRESH_TIMER_ID,
                    TASK_REFRESH_INTERVAL,
                    nullptr
                );

                if (!taskProcessesLoaded)
                    RefreshTaskProcesses(hwnd);
            }
            else
            {
                KillTimer(
                    hwnd,
                    TASK_REFRESH_TIMER_ID
                );
            }

            statusText =
                std::wstring(
                    currentLanguage == 0
                    ? L"Открыт раздел: "
                    : L"Opened section: "
                ) + menuNames[currentLanguage][i];

            if (currentPage == 2)
            {
                UsersPage::Open(
                    hwnd,
                    currentLanguage,
                    statusText
                );
            }
            if (currentPage == 6)
            {
                BSODMonitorPage::Open(
                    hwnd,
                    currentLanguage,
                    statusText
                );
                MarkBsodIncidentsViewed();
            }
            if (currentPage == 8)
            {
                RegistryPage::Open(
                    hwnd,
                    currentLanguage,
                    statusText
                );
            }
            if (currentPage == 9)
            {
                DiskManagerPage::Open(
                    hwnd,
                    currentLanguage,
                    statusText
                );
            }

            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
    }

    // Диспетчер задач.

    if (currentPage == 1)
    {
        // Открытие вкладки автозагрузки Windows.

        RECT startup = taskStartupRect;
        if (x >= startup.left && x <= startup.right &&
            y >= startup.top && y <= startup.bottom)
        {
            LaunchSystemItem(hwnd, L"taskmgr.exe", L"/0 /startup");
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }

        // Обновление.

        RECT refresh = taskRefreshRect;

        if (x >= refresh.left && x <= refresh.right &&
            y >= refresh.top && y <= refresh.bottom)
        {
            RefreshTaskProcesses(hwnd);
            return;
        }

        // Завершение процесса.

        RECT end = taskEndRect;

        if (x >= end.left && x <= end.right &&
            y >= end.top && y <= end.bottom)
        {
            EndSelectedTaskProcess(hwnd);
            return;
        }

        // Приостановка процесса.

        RECT suspend = taskSuspendRect;

        if (x >= suspend.left && x <= suspend.right &&
            y >= suspend.top && y <= suspend.bottom)
        {
            SuspendSelectedTaskProcess(hwnd);
            return;
        }

        // Возобновление процесса.

        RECT resume = taskResumeRect;

        if (x >= resume.left && x <= resume.right &&
            y >= resume.top && y <= resume.bottom)
        {
            ResumeSelectedTaskProcess(hwnd);
            return;
        }

        // Выбор строки процесса.

        if (x >= 282 && x <= 1022 &&
            y >= 220 && y < 220 + TASK_MAX_VISIBLE_PROCESSES * 27)
        {
            int row = (y - 220) / 27;

            int processIndex = taskScrollOffset + row;

            if (processIndex >= 0 &&
                processIndex < static_cast<int>(taskProcesses.size()))
            {
                taskSelectedPid =
                    taskProcesses[processIndex].pid;

                statusText =
                    (currentLanguage == 0
                        ? L"Выбран процесс: "
                        : L"Selected process: ")
                    + taskProcesses[processIndex].name;

                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
        }

        // Клик по пустому месту снимает выбор.

        taskSelectedPid = 0;

        statusText = currentLanguage == 0
            ? L"Выбор процесса сброшен"
            : L"Process selection cleared";

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    // Страница пользователей.

    if (currentPage == 2)
    {
        if (UsersPage::HandleClick(
            hwnd,
            x,
            y,
            currentLanguage,
            statusText))
        {
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        return;
    }
    // BSOD Monitor.

    if (currentPage == 6)
    {
        if (BSODMonitorPage::HandleClick(
            hwnd, x, y, currentLanguage, statusText))
        {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }

    // Страница восстановления реестра.

    if (currentPage == 8)
    {
        if (RegistryPage::HandleClick(
            hwnd,
            x,
            y,
            currentLanguage,
            statusText))
        {
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        return;
    }

    // Диспетчер дисков.

    if (currentPage == 9)
    {
        if (DiskManagerPage::HandleClick(
            hwnd, x, y, currentLanguage, statusText))
        {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }

    // Настройки языка.

    if (currentPage == 7)
    {
        for (int i = 0; i < 2; ++i)
        {
            RECT r = languageRects[i];

            if (x >= r.left && x <= r.right &&
                y >= r.top && y <= r.bottom)
            {
                currentLanguage = i;

                SaveLanguage();

                statusText = currentLanguage == 0
                    ? L"Язык интерфейса изменён"
                    : L"Interface language changed";

                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
        }
    }

    // Очистка системы.

    if (currentPage == 5)
    {
        for (int i = 0; i < 4; ++i)
        {
            RECT r = cleanupItemRects[i];

            if (x >= r.left && x <= r.right &&
                y >= r.top && y <= r.bottom)
            {
                if (!IsCleanupCategorySupported(i))
                {
                    statusText = currentLanguage == 0
                        ? L"Эта категория пока не реализована"
                        : L"This category is not implemented yet";

                    InvalidateRect(hwnd, nullptr, FALSE);
                    return;
                }

                cleanupSelected[i] = !cleanupSelected[i];

                ResetCleanupResults();

                statusText = currentLanguage == 0
                    ? L"Выбор категорий изменён"
                    : L"Category selection changed";

                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
        }

        RECT scan = cleanupScanRect;

        if (x >= scan.left && x <= scan.right &&
            y >= scan.top && y <= scan.bottom)
        {
            ScanTemporaryFiles(hwnd);
            return;
        }

        RECT clean = cleanupCleanRect;

        if (cleanupScanFinished &&
            x >= clean.left && x <= clean.right &&
            y >= clean.top && y <= clean.bottom)
        {
            CleanTemporaryFiles(hwnd);
            return;
        }

        return;
    }

    // Кнопка «Начать».

    if (currentPage == 0 &&
        x >= 300 && x <= 412 &&
        y >= 210 && y <= 240)
    {
        if (currentPage != 1)
        {
            BeginPageTransition(hwnd);
        }

        currentPage = 1;

        SetTimer(
            hwnd,
            TASK_REFRESH_TIMER_ID,
            TASK_REFRESH_INTERVAL,
            nullptr
        );

        statusText = currentLanguage == 0
            ? L"Открыт диспетчер задач"
            : L"Task Manager opened";

        if (!taskProcessesLoaded)
        {
            RefreshTaskProcesses(hwnd);
        }

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    // Карточки главного экрана.

    for (int i = 0; i < 4; ++i)
    {
        RECT r = cardRects[i];

        if (x >= r.left && x <= r.right &&
            y >= r.top && y <= r.bottom)
        {
            // На главной странице карточки открывают соответствующие разделы.
            if (currentPage == 0)
            {
                int targetPage = -1;

                switch (i)
                {
                case 0:
                    targetPage = 3; // Диагностика
                    break;

                case 1:
                    targetPage = 1; // Диспетчер задач
                    break;

                case 2:
                    targetPage = 6; // BSOD Monitor
                    break;

                case 3:
                    targetPage = 5; // Очистка системы
                    break;
                }

                if (targetPage >= 0)
                {
                    if (currentPage != targetPage)
                    {
                        BeginPageTransition(hwnd);
                    }

                    currentPage = targetPage;

                    hoveredCard = -1;
                    hoveredCleanupItem = -1;

                    if (currentPage == 6)
                    {
                        BSODMonitorPage::Open(
                            hwnd,
                            currentLanguage,
                            statusText
                        );
                        MarkBsodIncidentsViewed();
                    }

                    if (currentPage == 1)
                    {
                        SetTimer(
                            hwnd,
                            TASK_REFRESH_TIMER_ID,
                            TASK_REFRESH_INTERVAL,
                            nullptr
                        );

                        if (!taskProcessesLoaded)
                            RefreshTaskProcesses(hwnd);
                    }
                    else
                    {
                        KillTimer(
                            hwnd,
                            TASK_REFRESH_TIMER_ID
                        );
                    }

                    statusText =
                        std::wstring(
                            currentLanguage == 0
                            ? L"Открыт раздел: "
                            : L"Opened section: "
                        ) + menuNames[currentLanguage][currentPage];

                    InvalidateRect(hwnd, nullptr, FALSE);
                    return;
                }
            }

            if (HandleUtilityCardClick(hwnd, currentPage, i))
            {
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }

            statusText =
                std::wstring(
                    currentLanguage == 0
                    ? L"Функция пока недоступна: "
                    : L"Feature is not available yet: "
                ) + cardTitles[currentLanguage][currentPage][i];

            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
    }
}

// ============================================================
// WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case UPDATE_CHECK_COMPLETED_MESSAGE:
        HandleUpdateCheckCompleted(hwnd, wParam, lParam);
        return 0;

    case WM_PAINT:
        PaintInterface(hwnd);
        return 0;

    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if (taskScrollbarDragging &&
            currentPage == 1)
        {
            int thumbHeight = 0;
            int thumbY = 0;
            int maxOffset = 0;

            if (GetTaskScrollbarGeometry(
                thumbHeight,
                thumbY,
                maxOffset))
            {
                int newThumbY =
                    y - taskScrollbarDragGrabOffset;

                int minThumbY = TASK_SCROLLBAR_Y;

                int maxThumbY =
                    TASK_SCROLLBAR_Y +
                    TASK_SCROLLBAR_HEIGHT -
                    thumbHeight;

                newThumbY = (std::max)(
                    minThumbY,
                    (std::min)(newThumbY, maxThumbY)
                    );

                int availableTrack =
                    TASK_SCROLLBAR_HEIGHT - thumbHeight;

                if (availableTrack > 0)
                {
                    double position =
                        static_cast<double>(
                            newThumbY - TASK_SCROLLBAR_Y
                            ) / availableTrack;

                    taskScrollOffset =
                        static_cast<int>(
                            position * maxOffset + 0.5
                            );

                    taskScrollOffset = (std::max)(
                        0,
                        (std::min)(
                            taskScrollOffset,
                            maxOffset
                            )
                        );

                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
        }
        else
        {
            UpdateHover(hwnd, x, y);
        }

        return 0;
    }

    case WM_MOUSELEAVE:
        hoveredMenu = -1;
        hoveredCard = -1;
        hoveredCleanupItem = -1;

        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if (currentPage == 1 &&
            taskProcessesLoaded)
        {
            int thumbHeight = 0;
            int thumbY = 0;
            int maxOffset = 0;

            if (GetTaskScrollbarGeometry(
                thumbHeight,
                thumbY,
                maxOffset))
            {
                bool insideThumb =
                    x >= TASK_SCROLLBAR_X - 8 &&
                    x <= TASK_SCROLLBAR_X +
                    TASK_SCROLLBAR_WIDTH + 8 &&
                    y >= thumbY &&
                    y <= thumbY + thumbHeight;

                if (insideThumb)
                {
                    taskScrollbarDragging = true;

                    taskScrollbarDragGrabOffset =
                        y - thumbY;

                    SetCapture(hwnd);

                    InvalidateRect(hwnd, nullptr, FALSE);

                    return 0;
                }
            }
        }

        HandleClick(hwnd, x, y);
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (taskScrollbarDragging)
        {
            taskScrollbarDragging = false;

            if (GetCapture() == hwnd)
                ReleaseCapture();

            InvalidateRect(hwnd, nullptr, FALSE);
        }

        return 0;
    }

    case WM_CAPTURECHANGED:
    {
        taskScrollbarDragging = false;

        InvalidateRect(hwnd, nullptr, FALSE);

        return 0;
    }

    case WM_RBUTTONUP:
    {
        if (currentPage != 1 || !taskProcessesLoaded)
            return 0;

        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);

        if (x < 282 || x > 1022 || y < 220 ||
            y >= 220 + TASK_MAX_VISIBLE_PROCESSES * 27)
            return 0;

        const int row = (y - 220) / 27;
        const int index = taskScrollOffset + row;
        if (index < 0 || index >= static_cast<int>(taskProcesses.size()))
            return 0;

        taskSelectedPid = taskProcesses[index].pid;
        const TaskManager::ProcessInfo* process = GetSelectedTaskProcess();
        if (!process)
            return 0;

        HMENU menu = CreatePopupMenu();
        if (!menu)
            return 0;

        const bool critical = process->isCritical;
        AppendMenuW(menu,
            process->exePath.empty() ? MF_STRING | MF_GRAYED : MF_STRING,
            1,
            currentLanguage == 0 ? L"Открыть расположение файла" : L"Open file location");

        AppendMenuW(menu,
            critical ? MF_STRING | MF_GRAYED : MF_STRING,
            2,
            process->isSuspended
                ? (currentLanguage == 0 ? L"Возобновить процесс" : L"Resume process")
                : (currentLanguage == 0 ? L"Приостановить процесс" : L"Suspend process"));

        AppendMenuW(menu,
            critical ? MF_STRING | MF_GRAYED : MF_STRING,
            3,
            currentLanguage == 0 ? L"Завершить процесс" : L"End process");

        POINT point{ x, y };
        ClientToScreen(hwnd, &point);
        const UINT command = TrackPopupMenu(
            menu,
            TPM_RIGHTBUTTON | TPM_RETURNCMD,
            point.x,
            point.y,
            0,
            hwnd,
            nullptr);
        DestroyMenu(menu);

        if (command == 1 && !process->exePath.empty())
        {
            const std::wstring arguments = L"/select,\"" + process->exePath + L"\"";
            ShellExecuteW(hwnd, L"open", L"explorer.exe",
                arguments.c_str(), nullptr, SW_SHOWNORMAL);
            statusText = currentLanguage == 0
                ? L"Открыто расположение файла процесса"
                : L"Process file location opened";
        }
        else if (command == 2)
        {
            if (process->isSuspended)
                ResumeSelectedTaskProcess(hwnd);
            else
                SuspendSelectedTaskProcess(hwnd);
        }
        else if (command == 3)
        {
            EndSelectedTaskProcess(hwnd);
        }

        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        if (currentPage == 1 &&
            taskProcessesLoaded &&
            taskProcesses.size() > TASK_MAX_VISIBLE_PROCESSES)
        {
            int maxOffset = (std::max)(
                0,
                static_cast<int>(taskProcesses.size())
                - TASK_MAX_VISIBLE_PROCESSES
                );

            int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);

            if (wheelDelta < 0)
            {
                taskScrollOffset += 3;
            }
            else if (wheelDelta > 0)
            {
                taskScrollOffset -= 3;
            }

            taskScrollOffset = (std::max)(
                0,
                (std::min)(taskScrollOffset, maxOffset)
                );

            InvalidateRect(hwnd, nullptr, FALSE);
        }

        return 0;
    }

    case WM_TIMER:
    {
        if (wParam == PAGE_TRANSITION_TIMER_ID)
        {
            ULONGLONG elapsed =
                GetTickCount64() - pageTransitionStart;

            if (elapsed >= PAGE_TRANSITION_DURATION)
            {
                KillTimer(
                    hwnd,
                    PAGE_TRANSITION_TIMER_ID
                );

                ClearPageTransition();
            }

            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (wParam == TASK_REFRESH_TIMER_ID &&
            currentPage == 1)
        {
            RefreshTaskProcesses(hwnd, false);
        }

        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        RemoveBsodTrayIcon();
        taskScrollbarDragging = false;

        KillTimer(
            hwnd,
            TASK_REFRESH_TIMER_ID
        );
        KillTimer(
            hwnd,
            PAGE_TRANSITION_TIMER_ID
        );

        ClearPageTransition();

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}

// ============================================================
// WINMAIN
// ============================================================

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    LPSTR,
    int nCmdShow)
{
    InitializeSettings();

    statusText = readyText[currentLanguage];

    GdiplusStartupInput gdiplusInput{};

    if (GdiplusStartup(
        &g_gdiplusToken,
        &gdiplusInput,
        nullptr) != Ok)
    {
        MessageBoxW(
            nullptr,
            L"Не удалось запустить GDI+.",
            L"UnlockTool",
            MB_ICONERROR
        );

        return 1;
    }

    WNDCLASSEXW wc{};

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;

    wc.hIcon = LoadIconW(
        hInstance,
        MAKEINTRESOURCEW(IDI_APP_ICON)
    );

    wc.hIconSm = LoadIconW(
        hInstance,
        MAKEINTRESOURCEW(IDI_APP_ICON)
    );

    if (!RegisterClassExW(&wc))
    {
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    RECT windowRect{
        0, 0,
        WINDOW_WIDTH,
        WINDOW_HEIGHT
    };

    AdjustWindowRect(
        &windowRect,
        WS_OVERLAPPEDWINDOW,
        FALSE
    );

    g_hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"UnlockTool",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!g_hwnd)
    {
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    CheckBsodStartupNotification(g_hwnd, hInstance);

    if (autoCheckUpdates)
        StartUpdateCheck(g_hwnd, false);

    MSG msg{};

    int result;

    while ((result = GetMessageW(
        &msg,
        nullptr,
        0,
        0)) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(g_gdiplusToken);

    return result == -1
        ? 1
        : static_cast<int>(msg.wParam);
}