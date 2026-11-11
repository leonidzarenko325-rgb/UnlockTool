
# UnlockTool 1.0.0

**Windows maintenance and recovery utility — C++ / Win32 / GDI+**

UnlockTool is a desktop application for Windows maintenance, diagnostics, and recovery. This repository contains the Visual Studio source project, preserving the application's original architecture and integrating additional modules.

## Implemented and integrated modules

- **Task Manager:** process list, critical-process information, suspend, resume, and terminate actions with protection for critical processes, a context-menu option to open a process file location, and a shortcut to the native Windows Startup tab.
- **Users:** local user account management module.
- **Diagnostics:** system information, SFC checks, Windows services, and Event Viewer.
- **Recovery:** Microsoft Store reset, Windows Update settings, SFC, DISM, CHKDSK, Advanced Startup / WinRE, and UAC enablement with a `.reg` backup.
- **System Cleanup:** cleanup of user temporary files, thumbnail cache, Recycle Bin, and DNS cache. Recycle Bin cleanup requires confirmation and cannot be undone.
- **BSOD Monitor:** reads BugCheck events from the System Event Log, provides access to Reliability Monitor and Event Viewer, opens the dump folder, and exports TXT and HTML reports. The application can notify the user about a new entry on startup and includes an auto-start setting.
- **Registry:** manages the Shell, Userinit, CmdLine, AppInit_DLLs, SetupType, and EnableCursorSuppression parameters. Value types are checked before writing; backup and rollback are available during the current application session.
- **Disk Manager:** displays volumes, file systems, total and free space, physical disk model, GPT/MBR partition style, and SMART failure prediction when supported by the drive. It can launch CHKDSK, Disk Management, and `manage-bde -status`.
- **Settings:** Russian and English interface languages, notification and auto-start settings, and application information.

## Important limitations of the current build

- The source tree and project references have been checked for structural consistency. However, building and running a Win32 application in the Linux environment used for that check is not possible. A real build in Visual Studio and testing on a separate system or virtual machine are required before use.
- WinRE support is currently partial. Online SFC and DISM commands are not launched from WinRE, preventing accidental servicing of the recovery environment image itself. Automatic detection and servicing of an offline Windows installation have not yet been implemented.
- Registry operations currently target the active `HKLM` hive. Automatically loading an offline `SYSTEM` or `SOFTWARE` hive is not implemented yet.
- Registry page backups and rollback data are kept in memory until the application is closed. UAC operations create a separate `.reg` file in `%LocalAppData%\UnlockTool\RegistryBackups`.
- BSOD Monitor creates a combined TXT or HTML report from detected BugCheck events. Separate reports for each incident, filtering, and persistence of user-selected sorting still need further work.
- Disk Manager displays SMART failure prediction only when the drive and its driver support the required IOCTL. Changing drive letters opens the native Disk Management utility. BitLocker status checks launch `manage-bde`; unlocking an encrypted volume is handled by Windows.
- Full compatibility with Windows 10 1607, the x86 architecture, and WinRE has not been independently confirmed.

## Project structure

- `UnlockTool/UnlockTool.vcxproj` — C++ project file.
- `UnlockTool/UnlockTool.rc` and `UnlockTool/UnlockTool.ico` — application resources.
- `main.cpp` — main window, navigation, and module integration.
- `RegistryRepair.*`, `RegistryPage.*` — registry parameter operations.
- `Cleanup.*` — cleanup tools.
- `TaskManager.*` — process management.
- `Users.*`, `UsersPage.*` — user management.
- `DiskManager.*`, `DiskManagerPage.*` — disk and volume management.
- `BSODMonitor.*`, `BSODMonitorPage.*` — BugCheck event history.

## Safety

Recovery operations and system-configuration changes can affect Windows. Create a current backup of important data before using them. Review each operation carefully and test the application in a virtual machine or another safe environment.

## License

A license will be specified separately once one has been selected. Until then, all rights to the source code remain with the copyright holder.

## Community

Follow the official UnlockTool Telegram channel for news, updates, and release announcements.

**Telegram:** [@UnlockToolProject](https://t.me/UnlockToolProject)
