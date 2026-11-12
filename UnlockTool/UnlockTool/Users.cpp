
#include "Users.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>

namespace Users
{
    namespace
    {
        bool IsValidUsername(const std::wstring& username)
        {
            if (username.empty() || username.length() > 20)
                return false;

            if (username.back() == L'.')
                return false;

            const std::wstring invalidCharacters =
                L"\"/\\[]:;|=,+*?<>";

            for (wchar_t character : username)
            {
                if (iswcntrl(character) ||
                    invalidCharacters.find(character) != std::wstring::npos)
                {
                    return false;
                }
            }

            return true;
        }

        bool IsProtectedAccountName(const std::wstring& username)
        {
            wchar_t currentUsername[UNLEN + 1]{};
            DWORD usernameLength = UNLEN + 1;

            if (GetUserNameW(currentUsername, &usernameLength))
            {
                if (_wcsicmp(username.c_str(), currentUsername) == 0)
                    return true;
            }

            static const wchar_t* protectedNames[] =
            {
                L"Administrator",
                L"Guest",
                L"DefaultAccount",
                L"WDAGUtilityAccount"
            };

            for (const wchar_t* name : protectedNames)
            {
                if (_wcsicmp(username.c_str(), name) == 0)
                    return true;
            }

            return false;
        }

        bool IsProtectedUser(const USER_INFO_1& user)
        {
            if (IsProtectedAccountName(user.usri1_name))
                return true;

            DWORD protectedFlags =
                UF_SERVER_TRUST_ACCOUNT |
                UF_WORKSTATION_TRUST_ACCOUNT |
                UF_INTERDOMAIN_TRUST_ACCOUNT;

            return (user.usri1_flags & protectedFlags) != 0;
        }

        NET_API_STATUS GetRawUserInfo(
            const std::wstring& username,
            USER_INFO_1*& user
        )
        {
            user = nullptr;

            LPBYTE buffer = nullptr;

            NET_API_STATUS status = NetUserGetInfo(
                nullptr,
                username.c_str(),
                1,
                &buffer
            );

            if (status != NERR_Success)
                return status;

            user = reinterpret_cast<USER_INFO_1*>(buffer);

            return NERR_Success;
        }
    }

    NET_API_STATUS GetUsers(std::vector<UserInfo>& users)
    {
        users.clear();

        DWORD resumeHandle = 0;

        do
        {
            LPBYTE buffer = nullptr;

            DWORD entriesRead = 0;
            DWORD totalEntries = 0;

            NET_API_STATUS status = NetUserEnum(
                nullptr,
                2,
                FILTER_NORMAL_ACCOUNT,
                &buffer,
                MAX_PREFERRED_LENGTH,
                &entriesRead,
                &totalEntries,
                &resumeHandle
            );

            if (status != NERR_Success &&
                status != ERROR_MORE_DATA)
            {
                if (buffer)
                    NetApiBufferFree(buffer);

                return status;
            }

            USER_INFO_2* entries =
                reinterpret_cast<USER_INFO_2*>(buffer);

            for (DWORD i = 0; i < entriesRead; ++i)
            {
                UserInfo user;

                user.username =
                    entries[i].usri2_name
                    ? entries[i].usri2_name
                    : L"";

                user.fullName =
                    entries[i].usri2_full_name
                    ? entries[i].usri2_full_name
                    : L"";

                user.comment =
                    entries[i].usri2_comment
                    ? entries[i].usri2_comment
                    : L"";

                user.flags = entries[i].usri2_flags;
                user.privilege = entries[i].usri2_priv;

                user.disabled =
                    (user.flags & UF_ACCOUNTDISABLE) != 0;

                users.push_back(user);
            }

            if (buffer)
                NetApiBufferFree(buffer);

            if (status != ERROR_MORE_DATA)
                break;

        } while (true);

        return NERR_Success;
    }

    NET_API_STATUS GetUserInfo(
        const std::wstring& username,
        UserInfo& user
    )
    {
        if (username.empty())
            return ERROR_INVALID_PARAMETER;

        LPBYTE buffer = nullptr;

        NET_API_STATUS status = NetUserGetInfo(
            nullptr,
            username.c_str(),
            2,
            &buffer
        );

        if (status != NERR_Success)
            return status;

        USER_INFO_2* rawUser =
            reinterpret_cast<USER_INFO_2*>(buffer);

        user.username =
            rawUser->usri2_name
            ? rawUser->usri2_name
            : L"";

        user.fullName =
            rawUser->usri2_full_name
            ? rawUser->usri2_full_name
            : L"";

        user.comment =
            rawUser->usri2_comment
            ? rawUser->usri2_comment
            : L"";

        user.flags = rawUser->usri2_flags;
        user.privilege = rawUser->usri2_priv;

        user.disabled =
            (user.flags & UF_ACCOUNTDISABLE) != 0;

        NetApiBufferFree(buffer);

        return NERR_Success;
    }

