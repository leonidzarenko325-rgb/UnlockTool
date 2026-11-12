
#include "RegistryRepair.h"

#include <windows.h>
#include <vector>
#include <string>
#include <cwchar>
#include <utility>

namespace RegistryRepair
{
    namespace
    {
        constexpr size_t PARAMETER_COUNT = 6;
        constexpr DWORD MAX_VALUE_SIZE = 1024 * 1024;

        struct ParameterDefinition
        {
            Parameter parameter;
            const wchar_t* name;
            const wchar_t* keyPath;
            const wchar_t* valueName;
            const wchar_t* expectedValue;
            DWORD expectedType;
        };

        const ParameterDefinition definitions[] =
        {
            {
                Parameter::Shell,
                L"Shell",
                L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                L"Shell",
                L"explorer.exe",
                REG_SZ
            },
            {
                Parameter::Userinit,
                L"Userinit",
                L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                L"Userinit",
                L"%SystemRoot%\\system32\\userinit.exe,",
                REG_SZ
            },
            {
                Parameter::CmdLine,
                L"CmdLine",
                L"SYSTEM\\Setup",
                L"CmdLine",
                L"",
                REG_SZ
            },
            {
                Parameter::AppInitDlls,
                L"AppInit_DLLs",
                L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
                L"AppInit_DLLs",
                L"",
                REG_SZ
            },
            {
                Parameter::SetupType,
                L"SetupType",
                L"SYSTEM\\Setup",
                L"SetupType",
                L"0",
                REG_DWORD
            },
            {
                Parameter::CursorSuppression,
                L"EnableCursorSuppression",
                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                L"EnableCursorSuppression",
                L"0",
                REG_DWORD
            }
        };

        REGSAM GetRegistryView()
        {
            BOOL isWow64 = FALSE;

            if (IsWow64Process(GetCurrentProcess(), &isWow64) &&
                isWow64)
            {
                return KEY_WOW64_64KEY;
            }

            return 0;
        }

        const ParameterDefinition* FindDefinition(
            Parameter parameter)
        {
            for (const auto& definition : definitions)
            {
                if (definition.parameter == parameter)
                    return &definition;
            }

            return nullptr;
        }

        bool IsRestorableParameter(Parameter parameter)
        {
            // All six parameters have an explicit, typed default and are
            // backed up before any write. Setup-related values are additionally
            // protected by IsWindowsSetupActive().
            return FindDefinition(parameter) != nullptr;
        }

        bool IsWindowsSetupActive()
        {
            HKEY key = nullptr;
            const LONG openError = RegOpenKeyExW(
                HKEY_LOCAL_MACHINE,
                L"SYSTEM\\Setup",
                0,
                KEY_QUERY_VALUE | GetRegistryView(),
                &key);

            if (openError != ERROR_SUCCESS)
                return false;

            const wchar_t* flags[] =
            {
                L"SystemSetupInProgress",
                L"OOBEInProgress",
                L"SetupType"
            };

            bool active = false;
            for (const wchar_t* flag : flags)
            {
                DWORD value = 0;
                DWORD type = 0;
                DWORD size = sizeof(value);
                const LONG error = RegQueryValueExW(
                    key, flag, nullptr, &type,
                    reinterpret_cast<LPBYTE>(&value), &size);

                if (error == ERROR_SUCCESS &&
                    type == REG_DWORD && size == sizeof(value) &&
                    value != 0)
                {
                    active = true;
                    break;
                }
            }

            RegCloseKey(key);
            return active;
        }

        LONG WriteExpectedValue(
            HKEY key,
            const ParameterDefinition& definition)
        {
            if (definition.expectedType == REG_DWORD)
            {
                wchar_t* end = nullptr;
                const unsigned long parsed =
                    std::wcstoul(definition.expectedValue, &end, 10);

                if (!end || end == definition.expectedValue || *end != L'\0' ||
                    parsed > MAXDWORD)
                    return ERROR_INVALID_DATA;

                const DWORD value = static_cast<DWORD>(parsed);
                return RegSetValueExW(
                    key, definition.valueName, 0, REG_DWORD,
                    reinterpret_cast<const BYTE*>(&value), sizeof(value));
            }

            if (definition.expectedType == REG_SZ ||
                definition.expectedType == REG_EXPAND_SZ)
            {
                const std::wstring value = definition.expectedValue;
                const size_t byteSize = (value.size() + 1) * sizeof(wchar_t);
                if (byteSize > MAXDWORD)
                    return ERROR_INVALID_DATA;

                return RegSetValueExW(
                    key, definition.valueName, 0, definition.expectedType,
                    reinterpret_cast<const BYTE*>(value.c_str()),
                    static_cast<DWORD>(byteSize));
            }

            return ERROR_UNSUPPORTED_TYPE;
        }

