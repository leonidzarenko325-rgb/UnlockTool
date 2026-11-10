# Changelog

## 2.0.0 — 2026-11-10

- Expanded RegistryRepair to six typed parameters, including EnableCursorSuppression.
- Corrected typed registry writes for REG_DWORD and added a Windows Setup guard for SetupType/CmdLine.
- Added Recycle Bin cleanup and DNS resolver cache flushing.
- Added process context actions and a Windows Startup tab shortcut.
- Connected Diagnostics and Recovery shortcuts, including guarded WinRE behavior and UAC enablement with a `.reg` backup.
- Added BugCheck Event Log history, startup notifications, and TXT/HTML report export.
- Added a Disk Manager page with volume, file-system, disk-model, partition-style and supported health-prediction information.
- Fixed GUI subsystem configuration, UTF-8 compiler options, and the project icon path.
- Added classic `.sln` alongside `.slnx`, plus Russian and English build documentation.
