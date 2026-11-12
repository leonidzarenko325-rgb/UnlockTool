
#include "UsersPage.h"
#include "Users.h"

#include <windows.h>
#include <lm.h>

#include <string>
#include <vector>
#include <utility>
#include <cwchar>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Netapi32.lib")

using namespace Gdiplus;

namespace UsersPage
{
    namespace
    {
        // ====================================================
        // СОСТОЯНИЕ СТРАНИЦЫ
        // ====================================================

        std::vector<Users::UserInfo> users;

        std::wstring selectedUsername;

        bool usersLoaded = false;

        constexpr int MAX_VISIBLE_USERS = 10;

        RECT refreshRect{};
        RECT createRect{};
        RECT passwordRect{};
        RECT toggleRect{};
        RECT deleteRect{};

        RECT userRows[MAX_VISIBLE_USERS]{};

        // ====================================================
        // ЦВЕТА
        // ====================================================

        const Color PANEL(255, 23, 29, 43);
        const Color CARD(255, 27, 34, 50);
        const Color CARD_DISABLED(255, 35, 40, 53);

        const Color ACCENT(255, 65, 125, 255);
        const Color ACCENT_LIGHT(255, 100, 175, 255);

        const Color TEXT_MAIN(255, 240, 244, 252);
        const Color TEXT_SECONDARY(255, 145, 158, 181);
        const Color TEXT_MUTED(255, 105, 119, 143);

        const Color BORDER(255, 43, 53, 73);

        const Color SUCCESS(255, 70, 190, 135);
        const Color WARNING(255, 225, 165, 75);
        const Color DANGER(255, 225, 95, 100);

        // ====================================================
        // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
        // ====================================================

        bool IsRussian(int language)
        {
            return language == 0;
        }

        void SecureClear(std::wstring& value)
        {
            if (!value.empty())
            {
                SecureZeroMemory(
                    &value[0],
                    value.size() * sizeof(wchar_t)
                );
            }

            value.clear();
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
                x,
                y,
                diameter,
                diameter,
                180,
                90
            );

            path.AddArc(
                x + width - diameter,
                y,
                diameter,
                diameter,
                270,
                90
            );

            path.AddArc(
                x + width - diameter,
                y + height - diameter,
                diameter,
                diameter,
                0,
                90
            );