        std::wstring DwordToString(DWORD value)
        {
            return std::to_wstring(value);
        }

        bool ReadRegistryValue(
            const ParameterDefinition& definition,
            ParameterInfo& result)
        {
            HKEY key = nullptr;

            const REGSAM access =
                KEY_QUERY_VALUE | GetRegistryView();

            LONG error = RegOpenKeyExW(
                HKEY_LOCAL_MACHINE,
                definition.keyPath,
                0,
                access,
                &key
            );

            if (error != ERROR_SUCCESS)
            {
                result.errorCode = error;
                result.readable = false;
                result.exists = false;
                return false;
            }

            DWORD type = REG_NONE;
            DWORD size = 0;

            error = RegQueryValueExW(
                key,
                definition.valueName,
                nullptr,
                &type,
                nullptr,
                &size
            );

            if (error != ERROR_SUCCESS)
            {
                RegCloseKey(key);

                // CmdLine and AppInit_DLLs are commonly absent when their
                // effective default is an empty string. Treat that state as
                // healthy instead of reporting a false registry error.
                if (error == ERROR_FILE_NOT_FOUND &&
                    definition.expectedType == REG_SZ &&
                    definition.expectedValue[0] == L'\0')
                {
                    result.exists = false;
                    result.readable = true;
                    result.currentValue.clear();
                    result.registryType = definition.expectedType;
                    result.errorCode = ERROR_SUCCESS;
                    return true;
                }

                result.errorCode = error;
                result.readable = false;
                result.exists = false;
                return false;
            }

            result.exists = true;
            result.registryType = type;

            if (type == REG_DWORD)
            {
                if (size != sizeof(DWORD))
                {
                    RegCloseKey(key);

                    result.errorCode = ERROR_INVALID_DATA;
                    result.readable = false;
                    return false;
                }

                DWORD value = 0;

                error = RegQueryValueExW(
                    key,
                    definition.valueName,
                    nullptr,
                    nullptr,
                    reinterpret_cast<LPBYTE>(&value),
                    &size
                );

                RegCloseKey(key);

                if (error != ERROR_SUCCESS)
                {
                    result.errorCode = error;
                    result.readable = false;
                    return false;
                }

                result.currentValue = DwordToString(value);
            }
            else if (type == REG_SZ || type == REG_EXPAND_SZ)
            {
                if (size > MAX_VALUE_SIZE ||
                    size % sizeof(wchar_t) != 0)
                {
                    RegCloseKey(key);

                    result.errorCode = ERROR_INVALID_DATA;
                    result.readable = false;
                    return false;
                }

                const size_t characterCount =
                    size / sizeof(wchar_t);

                std::vector<wchar_t> buffer(
                    characterCount + 1,
                    L'\0'
                );

                error = RegQueryValueExW(
                    key,
                    definition.valueName,
                    nullptr,
                    nullptr,
                    reinterpret_cast<LPBYTE>(buffer.data()),
                    &size
                );

                RegCloseKey(key);

                if (error != ERROR_SUCCESS)
                {
                    result.errorCode = error;
                    result.readable = false;
                    return false;
                }

                result.currentValue.assign(
                    buffer.data(),
                    size / sizeof(wchar_t)
                );

                const size_t nullPosition =
                    result.currentValue.find(L'\0');

                if (nullPosition != std::wstring::npos)
                {
                    result.currentValue.resize(nullPosition);
                }
            }
            else
            {
                RegCloseKey(key);

                result.errorCode = ERROR_UNSUPPORTED_TYPE;
                result.readable = false;
                return false;
            }

            result.readable = true;
            result.errorCode = ERROR_SUCCESS;

            return true;
        }
    }

