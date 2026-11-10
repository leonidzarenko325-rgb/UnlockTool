#include "RegistryPage.h"
#include "RegistryRepair.h"

#include <windows.h>
#include <gdiplus.h>

#include <string>
#include <array>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

namespace RegistryPage
{
    namespace
    {
        constexpr size_t PARAMETER_COUNT = 6;

        std::array<RegistryRepair::ParameterInfo, PARAMETER_COUNT>
            parameters{};

        bool parametersLoaded = false;

        RECT refreshRect{};

        RECT parameterRows[PARAMETER_COUNT]{};
        int selectedParameter = -1;
        // Резервные копии параметров реестра.
        std::array<RegistryRepair::BackupInfo, PARAMETER_COUNT>
            parameterBackups{};

        std::array<bool, PARAMETER_COUNT>
            backupAvailable{};

        RECT restoreRect{};
        RECT rollbackRect{};

        // RestoreDefault performs a typed backup and verifies the write.
        // SetupType/CmdLine are additionally blocked while Windows Setup/OOBE
        // is active by the RegistryRepair backend.
        bool IsRestorableParameter(
            RegistryRepair::Parameter parameter)
        {
            switch (parameter)
            {
            case RegistryRepair::Parameter::Shell:
            case RegistryRepair::Parameter::Userinit:
            case RegistryRepair::Parameter::CmdLine:
            case RegistryRepair::Parameter::AppInitDlls:
            case RegistryRepair::Parameter::SetupType:
            case RegistryRepair::Parameter::CursorSuppression:
                return true;
            default:
                return false;
            }
        }

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
        const Color DANGER(255, 225, 95, 100);

        bool IsRussian(int language)
        {
            return language == 0;
        }

        bool Contains(
            const RECT& rect,
            int x,
            int y)
        {
            return
                x >= rect.left &&
                x <= rect.right &&
                y >= rect.top &&
                y <= rect.bottom;
        }

        void MakePath(
            GraphicsPath& path,
            float x,
            float y,
            float width,
            float height,
            float radius)
        {
            path.Reset();

            float diameter = radius * 2.0f;

            path.AddArc(
                x, y,
                diameter, diameter,
                180, 90
            );

            path.AddArc(
                x + width - diameter,
                y,
                diameter, diameter,
                270, 90
            );

            path.AddArc(
                x + width - diameter,
                y + height - diameter,
                diameter, diameter,
                0, 90
            );

            path.AddArc(
                x,
                y + height - diameter,
                diameter, diameter,
                90, 90
            );

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

            MakePath(
                path,
                x, y,
                width, height,
                radius
            );

            SolidBrush brush(color);

            graphics.FillPath(
                &brush,
                &path
            );
        }

        void DrawRounded(
            Graphics& graphics,
            float x,
            float y,
            float width,
            float height,
            float radius,
            Color color)
        {
            GraphicsPath path;

            MakePath(
                path,
                x, y,
                width, height,
                radius
            );

            Pen pen(color, 1.0f);

            graphics.DrawPath(
                &pen,
                &path
            );
        }

        void DrawText(
            Graphics& graphics,
            const std::wstring& text,
            float x,
            float y,
            float width,
            float height,
            float size,
            Color color,
            bool bold = false)
        {
            Font font(
                L"Segoe UI",
                size,
                bold ? FontStyleBold : FontStyleRegular,
                UnitPixel
            );

            SolidBrush brush(color);

            RectF rect(
                x, y,
                width, height
            );

            StringFormat format;

            format.SetAlignment(
                StringAlignmentNear
            );

            format.SetLineAlignment(
                StringAlignmentCenter
            );

            graphics.DrawString(
                text.c_str(),
                -1,
                &font,
                rect,
                &format,
                &brush
            );
        }

        void DrawButton(
            Graphics& graphics,
            const RECT& rect,
            const std::wstring& text)
        {
            float width = static_cast<float>(
                rect.right - rect.left
                );

            float height = static_cast<float>(
                rect.bottom - rect.top
                );

            GraphicsPath path;

            MakePath(
                path,
                static_cast<float>(rect.left),
                static_cast<float>(rect.top),
                width,
                height,
                9.0f
            );

            LinearGradientBrush brush(
                Point(rect.left, rect.top),
                Point(rect.right, rect.bottom),
                ACCENT,
                ACCENT_LIGHT
            );

            graphics.FillPath(
                &brush,
                &path
            );

            Font font(
                L"Segoe UI",
                10.5f,
                FontStyleBold,
                UnitPixel
            );

            SolidBrush textBrush(TEXT_MAIN);

            RectF textRect(
                static_cast<float>(rect.left),
                static_cast<float>(rect.top),
                width,
                height
            );

            StringFormat format;
            format.SetAlignment(StringAlignmentCenter);
            format.SetLineAlignment(StringAlignmentCenter);

            graphics.DrawString(
                text.c_str(),
                -1,
                &font,
                textRect,
                &format,
                &textBrush
            );
        }

