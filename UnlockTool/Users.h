
#pragma once

#include <windows.h>
#include <lm.h>

#include <string>
#include <vector>

#pragma comment(lib, "Netapi32.lib")

namespace Users
{
    struct UserInfo
    {
        std::wstring username;
        std::wstring fullName;
        std::wstring comment;

        DWORD flags = 0;
        DWORD privilege = 0;

        bool disabled = false;
    };

    NET_API_STATUS GetUsers(std::vector<UserInfo>& users);

    NET_API_STATUS GetUserInfo(
        const std::wstring& username,
        UserInfo& user
    );

    NET_API_STATUS CreateUser(
        const std::wstring& username,
        const std::wstring& password,
        const std::wstring& fullName = L""
    );

    NET_API_STATUS ChangePassword(
        const std::wstring& username,
        const std::wstring& newPassword
    );

    NET_API_STATUS EnableUser(const std::wstring& username);

    NET_API_STATUS DisableUser(const std::wstring& username);

    NET_API_STATUS DeleteUser(const std::wstring& username);
}