    NET_API_STATUS CreateUser(
        const std::wstring& username,
        const std::wstring& password,
        const std::wstring& fullName
    )
    {
        if (!IsValidUsername(username))
            return ERROR_INVALID_PARAMETER;

        USER_INFO_1 userInfo{};

        userInfo.usri1_name =
            const_cast<LPWSTR>(username.c_str());

        // nullptr означает создание учётной записи без пароля.
        userInfo.usri1_password = password.empty()
            ? nullptr
            : const_cast<LPWSTR>(password.c_str());

        userInfo.usri1_priv = USER_PRIV_USER;

        userInfo.usri1_comment = nullptr;
        userInfo.usri1_home_dir = nullptr;
        userInfo.usri1_script_path = nullptr;

        userInfo.usri1_flags =
            UF_SCRIPT | UF_NORMAL_ACCOUNT;

        DWORD parameterError = 0;

        NET_API_STATUS status = NetUserAdd(
            nullptr,
            1,
            reinterpret_cast<LPBYTE>(&userInfo),
            &parameterError
        );

        if (status != NERR_Success)
            return status;

        if (!fullName.empty())
        {
            USER_INFO_1011 fullNameInfo{};

            fullNameInfo.usri1011_full_name =
                const_cast<LPWSTR>(fullName.c_str());

            status = NetUserSetInfo(
                nullptr,
                username.c_str(),
                1011,
                reinterpret_cast<LPBYTE>(&fullNameInfo),
                &parameterError
            );

            if (status != NERR_Success)
            {
                NetUserDel(nullptr, username.c_str());
                return status;
            }
        }

        return NERR_Success;
    }

    NET_API_STATUS ChangePassword(
        const std::wstring& username,
        const std::wstring& newPassword
    )
    {
        if (username.empty() || newPassword.empty())
            return ERROR_INVALID_PARAMETER;

        USER_INFO_1003 passwordInfo{};

        passwordInfo.usri1003_password =
            const_cast<LPWSTR>(newPassword.c_str());

        DWORD parameterError = 0;

        return NetUserSetInfo(
            nullptr,
            username.c_str(),
            1003,
            reinterpret_cast<LPBYTE>(&passwordInfo),
            &parameterError
        );
    }

    NET_API_STATUS EnableUser(const std::wstring& username)
    {
        if (username.empty())
            return ERROR_INVALID_PARAMETER;

        USER_INFO_1* user = nullptr;

        NET_API_STATUS status =
            GetRawUserInfo(username, user);

        if (status != NERR_Success)
            return status;

        if (IsProtectedUser(*user))
        {
            NetApiBufferFree(user);
            return ERROR_ACCESS_DENIED;
        }

        USER_INFO_1008 flagsInfo{};

        flagsInfo.usri1008_flags =
            user->usri1_flags & ~UF_ACCOUNTDISABLE;

        NetApiBufferFree(user);

        DWORD parameterError = 0;

        return NetUserSetInfo(
            nullptr,
            username.c_str(),
            1008,
            reinterpret_cast<LPBYTE>(&flagsInfo),
            &parameterError
        );
    }

    NET_API_STATUS DisableUser(const std::wstring& username)
    {
        if (username.empty())
            return ERROR_INVALID_PARAMETER;

        USER_INFO_1* user = nullptr;

        NET_API_STATUS status =
            GetRawUserInfo(username, user);

        if (status != NERR_Success)
            return status;

        if (IsProtectedUser(*user))
        {
            NetApiBufferFree(user);
            return ERROR_ACCESS_DENIED;
        }

        USER_INFO_1008 flagsInfo{};

        flagsInfo.usri1008_flags =
            user->usri1_flags | UF_ACCOUNTDISABLE;

        NetApiBufferFree(user);

        DWORD parameterError = 0;

        return NetUserSetInfo(
            nullptr,
            username.c_str(),
            1008,
            reinterpret_cast<LPBYTE>(&flagsInfo),
            &parameterError
        );
    }

    NET_API_STATUS DeleteUser(const std::wstring& username)
    {
        if (username.empty())
            return ERROR_INVALID_PARAMETER;

        USER_INFO_1* user = nullptr;

        NET_API_STATUS status =
            GetRawUserInfo(username, user);

        if (status != NERR_Success)
            return status;

        if (IsProtectedUser(*user))
        {
            NetApiBufferFree(user);
            return ERROR_ACCESS_DENIED;
        }

        NetApiBufferFree(user);

        return NetUserDel(
            nullptr,
            username.c_str()
        );
    }
}