        // ====================================================
        // ОБНОВЛЕНИЕ ДАННЫХ
        // ====================================================

        bool RefreshParameters(
            std::wstring& statusText,
            int language)
        {
            parametersLoaded = false;

            bool success =
                RegistryRepair::ReadAllParameters(
                    parameters.data(),
                    parameters.size()
                );

            parametersLoaded = true;

            if (success)
            {
                statusText = IsRussian(language)
                    ? L"Параметры реестра проверены"
                    : L"Registry parameters checked";
            }
            else
            {
                statusText = IsRussian(language)
                    ? L"Некоторые параметры не удалось прочитать"
                    : L"Some parameters could not be read";
            }

            return success;
        }

        // ====================================================
        // НАЗВАНИЯ ПАРАМЕТРОВ
        // ====================================================

        std::wstring GetParameterDescription(
            RegistryRepair::Parameter parameter,
            int language)
        {
            bool russian = IsRussian(language);

            switch (parameter)
            {
            case RegistryRepair::Parameter::Shell:
                return russian
                    ? L"Оболочка Windows"
                    : L"Windows shell";

            case RegistryRepair::Parameter::Userinit:
                return russian
                    ? L"Инициализация пользователя"
                    : L"User initialization";

            case RegistryRepair::Parameter::CmdLine:
                return russian
                    ? L"Командная строка установки Windows"
                    : L"Windows Setup command line";

            case RegistryRepair::Parameter::AppInitDlls:
                return russian
                    ? L"Библиотеки AppInit"
                    : L"AppInit libraries";

            case RegistryRepair::Parameter::SetupType:
                return russian
                    ? L"Тип установки Windows"
                    : L"Windows setup type";

            case RegistryRepair::Parameter::CursorSuppression:
                return russian
                    ? L"Подавление курсора (рекомендуется отключить)"
                    : L"Cursor suppression (recommended off)";

            default:
                return L"Unknown";
            }
        }


        std::wstring GetParameterStatus(
            const RegistryRepair::ParameterInfo& info,
            int language)
        {
            if (!info.readable)
            {
                if (info.errorCode == ERROR_FILE_NOT_FOUND)
                {
                    return IsRussian(language)
                        ? L"Не найдено"
                        : L"Not found";
                }

                return IsRussian(language)
                    ? L"Ошибка чтения"
                    : L"Read error";
            }

            if (info.matchesExpected)
            {
                return IsRussian(language)
                    ? L"Норма"
                    : L"Normal";
            }

            if (!info.exists)
            {
                return IsRussian(language)
                    ? L"Не найдено"
                    : L"Not found";
            }

            return IsRussian(language)
                ? L"Отличается"
                : L"Different";
        }


        Color GetParameterStatusColor(
            const RegistryRepair::ParameterInfo& info)
        {
            if (!info.readable)
                return DANGER;

            if (info.matchesExpected)
                return SUCCESS;

            return !info.exists ? DANGER : WARNING;
        }


        std::wstring GetDisplayValue(
            const RegistryRepair::ParameterInfo& info,
            int language)
        {
            if (!info.readable)
            {
                if (info.errorCode == ERROR_FILE_NOT_FOUND)
                {
                    return IsRussian(language)
                        ? L"Значение отсутствует"
                        : L"Value does not exist";
                }

                return RegistryRepair::GetErrorMessage(
                    info.errorCode
                );
            }

            if (!info.exists)
            {
                if (info.matchesExpected)
                    return IsRussian(language)
                        ? L"Пустое значение по умолчанию"
                        : L"Default empty value";

                return IsRussian(language)
                    ? L"Значение отсутствует"
                    : L"Value does not exist";
            }

            if (info.currentValue.empty())
                return L"(empty)";

            return info.currentValue;
        }
    }
    

    // ========================================================
    // ОТКРЫТИЕ СТРАНИЦЫ
    // ========================================================

    void Open(
        HWND hwnd,
        int language,
        std::wstring& statusText)
    {
        RefreshParameters(
            statusText,
            language
        );

        InvalidateRect(
            hwnd,
            nullptr,
            FALSE
        );
    }

    // ========================================================
    // ОТРИСОВКА СТРАНИЦЫ
    // ========================================================