            path.AddArc(
                x,
                y + height - diameter,
                diameter,
                diameter,
                90,
                90
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
                x,
                y,
                width,
                height,
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
                x,
                y,
                width,
                height,
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
                x,
                y,
                width,
                height
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
            const std::wstring& text,
            bool primary,
            bool enabled = true,
            Color customColor = Color(0, 0, 0, 0))
        {
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            if (primary && enabled)
            {
                GraphicsPath path;

                MakePath(
                    path,
                    static_cast<float>(rect.left),
                    static_cast<float>(rect.top),
                    static_cast<float>(width),
                    static_cast<float>(height),
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
            }
            else
            {
                Color background = enabled
                    ? CARD
                    : CARD_DISABLED;

                if (customColor.GetA() != 0 && enabled)
                    background = customColor;

                FillRounded(
                    graphics,
                    static_cast<float>(rect.left),
                    static_cast<float>(rect.top),
                    static_cast<float>(width),
                    static_cast<float>(height),
                    9.0f,
                    background
                );

                DrawRounded(
                    graphics,
                    static_cast<float>(rect.left),
                    static_cast<float>(rect.top),
                    static_cast<float>(width),
                    static_cast<float>(height),
                    9.0f,
                    BORDER
                );
            }

            Font font(
                L"Segoe UI",
                10.5f,
                FontStyleBold,
                UnitPixel
            );

            SolidBrush textBrush(
                enabled ? TEXT_MAIN : TEXT_MUTED
            );

            RectF textRect(
                static_cast<float>(rect.left + 5),
                static_cast<float>(rect.top),
                static_cast<float>(width - 10),
                static_cast<float>(height)
            );

            StringFormat format;

            format.SetAlignment(
                StringAlignmentCenter
            );

            format.SetLineAlignment(
                StringAlignmentCenter
            );

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
        // ПОИСК ПОЛЬЗОВАТЕЛЕЙ
        // ====================================================

        Users::UserInfo* FindUser(
            const std::wstring& username)
        {
            for (auto& user : users)
            {
                if (_wcsicmp(
                    user.username.c_str(),
                    username.c_str()) == 0)
                {
                    return &user;
                }
            }

            return nullptr;
        }

        Users::UserInfo* GetSelectedUser()
        {
            if (selectedUsername.empty())
                return nullptr;

            return FindUser(selectedUsername);
        }

        // ====================================================
        // ОБНОВЛЕНИЕ СПИСКА
        // ====================================================

        bool RefreshUsers(
            std::wstring& statusText,
            int language)
        {
            std::vector<Users::UserInfo> newUsers;

            NET_API_STATUS result =
                Users::GetUsers(newUsers);

            if (result != NERR_Success)
            {
                users.clear();
                selectedUsername.clear();

                usersLoaded = false;

                statusText = IsRussian(language)
                    ? L"Не удалось получить список пользователей. Код: "
                    : L"Could not retrieve users. Error code: ";

                statusText += std::to_wstring(result);

                return false;
            }

            users = std::move(newUsers);

            usersLoaded = true;

            if (!selectedUsername.empty() &&
                !FindUser(selectedUsername))
            {
                selectedUsername.clear();
            }

            statusText = IsRussian(language)
                ? L"Список пользователей обновлён"
                : L"User list refreshed";

            return true;
        }

        // ====================================================
        // ДИАЛОГИ
        // ====================================================

        struct DialogData
        {
            HWND window = nullptr;

            HWND usernameEdit = nullptr;
            HWND fullNameEdit = nullptr;
            HWND passwordEdit = nullptr;

            bool createMode = false;
            bool accepted = false;

            int language = 0;

            std::wstring username;
            std::wstring fullName;
            std::wstring password;
        };

        constexpr int ID_USERNAME = 1001;
        constexpr int ID_FULLNAME = 1002;
        constexpr int ID_PASSWORD = 1003;

        constexpr int ID_OK = 1004;
        constexpr int ID_CANCEL = 1005;

        LRESULT CALLBACK UserDialogProc(
            HWND hwnd,
            UINT message,
            WPARAM wParam,
            LPARAM lParam)
        {
            DialogData* data =
                reinterpret_cast<DialogData*>(
                    GetWindowLongPtrW(
                        hwnd,
                        GWLP_USERDATA
                    )
                    );

            switch (message)
            {
            case WM_NCCREATE:
            {
                auto create =
                    reinterpret_cast<CREATESTRUCTW*>(lParam);

                data = static_cast<DialogData*>(
                    create->lpCreateParams
                    );

                SetWindowLongPtrW(
                    hwnd,
                    GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(data)
                );

                data->window = hwnd;

                return TRUE;
            }

            case WM_CREATE:
            {
                if (!data)
                    return -1;

                HFONT font = static_cast<HFONT>(
                    GetStockObject(DEFAULT_GUI_FONT)
                    );

                bool russian = IsRussian(data->language);

                auto addLabel = [&](const wchar_t* text,
                    int x,
                    int y,
                    int width)
                    {
                        HWND control = CreateWindowExW(
                            0,
                            L"STATIC",
                            text,
                            WS_CHILD | WS_VISIBLE,
                            x,
                            y,
                            width,
                            22,
                            hwnd,
                            nullptr,
                            GetModuleHandleW(nullptr),
                            nullptr
                        );

                        if (control)
                        {
                            SendMessageW(
                                control,
                                WM_SETFONT,
                                reinterpret_cast<WPARAM>(font),
                                TRUE
                            );
                        }
                    };

                auto addEdit = [&](int id,
                    int x,
                    int y,
                    int width,
                    DWORD style)
                    {
                        HWND control = CreateWindowExW(
                            WS_EX_CLIENTEDGE,
                            L"EDIT",
                            L"",
                            WS_CHILD |
                            WS_VISIBLE |
                            WS_TABSTOP |
                            ES_AUTOHSCROLL |
                            style,
                            x,
                            y,
                            width,
                            27,
                            hwnd,
                            reinterpret_cast<HMENU>(
                                static_cast<INT_PTR>(id)
                                ),
                            GetModuleHandleW(nullptr),
                            nullptr
                        );

                        if (control)
                        {
                            SendMessageW(
                                control,
                                WM_SETFONT,
                                reinterpret_cast<WPARAM>(font),
                                TRUE
                            );
                        }

                        return control;
                    };

                if (data->createMode)
                {
                    addLabel(
                        russian
                        ? L"Имя пользователя"
                        : L"Username",
                        22, 18, 350
                    );

                    data->usernameEdit =
                        addEdit(
                            ID_USERNAME,
                            22, 40, 350, 0
                        );

                    addLabel(
                        russian
                        ? L"Полное имя (необязательно)"
                        : L"Full name (optional)",
                        22, 78, 350
                    );

                    data->fullNameEdit =
                        addEdit(
                            ID_FULLNAME,
                            22, 100, 350, 0
                        );

                    addLabel(
                        russian
                        ? L"Пароль (необязательно)"
                        : L"Password (optional)",
                        22, 138, 350
                    );

                    data->passwordEdit =
                        addEdit(
                            ID_PASSWORD,
                            22, 160, 350,
                            ES_PASSWORD
                        );
                }
                else
                {
                    addLabel(
                        russian
                        ? L"Новый пароль"
                        : L"New password",
                        22, 75, 350
                    );

                    data->passwordEdit =
                        addEdit(
                            ID_PASSWORD,
                            22, 100, 350,
                            ES_PASSWORD
                        );
                }

                HWND okButton = CreateWindowExW(
                    0,
                    L"BUTTON",
                    russian ? L"Сохранить" : L"Save",
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_DEFPUSHBUTTON,
                    190,
                    245,
                    85,
                    32,
                    hwnd,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(ID_OK)
                        ),
                    GetModuleHandleW(nullptr),
                    nullptr
                );

                HWND cancelButton = CreateWindowExW(
                    0,
                    L"BUTTON",
                    russian ? L"Отмена" : L"Cancel",
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP,
                    287,
                    245,
                    85,
                    32,
                    hwnd,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(ID_CANCEL)
                        ),
                    GetModuleHandleW(nullptr),
                    nullptr
                );

                if (okButton)
                {
                    SendMessageW(
                        okButton,
                        WM_SETFONT,
                        reinterpret_cast<WPARAM>(font),
                        TRUE
                    );
                }

                if (cancelButton)
                {
                    SendMessageW(
                        cancelButton,
                        WM_SETFONT,
                        reinterpret_cast<WPARAM>(font),
                        TRUE
                    );
                }

                return 0;
            }

            case WM_COMMAND:
            {
                if (!data)
                    break;

                int id = LOWORD(wParam);

                if (id == ID_CANCEL)
                {
                    if (data->passwordEdit)
                        SetWindowTextW(data->passwordEdit, L"");

                    DestroyWindow(hwnd);

                    return 0;
                }

                if (id == ID_OK)
                {
                    auto readText = [](HWND edit)
                        {
                            int length =
                                GetWindowTextLengthW(edit);

                            std::wstring value(
                                static_cast<size_t>(length) + 1,
                                L'\0'
                            );

                            if (length > 0)
                            {
                                GetWindowTextW(
                                    edit,
                                    value.data(),
                                    length + 1
                                );
                            }

                            value.resize(
                                static_cast<size_t>(length)
                            );

                            return value;
                        };

                    data->password =
                        readText(data->passwordEdit);

                    // При смене пароля пустое значение запрещено.
                    // При создании пользователя оно разрешено.

                    if (data->password.empty() &&
                        !data->createMode)
                    {
                        MessageBoxW(
                            hwnd,
                            IsRussian(data->language)
                            ? L"Введите новый пароль."
                            : L"Enter a new password.",
                            L"UnlockTool",
                            MB_OK | MB_ICONWARNING
                        );

                        SetFocus(data->passwordEdit);

                        return 0;
                    }

                    if (data->createMode)
                    {
                        data->username =
                            readText(data->usernameEdit);

                        data->fullName =
                            readText(data->fullNameEdit);

                        if (data->username.empty())
                        {
                            MessageBoxW(
                                hwnd,
                                IsRussian(data->language)
                                ? L"Введите имя пользователя."
                                : L"Enter a username.",
                                L"UnlockTool",
                                MB_OK | MB_ICONWARNING
                            );

                            SetFocus(data->usernameEdit);

                            SecureClear(data->password);

                            return 0;
                        }
                    }

                    data->accepted = true;

                    if (data->passwordEdit)
                        SetWindowTextW(data->passwordEdit, L"");

                    DestroyWindow(hwnd);

                    return 0;
                }

                break;
            }

            case WM_CLOSE:
            {
                if (data && data->passwordEdit)
                    SetWindowTextW(data->passwordEdit, L"");

                DestroyWindow(hwnd);

                return 0;
            }
            }

            return DefWindowProcW(
                hwnd,
                message,
                wParam,
                lParam
            );
        }