    ParameterInfo GetParameterInfo(Parameter parameter)
    {
        ParameterInfo result{};

        const auto* definition = FindDefinition(parameter);

        if (!definition)
        {
            result.errorCode = ERROR_INVALID_PARAMETER;
            return result;
        }

        result.parameter = definition->parameter;
        result.name = definition->name;
        result.keyPath = definition->keyPath;
        result.valueName = definition->valueName;
        result.expectedValue = definition->expectedValue;
        result.registryType = definition->expectedType;

        return result;
    }

    bool ReadParameter(
        Parameter parameter,
        ParameterInfo& result)
    {
        result = GetParameterInfo(parameter);

        const auto* definition = FindDefinition(parameter);

        if (!definition)
            return false;

        if (!ReadRegistryValue(*definition, result))
            return false;

        CheckParameter(result);

        return true;
    }

    bool ReadAllParameters(
        ParameterInfo results[],
        size_t count)
    {
        if (!results || count < PARAMETER_COUNT)
            return false;

        const Parameter parameters[PARAMETER_COUNT] =
        {
            Parameter::Shell,
            Parameter::Userinit,
            Parameter::CmdLine,
            Parameter::AppInitDlls,
            Parameter::SetupType,
            Parameter::CursorSuppression
        };

        bool allReadable = true;

        for (size_t i = 0; i < PARAMETER_COUNT; ++i)
        {
            if (!ReadParameter(parameters[i], results[i]))
                allReadable = false;
        }

        return allReadable;
    }

    bool CheckParameter(ParameterInfo& info)
    {
        info.matchesExpected = false;

        if (!info.readable)
            return false;

        const auto* definition = FindDefinition(info.parameter);

        if (!definition)
            return false;

        if (info.registryType != definition->expectedType)
            return false;

        info.matchesExpected =
            info.currentValue == info.expectedValue;

        return info.matchesExpected;
    }

    // ========================================================
    // СОЗДАНИЕ РЕЗЕРВНОЙ КОПИИ
    // ========================================================

    bool CreateBackup(
        Parameter parameter,
        BackupInfo& backup)
    {
        backup = BackupInfo{};
        backup.parameter = parameter;

        const auto* definition = FindDefinition(parameter);

        if (!definition)
            return false;

        HKEY key = nullptr;

        LONG error = RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            definition->keyPath,
            0,
            KEY_QUERY_VALUE | GetRegistryView(),
            &key
        );

        if (error != ERROR_SUCCESS)
            return false;

        DWORD type = REG_NONE;
        DWORD size = 0;

        error = RegQueryValueExW(
            key,
            definition->valueName,
            nullptr,
            &type,
            nullptr,
            &size
        );

        if (error == ERROR_FILE_NOT_FOUND)
        {
            RegCloseKey(key);

            backup.valueExisted = false;
            backup.valid = true;

            return true;
        }

        if (error != ERROR_SUCCESS || size > MAX_VALUE_SIZE)
        {
            RegCloseKey(key);
            return false;
        }

        std::vector<BYTE> data(size);

        error = RegQueryValueExW(
            key,
            definition->valueName,
            nullptr,
            &type,
            data.empty() ? nullptr : data.data(),
            &size
        );

        RegCloseKey(key);

        if (error != ERROR_SUCCESS)
            return false;

        data.resize(size);

        backup.registryType = type;
        backup.data = std::move(data);
        backup.valueExisted = true;
        backup.valid = true;