    void Draw(
        Graphics& graphics,
        int language)
    {
        bool russian = IsRussian(language);

        DrawText(
            graphics,
            russian
            ? L"Проверка реестра"
            : L"Registry repair",
            270, 100,
            500, 30,
            17,
            TEXT_MAIN,
            true
        );

        DrawText(
            graphics,
            russian
            ? L"Проверка важных системных параметров Windows"
            : L"Checking important Windows system parameters",
            270, 130,
            600, 20,
            10,
            TEXT_SECONDARY
        );

        refreshRect = {
            900, 105, 1060, 145
        };

        DrawButton(
            graphics,
            refreshRect,
            russian ? L"Обновить" : L"Refresh"
        );
        restoreRect = {
    680, 105, 810, 145
        };

        rollbackRect = {
            820, 105, 890, 145
        };

        DrawButton(
            graphics,
            restoreRect,
            russian ? L"Восстановить" : L"Restore"
        );

        DrawButton(
            graphics,
            rollbackRect,
            russian ? L"Откат" : L"Undo"
        );

        // ====================================================
        // ОСНОВНАЯ КАРТОЧКА
        // ====================================================

        FillRounded(
            graphics,
            270, 165,
            790, 455,
            13,
            PANEL
        );

        DrawRounded(
            graphics,
            270, 165,
            790, 455,
            13,
            BORDER
        );

        DrawText(
            graphics,
            russian
            ? L"Системные параметры"
            : L"System parameters",
            290, 180,
            450, 28,
            14,
            TEXT_MAIN,
            true
        );

        DrawText(
            graphics,
            russian ? L"ПАРАМЕТР" : L"PARAMETER",
            290, 220,
            180, 25,
            9,
            TEXT_MUTED,
            true
        );

        DrawText(
            graphics,
            russian ? L"ТЕКУЩЕЕ ЗНАЧЕНИЕ" : L"CURRENT VALUE",
            480, 220,
            330, 25,
            9,
            TEXT_MUTED,
            true
        );

        DrawText(
            graphics,
            russian ? L"СТАТУС" : L"STATUS",
            850, 220,
            180, 25,
            9,
            TEXT_MUTED,
            true
        );

        for (size_t i = 0; i < PARAMETER_COUNT; ++i)
        {
            int rowY = 250 + static_cast<int>(i) * 55;

            parameterRows[i] = {
                285,
                rowY,
                1045,
                rowY + 48
            };

            FillRounded(
                graphics,
                285,
                static_cast<float>(rowY),
                760,
                48,
                8,
                selectedParameter == static_cast<int>(i)
                ? Color(255, 38, 58, 88)
                : CARD
            );

            const auto& info = parameters[i];

            DrawText(
                graphics,
                info.name,
                300,
                static_cast<float>(rowY),
                170,
                22,
                11,
                TEXT_MAIN,
                true
            );

            DrawText(
                graphics,
                GetParameterDescription(
                    info.parameter,
                    language
                ),
                300,
                static_cast<float>(rowY + 22),
                170,
                18,
                9,
                TEXT_MUTED
            );

            DrawText(
                graphics,
                GetDisplayValue(info, language),
                480,
                static_cast<float>(rowY),
                350,
                48,
                10,
                TEXT_SECONDARY
            );

            DrawText(
                graphics,
                GetParameterStatus(info, language),
                850,
                static_cast<float>(rowY),
                180,
                48,
                10,
                GetParameterStatusColor(info),
                true
            );
        }

        // ====================================================
        // ИНФОРМАЦИЯ
        // ====================================================

        FillRounded(
            graphics,
            285, 595,
            760, 15,
            5,
            CARD
        );

        DrawText(
            graphics,
            russian
            ? L"Проверка не изменяет значения реестра."
            : L"Checking does not modify registry values.",
            295, 592,
            740, 20,
            9,
            TEXT_MUTED
        );

        if (!parametersLoaded)
        {
            DrawText(
                graphics,
                russian
                ? L"Данные ещё не загружены"
                : L"Data has not been loaded",
                290, 300,
                500, 30,
                11,
                WARNING
            );
        }
    }

    // ========================================================
    // ОБРАБОТКА КЛИКОВ
    // ========================================================