        bool ShowUserDialog(
            HWND owner,
            bool createMode,
            int language,
            std::wstring& username,
            std::wstring& fullName,
            std::wstring& password)
        {
            static bool classRegistered = false;

            const wchar_t* className =
                L"UnlockToolUserInputDialog";

            if (!classRegistered)
            {
                WNDCLASSEXW wc{};

                wc.cbSize = sizeof(wc);
                wc.lpfnWndProc = UserDialogProc;
                wc.hInstance = GetModuleHandleW(nullptr);
                wc.lpszClassName = className;
                wc.hCursor = LoadCursorW(
                    nullptr,
                    IDC_ARROW
                );

                wc.hbrBackground =
                    reinterpret_cast<HBRUSH>(
                        COLOR_BTNFACE + 1
                        );

                if (!RegisterClassExW(&wc) &&
                    GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                {
                    return false;
                }

                classRegistered = true;
            }

            DialogData data{};

            data.createMode = createMode;
            data.language = language;

            bool russian = IsRussian(language);

            HWND dialog = CreateWindowExW(
                WS_EX_DLGMODALFRAME,
                className,
                createMode
                ? (russian
                    ? L"Создание пользователя"
                    : L"Create user")
                : (russian
                    ? L"Изменение пароля"
                    : L"Change password"),
                WS_CAPTION |
                WS_SYSMENU,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                410,
                330,
                owner,
                nullptr,
                GetModuleHandleW(nullptr),
                &data
            );

            if (!dialog)
                return false;

            if (owner)
                EnableWindow(owner, FALSE);

            RECT ownerRect{};
            RECT dialogRect{};

            if (owner)
                GetWindowRect(owner, &ownerRect);

            GetWindowRect(dialog, &dialogRect);

            int dialogWidth =
                dialogRect.right - dialogRect.left;

            int dialogHeight =
                dialogRect.bottom - dialogRect.top;

            int x = owner
                ? ownerRect.left +
                ((ownerRect.right - ownerRect.left) -
                    dialogWidth) / 2
                : CW_USEDEFAULT;

            int y = owner
                ? ownerRect.top +
                ((ownerRect.bottom - ownerRect.top) -
                    dialogHeight) / 2
                : CW_USEDEFAULT;

            SetWindowPos(
                dialog,
                HWND_TOP,
                x,
                y,
                0,
                0,
                SWP_NOSIZE
            );

            ShowWindow(dialog, SW_SHOW);
            UpdateWindow(dialog);

            SetFocus(
                createMode
                ? data.usernameEdit
                : data.passwordEdit
            );

            MSG message{};

            while (IsWindow(dialog))
            {
                BOOL result = GetMessageW(
                    &message,
                    nullptr,
                    0,
                    0
                );

                if (result == -1)
                    break;

                if (result == 0)
                {
                    PostQuitMessage(
                        static_cast<int>(message.wParam)
                    );

                    break;
                }

                if (!IsDialogMessageW(dialog, &message))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }

            if (owner)
            {
                EnableWindow(owner, TRUE);
                SetForegroundWindow(owner);
            }

            if (!data.accepted)
            {
                SecureClear(data.password);
                SecureClear(data.username);
                SecureClear(data.fullName);

                return false;
            }

            username = data.username;
            fullName = data.fullName;
            password = data.password;

            SecureClear(data.password);
            SecureClear(data.username);
            SecureClear(data.fullName);

            return true;
        }

        // ====================================================
        // СОЗДАНИЕ ПОЛЬЗОВАТЕЛЯ
        // ====================================================

        void CreateNewUser(
            HWND hwnd,
            int language,
            std::wstring& statusText)
        {
            std::wstring username;
            std::wstring fullName;
            std::wstring password;

            if (!ShowUserDialog(
                hwnd,
                true,
                language,
                username,
                fullName,
                password))
            {
                return;
            }

            // Дополнительное подтверждение создания
            // учётной записи без пароля.

            if (password.empty())
            {
                int answer = MessageBoxW(
                    hwnd,
                    IsRussian(language)
                    ? L"Вы создаёте пользователя без пароля.\n\n"
                    L"Это снижает безопасность учётной записи. "
                    L"Продолжить?"
                    : L"You are creating an account without a password.\n\n"
                    L"This reduces account security. Continue?",
                    L"UnlockTool",
                    MB_YESNO |
                    MB_ICONWARNING |
                    MB_DEFBUTTON2
                );

                if (answer != IDYES)
                {
                    SecureClear(username);
                    SecureClear(fullName);
                    SecureClear(password);

                    return;
                }
            }

            NET_API_STATUS result =
                Users::CreateUser(
                    username,
                    password,
                    fullName
                );

            SecureClear(password);

            if (result == NERR_Success)
            {
                RefreshUsers(statusText, language);

                selectedUsername = username;

                statusText = IsRussian(language)
                    ? L"Пользователь успешно создан"
                    : L"User created successfully";
            }
            else
            {
                statusText = IsRussian(language)
                    ? L"Не удалось создать пользователя. Код: "
                    : L"Could not create user. Error code: ";

                statusText += std::to_wstring(result);

                MessageBoxW(
                    hwnd,
                    statusText.c_str(),
                    L"UnlockTool",
                    MB_OK | MB_ICONERROR
                );
            }

            SecureClear(username);
            SecureClear(fullName);
        }

        // ====================================================
        // ИЗМЕНЕНИЕ ПАРОЛЯ
        // ====================================================

        void ChangeSelectedPassword(
            HWND hwnd,
            int language,
            std::wstring& statusText)
        {
            Users::UserInfo* user = GetSelectedUser();

            if (!user)
                return;

            std::wstring username = user->username;
            std::wstring fullName;
            std::wstring password;

            if (!ShowUserDialog(
                hwnd,
                false,
                language,
                username,
                fullName,
                password))
            {
                SecureClear(username);
                return;
            }

            NET_API_STATUS result =
                Users::ChangePassword(
                    username,
                    password
                );

            SecureClear(password);

            if (result == NERR_Success)
            {
                statusText = IsRussian(language)
                    ? L"Пароль успешно изменён"
                    : L"Password changed successfully";
            }
            else
            {
                statusText = IsRussian(language)
                    ? L"Не удалось изменить пароль. Код: "
                    : L"Could not change password. Error code: ";

                statusText += std::to_wstring(result);

                MessageBoxW(
                    hwnd,
                    statusText.c_str(),
                    L"UnlockTool",
                    MB_OK | MB_ICONERROR
                );
            }

            SecureClear(username);
        }

        // ====================================================
        // ВКЛЮЧЕНИЕ И ОТКЛЮЧЕНИЕ
        // ====================================================

        void SetSelectedUserEnabled(
            HWND hwnd,
            int language,
            std::wstring& statusText,
            bool enabled)
        {
            Users::UserInfo* user = GetSelectedUser();

            if (!user)
                return;

            std::wstring username = user->username;

            std::wstring message =
                IsRussian(language)
                ? (enabled
                    ? L"Включить учётную запись "
                    : L"Отключить учётную запись ")
                : (enabled
                    ? L"Enable account "
                    : L"Disable account ");

            message += username;
            message += L"?";

            int answer = MessageBoxW(
                hwnd,
                message.c_str(),
                L"UnlockTool",
                MB_YESNO |
                MB_ICONWARNING |
                MB_DEFBUTTON2
            );

            if (answer != IDYES)
            {
                SecureClear(username);
                return;
            }

            NET_API_STATUS result = enabled
                ? Users::EnableUser(username)
                : Users::DisableUser(username);

            if (result == NERR_Success)
            {
                RefreshUsers(statusText, language);

                statusText = IsRussian(language)
                    ? (enabled
                        ? L"Учётная запись включена"
                        : L"Учётная запись отключена")
                    : (enabled
                        ? L"Account enabled"
                        : L"Account disabled");
            }
            else
            {
                statusText = IsRussian(language)
                    ? L"Операция не выполнена. Код: "
                    : L"Operation failed. Error code: ";

                statusText += std::to_wstring(result);

                MessageBoxW(
                    hwnd,
                    statusText.c_str(),
                    L"UnlockTool",
                    MB_OK | MB_ICONERROR
                );
            }

            SecureClear(username);
        }

        // ====================================================
        // УДАЛЕНИЕ ПОЛЬЗОВАТЕЛЯ
        // ====================================================

        void DeleteSelectedUser(
            HWND hwnd,
            int language,
            std::wstring& statusText)
        {
            Users::UserInfo* user = GetSelectedUser();

            if (!user)
                return;

            std::wstring username = user->username;

            std::wstring message =
                IsRussian(language)
                ? L"Вы действительно хотите удалить пользователя "
                : L"Do you really want to delete user ";

            message += username;
            message += L"?";

            message += IsRussian(language)
                ? L"\n\nЭто действие нельзя отменить."
                : L"\n\nThis action cannot be undone.";

            int answer = MessageBoxW(
                hwnd,
                message.c_str(),
                L"UnlockTool",
                MB_YESNO |
                MB_ICONWARNING |
                MB_DEFBUTTON2
            );

            if (answer != IDYES)
            {
                SecureClear(username);
                return;
            }

            NET_API_STATUS result =
                Users::DeleteUser(username);

            if (result == NERR_Success)
            {
                selectedUsername.clear();

                RefreshUsers(statusText, language);

                statusText = IsRussian(language)
                    ? L"Пользователь удалён"
                    : L"User deleted";
            }
            else
            {
                statusText = IsRussian(language)
                    ? L"Не удалось удалить пользователя. Код: "
                    : L"Could not delete user. Error code: ";

                statusText += std::to_wstring(result);

                MessageBoxW(
                    hwnd,
                    statusText.c_str(),
                    L"UnlockTool",
                    MB_OK | MB_ICONERROR
                );
            }

            SecureClear(username);
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
        selectedUsername.clear();

        RefreshUsers(
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
            ? L"Локальные учётные записи"
            : L"Local user accounts",
            270,
            100,
            500,
            30,
            17,
            TEXT_MAIN,
            true
        );

        std::wstring countText =
            russian
            ? L"Всего пользователей: "
            : L"Total users: ";

        countText += std::to_wstring(users.size());

        DrawText(
            graphics,
            countText,
            270,
            130,
            400,
            20,
            10,
            TEXT_SECONDARY
        );

        refreshRect = {
            900, 105, 1060, 145
        };

        DrawButton(
            graphics,
            refreshRect,
            russian ? L"Обновить" : L"Refresh",
            true
        );

        // ====================================================
        // ЛЕВАЯ ПАНЕЛЬ
        // ====================================================

        FillRounded(
            graphics,
            270, 165,
            490, 455,
            13,
            PANEL
        );

        DrawRounded(
            graphics,
            270, 165,
            490, 455,
            13,
            BORDER
        );

        DrawText(
            graphics,
            russian ? L"ПОЛЬЗОВАТЕЛЬ" : L"USER",
            290, 178,
            300, 28,
            10,
            TEXT_MUTED,
            true
        );

        DrawText(
            graphics,
            russian ? L"СТАТУС" : L"STATUS",
            625, 178,
            110, 28,
            10,
            TEXT_MUTED,
            true
        );

        for (int i = 0; i < MAX_VISIBLE_USERS; ++i)
        {
            int rowY = 215 + i * 37;

            userRows[i] = {
                282,
                rowY,
                748,
                rowY + 33
            };

            if (i >= static_cast<int>(users.size()))
                continue;

            const auto& user = users[i];

            bool selected =
                _wcsicmp(
                    user.username.c_str(),
                    selectedUsername.c_str()
                ) == 0;

            if (selected)
            {
                FillRounded(
                    graphics,
                    282,
                    static_cast<float>(rowY),
                    466,
                    33,
                    7,
                    Color(255, 39, 75, 125)
                );

                DrawRounded(
                    graphics,
                    282,
                    static_cast<float>(rowY),
                    466,
                    33,
                    7,
                    ACCENT
                );
            }
            else if (i % 2 == 0)
            {
                FillRounded(
                    graphics,
                    282,
                    static_cast<float>(rowY),
                    466,
                    33,
                    7,
                    CARD
                );
            }

            std::wstring displayName =
                user.fullName.empty()
                ? user.username
                : user.fullName;

            if (displayName.length() > 28)
            {
                displayName =
                    displayName.substr(0, 25) + L"...";
            }

            DrawText(
                graphics,
                displayName,
                295,
                static_cast<float>(rowY),
                300,
                33,
                11,
                selected ? TEXT_MAIN : TEXT_SECONDARY,
                selected
            );

            std::wstring accountStatus =
                user.disabled
                ? (russian ? L"Отключён" : L"Disabled")
                : (russian ? L"Активен" : L"Enabled");

            DrawText(
                graphics,
                accountStatus,
                625,
                static_cast<float>(rowY),
                110,
                33,
                10,
                user.disabled ? WARNING : SUCCESS
            );
        }

        if (!usersLoaded)
        {
            DrawText(
                graphics,
                russian
                ? L"Не удалось загрузить список пользователей"
                : L"Could not load user list",
                290, 300,
                440, 45,
                12,
                WARNING
            );
        }
        else if (users.empty())
        {
            DrawText(
                graphics,
                russian
                ? L"Локальные пользователи не найдены"
                : L"No local users found",
                290, 300,
                440, 45,
                12,
                TEXT_SECONDARY
            );
        }
        else if (users.size() > MAX_VISIBLE_USERS)
        {
            DrawText(
                graphics,
                russian
                ? L"Показаны первые 10 пользователей"
                : L"Showing the first 10 users",
                290, 590,
                440, 18,
                9,
                TEXT_MUTED
            );
        }

        // ====================================================
        // ПРАВАЯ ПАНЕЛЬ
        // ====================================================

        FillRounded(
            graphics,
            775, 165,
            285, 455,
            13,
            PANEL
        );

        DrawRounded(
            graphics,
            775, 165,
            285, 455,
            13,
            BORDER
        );

        DrawText(
            graphics,
            russian ? L"Информация" : L"Information",
            795, 178,
            240, 28,
            15,
            TEXT_MAIN,
            true
        );

        Users::UserInfo* selected = GetSelectedUser();

        if (selected)
        {
            DrawText(
                graphics,
                russian ? L"Имя пользователя" : L"Username",
                795, 215,
                240, 18,
                9,
                TEXT_MUTED
            );

            DrawText(
                graphics,
                selected->username,
                795, 234,
                240, 24,
                12,
                TEXT_MAIN,
                true
            );

            DrawText(
                graphics,
                russian ? L"Полное имя" : L"Full name",
                795, 265,
                240, 18,
                9,
                TEXT_MUTED
            );

            DrawText(
                graphics,
                selected->fullName.empty()
                ? L"—"
                : selected->fullName,
                795, 284,
                240, 24,
                11,
                TEXT_SECONDARY
            );

            DrawText(
                graphics,
                russian ? L"Комментарий" : L"Comment",
                795, 315,
                240, 18,
                9,
                TEXT_MUTED
            );

            DrawText(
                graphics,
                selected->comment.empty()
                ? L"—"
                : selected->comment,
                795, 334,
                240, 30,
                10,
                TEXT_SECONDARY
            );

            DrawText(
                graphics,
                russian ? L"Уровень NetAPI" : L"NetAPI privilege",
                795, 370,
                240, 18,
                9,
                TEXT_MUTED
            );

            DrawText(
                graphics,
                selected->privilege == USER_PRIV_ADMIN
                ? (russian
                    ? L"Администратор NetAPI"
                    : L"NetAPI administrator")
                : (russian
                    ? L"Обычный пользователь"
                    : L"Standard user"),
                795, 389,
                240, 24,
                10,
                TEXT_SECONDARY
            );
        }
        else
        {
            DrawText(
                graphics,
                russian
                ? L"Выберите пользователя из списка"
                : L"Select a user from the list",
                795, 230,
                240, 65,
                11,
                TEXT_SECONDARY
            );
        }

        // ====================================================
        // КНОПКИ
        // ====================================================

        createRect = {
            795, 425, 1040, 461
        };

        passwordRect = {
            795, 470, 1040, 506
        };

        toggleRect = {
            795, 515, 1040, 551
        };

        deleteRect = {
            795, 560, 1040, 596
        };

        bool hasSelection = selected != nullptr;

        DrawButton(
            graphics,
            createRect,
            russian
            ? L"Создать пользователя"
            : L"Create user",
            true
        );

        DrawButton(
            graphics,
            passwordRect,
            russian
            ? L"Изменить пароль"
            : L"Change password",
            false,
            hasSelection
        );

        bool toggleEnabled =
            hasSelection;

        std::wstring toggleText;

        if (!hasSelection)
        {
            toggleText = russian
                ? L"Включить / отключить"
                : L"Enable / disable";
        }
        else if (selected->disabled)
        {
            toggleText = russian
                ? L"Включить учётную запись"
                : L"Enable account";
        }
        else
        {
            toggleText = russian
                ? L"Отключить учётную запись"
                : L"Disable account";
        }

        DrawButton(
            graphics,
            toggleRect,
            toggleText,
            false,
            toggleEnabled,
            hasSelection && selected->disabled
            ? SUCCESS
            : Color(0, 0, 0, 0)
        );

        DrawButton(
            graphics,
            deleteRect,
            russian
            ? L"Удалить пользователя"
            : L"Delete user",
            false,
            hasSelection,
            DANGER
        );
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
        if (Contains(refreshRect, x, y))
        {
            RefreshUsers(
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

        // Выбор пользователя.

        for (int i = 0; i < MAX_VISIBLE_USERS; ++i)
        {
            if (Contains(userRows[i], x, y))
            {
                if (i < static_cast<int>(users.size()))
                {
                    selectedUsername =
                        users[i].username;

                    statusText = IsRussian(language)
                        ? L"Выбран пользователь: "
                        : L"Selected user: ";

                    statusText += selectedUsername;
                }

                InvalidateRect(
                    hwnd,
                    nullptr,
                    FALSE
                );

                return true;
            }
        }

        // Создание пользователя.

        if (Contains(createRect, x, y))
        {
            CreateNewUser(
                hwnd,
                language,
                statusText
            );

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE
            );

            return true;
        }

        // Изменение пароля.

        if (Contains(passwordRect, x, y))
        {
            if (GetSelectedUser())
            {
                ChangeSelectedPassword(
                    hwnd,
                    language,
                    statusText
                );
            }

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE
            );

            return true;
        }

        // Включение или отключение.

        if (Contains(toggleRect, x, y))
        {
            Users::UserInfo* user = GetSelectedUser();

            if (user)
            {
                SetSelectedUserEnabled(
                    hwnd,
                    language,
                    statusText,
                    user->disabled
                );
            }

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE
            );

            return true;
        }

        // Удаление пользователя.

        if (Contains(deleteRect, x, y))
        {
            if (GetSelectedUser())
            {
                DeleteSelectedUser(
                    hwnd,
                    language,
                    statusText
                );
            }

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
