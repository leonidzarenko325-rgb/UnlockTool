# UnlockTool 2.0.0

**Windows maintenance and recovery utility — C++ / Win32 / GDI+**

This archive contains the Visual Studio source project. The original application structure is retained and additional modules are integrated.

## Open and build

1. Install **Visual Studio 2026** with **Desktop development with C++**.
2. Make sure the Windows 10 SDK is installed.
3. Open `UnlockTool.sln` or `UnlockTool.slnx`.
4. Select `Release` and `x64`.
5. Run **Build → Build Solution**.
6. Launch the resulting `UnlockTool.exe`. The project requests administrator privileges through its manifest.

The project uses toolset `v145` and C++20. `Win32` and `x64` configurations are available.

## Integrated sections

- **Task Manager:** process list, critical-process information, suspend/resume/terminate with critical-process protection, file-location context menu, and a shortcut to the Windows Startup tab.
- **Users:** the existing local-user management module.
- **Diagnostics:** System Information, SFC verification, Windows Services and Event Viewer.
- **Recovery:** Microsoft Store reset, Windows Update settings, SFC, DISM, CHKDSK, Advanced Startup / WinRE and UAC enablement with a `.reg` backup.
- **System Cleanup:** user temporary files, thumbnail cache, Recycle Bin and DNS cache. Recycle Bin cleanup is irreversible and requires confirmation.
- **BSOD Monitor:** BugCheck events from the System Event Log, Reliability Monitor/Event Viewer shortcuts, dump-folder access and TXT/HTML export. A new-event notification is shown when the app starts; startup launch can be enabled in Settings.
- **Registry:** Shell, Userinit, CmdLine, AppInit_DLLs, SetupType and EnableCursorSuppression. Registry value types are preserved; backup and rollback are available during the current app session.
- **Disk Manager:** volumes, file systems, capacity/free space, physical disk model, GPT/MBR style and SMART failure prediction when supported; CHKDSK, Disk Management and `manage-bde -status` shortcuts.
- **Settings:** Russian/English interface, notification/startup options and application information.

## Current limitations

- Source structure and project references were checked, but **this Linux environment cannot build or run a Win32 application**. A real Visual Studio build and testing on a separate system or VM are still required.
- WinRE support is partial. Online SFC/DISM commands are blocked in WinRE to avoid servicing the recovery image by mistake. Automatic selection and servicing of an offline Windows installation are not implemented yet.
- Registry operations currently target the active `HKLM`, not an automatically loaded offline SYSTEM/SOFTWARE hive.
- Registry-page backups and rollback data are kept in memory until the application closes. UAC uses a separate `.reg` backup under `%LocalAppData%\UnlockTool\RegistryBackups`.
- BSOD Monitor exports one aggregate TXT/HTML report. Per-incident reports, filters and persistent user sorting still need work.
- Disk health prediction is shown only when the drive and driver support the relevant IOCTL. Drive-letter changes open Windows Disk Management; BitLocker status runs `manage-bde`, while opening an encrypted volume delegates unlocking to Windows.
- Full compatibility with Windows 10 1607, x86 and WinRE has not been confirmed separately.

## Project layout

- `UnlockTool/UnlockTool.vcxproj` — C++ project.
- `UnlockTool/UnlockTool.rc` and `UnlockTool/UnlockTool.ico` — application resources.
- `main.cpp` — main window, navigation and module integration.
- `RegistryRepair.*`, `RegistryPage.*` — registry functionality.
- `Cleanup.*` — cleanup operations.
- `TaskManager.*` — process management.
- `Users.*`, `UsersPage.*` — local users.
- `DiskManager.*`, `DiskManagerPage.*` — disks and volumes.
- `BSODMonitor.*`, `BSODMonitorPage.*` — BugCheck history.

Keep a current backup of important data before running recovery operations or changing system settings.