    bool HandleClick(
        HWND hwnd,
        int x,
        int y,
        int language,
        std::wstring& statusText)
    {
        // ====================================================
// ВОССТАНОВЛЕНИЕ СТАНДАРТНОГО ЗНАЧЕНИЯ
// ====================================================

        if (Contains(restoreRect, x, y))
        {
            if (selectedParameter < 0 ||
                selectedParameter >= static_cast<int>(PARAMETER_COUNT))
            {
                statusText = language == 0
                    ? L"Сначала выберите параметр"
                    : L"Select a parameter first";

                return true;
            }

            const size_t index =
                static_cast<size_t>(selectedParameter);

            const auto& info = parameters[index];

            if (!IsRestorableParameter(info.parameter))
            {
                statusText = language == 0
                    ? L"Восстановление этого параметра пока недоступно"
                    : L"Restoring this parameter is not available yet";

                return true;
            }

            if (!info.readable &&
                !(!info.exists &&
                    info.errorCode == ERROR_FILE_NOT_FOUND))
            {
                statusText = language == 0
                    ? L"Нельзя изменить параметр: ошибка чтения"
                    : L"Cannot modify parameter: read error";

                return true;
            }

            if (backupAvailable[index])
            {
                statusText = language == 0
                    ? L"Сначала отмените предыдущее изменение"
                    : L"Undo the previous change first";

                return true;
            }

            if (info.matchesExpected)
            {
                statusText = language == 0
                    ? L"Параметр уже соответствует норме"
                    : L"Parameter is already normal";

                return true;
            }

            int result = MessageBoxW(
                hwnd,
                language == 0
                ? L"Перед изменением будет создана резервная копия. Значения AppInit_DLLs, CmdLine и SetupType могут влиять на отдельные программы или незавершённую настройку Windows. EnableCursorSuppression будет установлен в 0. Продолжить?"
                : L"A backup will be created first. AppInit_DLLs, CmdLine and SetupType may affect specific software or an ongoing Windows setup. EnableCursorSuppression will be set to 0. Continue?",
                L"UnlockTool",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
            );

            if (result != IDYES)
                return true;

            LONG errorCode = ERROR_SUCCESS;

            bool success =
                RegistryRepair::RestoreDefault(
                    info.parameter,
                    parameterBackups[index],
                    errorCode
                );

            if (success)
            {
                backupAvailable[index] = true;

                statusText = language == 0
                    ? L"Параметр восстановлен. Резервная копия сохранена."
                    : L"Parameter restored. Backup saved.";
            }
            else
            {
                if (parameterBackups[index].valid)
                    backupAvailable[index] = true;

                statusText =
                    (language == 0
                        ? L"Ошибка восстановления: "
                        : L"Restore failed: ")
                    + RegistryRepair::GetErrorMessage(errorCode);
            }

            std::wstring operationStatus = statusText;

            RefreshParameters(statusText, language);

            statusText = operationStatus;

            InvalidateRect(hwnd, nullptr, FALSE);

            return true;
        }

        // ====================================================
        // ОТКАТ ИЗМЕНЕНИЙ
        // ====================================================

        if (Contains(rollbackRect, x, y))
        {
            if (selectedParameter < 0 ||
                selectedParameter >= static_cast<int>(PARAMETER_COUNT))
            {
                statusText = language == 0
                    ? L"Сначала выберите параметр"
                    : L"Select a parameter first";

                return true;
            }

            const size_t index =
                static_cast<size_t>(selectedParameter);

            if (!backupAvailable[index])
            {
                statusText = language == 0
                    ? L"Резервная копия отсутствует"
                    : L"No backup available";

                return true;
            }

            int result = MessageBoxW(
                hwnd,
                language == 0
                ? L"Восстановить исходное значение реестра из резервной копии?"
                : L"Restore the original registry value from the backup?",
                L"UnlockTool",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
            );

            if (result != IDYES)
                return true;

            LONG errorCode = ERROR_SUCCESS;

            bool success =
                RegistryRepair::RestoreBackup(
                    parameterBackups[index],
                    errorCode
                );

            if (success)
            {
                backupAvailable[index] = false;

                parameterBackups[index] =
                    RegistryRepair::BackupInfo{};

                statusText = language == 0
                    ? L"Исходное значение восстановлено"
                    : L"Original value restored";
            }
            else
            {
                statusText =
                    (language == 0
                        ? L"Ошибка отката: "
                        : L"Rollback failed: ")
                    + RegistryRepair::GetErrorMessage(errorCode);
            }

            std::wstring operationStatus = statusText;

            RefreshParameters(statusText, language);

            statusText = operationStatus;

            InvalidateRect(hwnd, nullptr, FALSE);

            return true;
        }
        for (size_t i = 0; i < PARAMETER_COUNT; ++i)
        {
            if (Contains(parameterRows[i], x, y))
            {
                selectedParameter = static_cast<int>(i);

                const auto& info = parameters[i];

                statusText =
                    GetParameterDescription(
                        info.parameter,
                        language
                    )
                    + L": "
                    + GetDisplayValue(info, language);

                InvalidateRect(
                    hwnd,
                    nullptr,
                    FALSE
                );

                return true;
            }
        }

        if (Contains(refreshRect, x, y))
        {
            RefreshParameters(
                statusText,
                language
            );

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE
            );

            return true;
        }

        return false;
    }
}