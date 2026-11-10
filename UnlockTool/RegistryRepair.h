
#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace RegistryRepair
{
    enum class Parameter
    {
        Shell,
        Userinit,
        CmdLine,
        AppInitDlls,
        SetupType,
        CursorSuppression
    };

    struct ParameterInfo
    {
        Parameter parameter{};

        std::wstring name;
        std::wstring keyPath;
        std::wstring valueName;

        std::wstring currentValue;
        std::wstring expectedValue;

        DWORD registryType = REG_NONE;
        LONG errorCode = ERROR_SUCCESS;

        bool exists = false;
        bool readable = false;
        bool matchesExpected = false;
    };

    // Получение описания параметра.
    ParameterInfo GetParameterInfo(Parameter parameter);

    // Резервная копия значения реестра.
    struct BackupInfo
    {
        Parameter parameter{};

        DWORD registryType = REG_NONE;

        std::vector<BYTE> data;

        bool valueExisted = false;
        bool valid = false;
    };

    // Чтение текущего значения.
    bool ReadParameter(
        Parameter parameter,
        ParameterInfo& result
    );

    // Чтение всех шести параметров.
    bool ReadAllParameters(
        ParameterInfo results[],
        size_t count
    );

    // Проверка значения относительно ожидаемого.
    bool CheckParameter(ParameterInfo& info);

    // Создание резервной копии.
    bool CreateBackup(
        Parameter parameter,
        BackupInfo& backup
    );

    // Восстановление исходного значения.
    bool RestoreBackup(
        const BackupInfo& backup,
        LONG& errorCode
    );

    // Восстановление проверенного стандартного значения.
    bool RestoreDefault(
        Parameter parameter,
        BackupInfo& backup,
        LONG& errorCode
    );

    // Получение текста ошибки Windows.
    std::wstring GetErrorMessage(LONG errorCode);
}