        return true;
    }

    // ========================================================
    // ВОССТАНОВЛЕНИЕ РЕЗЕРВНОЙ КОПИИ
    // ========================================================

    bool RestoreBackup(
        const BackupInfo& backup,
        LONG& errorCode)
    {
        errorCode = ERROR_SUCCESS;

        if (!backup.valid ||
            !IsRestorableParameter(backup.parameter))
        {
            errorCode = ERROR_INVALID_PARAMETER;
            return false;
        }

        const auto* definition = FindDefinition(backup.parameter);

        if (!definition)
        {
            errorCode = ERROR_INVALID_PARAMETER;
            return false;
        }

        HKEY key = nullptr;

        LONG error = RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            definition->keyPath,
            0,
            KEY_SET_VALUE | GetRegistryView(),
            &key
        );

        if (error != ERROR_SUCCESS)
        {
            errorCode = error;
            return false;
        }

        if (backup.valueExisted)
        {
            if (backup.data.size() > MAXDWORD)
            {
                RegCloseKey(key);
                errorCode = ERROR_INVALID_DATA;
                return false;
            }

            error = RegSetValueExW(
                key,
                definition->valueName,
                0,
                backup.registryType,
                backup.data.empty()
                ? nullptr
                : backup.data.data(),
                static_cast<DWORD>(backup.data.size())
            );
        }
        else
        {
            error = RegDeleteValueW(
                key,
                definition->valueName
            );

            if (error == ERROR_FILE_NOT_FOUND)
                error = ERROR_SUCCESS;
        }

        RegCloseKey(key);

        errorCode = error;

        return error == ERROR_SUCCESS;
    }

    // ========================================================
    // ВОССТАНОВЛЕНИЕ СТАНДАРТНОГО ЗНАЧЕНИЯ
    // ========================================================

    bool RestoreDefault(
        Parameter parameter,
        BackupInfo& backup,
        LONG& errorCode)
    {
        errorCode = ERROR_SUCCESS;
        backup = BackupInfo{};

        if (!IsRestorableParameter(parameter))
        {
            errorCode = ERROR_INVALID_PARAMETER;
            return false;
        }

        // CmdLine and SetupType belong to the Windows setup state. Do not
        // touch them while Setup/OOBE is active.
        if ((parameter == Parameter::CmdLine ||
             parameter == Parameter::SetupType) && IsWindowsSetupActive())
        {
            errorCode = ERROR_BUSY;
            return false;
        }

        const auto* definition = FindDefinition(parameter);

        if (!definition)
        {
            errorCode = ERROR_INVALID_PARAMETER;
            return false;
        }

        if (!CreateBackup(parameter, backup))
        {
            errorCode = ERROR_READ_FAULT;
            backup = BackupInfo{};
            return false;
        }

        HKEY key = nullptr;

        LONG error = RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            definition->keyPath,
            0,
            KEY_SET_VALUE | GetRegistryView(),
            &key
        );

        if (error != ERROR_SUCCESS)
        {
            errorCode = error;
            backup = BackupInfo{};
            return false;
        }

        // Preserve the registry type declared for the parameter. In
        // particular SetupType is REG_DWORD, not REG_SZ.
        error = WriteExpectedValue(key, *definition);

        RegCloseKey(key);

        if (error != ERROR_SUCCESS)
        {
            LONG originalError = error;
            LONG rollbackError = ERROR_SUCCESS;

            if (!RestoreBackup(backup, rollbackError))
            {
                errorCode = rollbackError;
                // Резервная копия сохраняется для ручного отката.
            }
            else
            {
                errorCode = originalError;
                backup = BackupInfo{};
            }

            return false;
        }

        ParameterInfo verification{};

        if (!ReadParameter(parameter, verification) ||
            !verification.matchesExpected)
        {
            LONG verificationError =
                verification.errorCode != ERROR_SUCCESS
                ? verification.errorCode
                : ERROR_INVALID_DATA;

            LONG rollbackError = ERROR_SUCCESS;

            if (!RestoreBackup(backup, rollbackError))
            {
                errorCode = rollbackError;
                // Не удаляем копию, если откат не удался.
            }
            else
            {
                errorCode = verificationError;
                backup = BackupInfo{};
            }

            return false;
        }

        return true;
    }

    // ========================================================
    // ТЕКСТОВОЕ ОПИСАНИЕ ОШИБОК
    // ========================================================

    std::wstring GetErrorMessage(LONG errorCode)
    {
        switch (errorCode)
        {
        case ERROR_SUCCESS:
            return L"OK";

        case ERROR_FILE_NOT_FOUND:
            return L"Значение не найдено";

        case ERROR_PATH_NOT_FOUND:
            return L"Раздел реестра не найден";

        case ERROR_ACCESS_DENIED:
            return L"Отказано в доступе";

        case ERROR_INVALID_DATA:
            return L"Некорректные данные";

        case ERROR_UNSUPPORTED_TYPE:
            return L"Неподдерживаемый тип данных";

        case ERROR_INVALID_PARAMETER:
            return L"Некорректный параметр";

        case ERROR_MORE_DATA:
            return L"Размер данных изменился во время чтения";

        case ERROR_READ_FAULT:
            return L"Не удалось прочитать резервную копию";

        case ERROR_BUSY:
            return L"Выполняется настройка Windows; изменение отложено";

        default:
            return L"Ошибка Windows: " +
                std::to_wstring(errorCode);
        }
    }
}
