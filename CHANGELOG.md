
# Changelog

All notable changes to UnlockTool are documented in this file.

## [1.2.0] — 2026-11-18

### Interface
- Redesigned navigation icon animation behavior.
- Added individual animations for navigation icons.
- Icons animate only when navigating to their corresponding sections.
- Removed continuous hover-triggered icon animations.
- Added smooth one-shot animations with automatic return to the resting state.
- Animations replay when navigating to a section again.
- Animations pause when the application window is inactive or minimized.
- Refined active navigation item appearance.
- Improved visual consistency across application sections.

### Improvements
- Updated application version labels to 1.2.0.
- Improved Russian and English documentation.
- Preserved the existing Win32 and GDI+ architecture.
- Preserved existing page logic and domain modules.

## [1.1.0] — 2026-11-16

### Added
- Added an integrated Startup Manager.
- Added custom registry value editing.
- Added individual sidebar icon animations.
- Added a redesigned Home overview with system, memory, storage, and process indicators.
- Added custom layouts for application sections.

### Changed
- Completely redesigned the graphical interface.
- Introduced a new sidebar navigation and top header.
- Reworked the application layout and page organization.
- Replaced inconsistent sidebar symbols with custom GDI+ vector icons.
- Updated the visual style with a neutral dark color palette.
- Improved active navigation styling and application branding.

### Improved
- Improved process search and navigation.
- Added mouse-wheel scrolling support.
- Improved right-click process actions.
- Improved overall application usability.
- Preserved existing core modules and their functionality.

### Fixes
- Fixed GDI+ drawing overload ambiguities.
- Fixed a missing closing brace in the process context-menu handler.
- Corrected resource file encoding.
- Improved project source organization.

## [1.0.1] — 2026-11-12

### Improved
- Improved application stability.
- Improved existing tools and features.
- Optimized selected application components.
- Made minor user interface improvements.

### Fixed
- Fixed various bugs.
- Applied maintenance fixes across the application.

### Compatibility
- Windows 10 support.
- Windows 11 support.
- WinRE compatibility testing continued.

## [1.0.0] — 2026-11-10

### Initial Release
- First public release of UnlockTool.

### Features
- **Task Manager** — process monitoring and management.
- **Users** — local user account management.
- **Diagnostics** — system information, SFC checks, Windows services, and Event Viewer.
- **Recovery** — SFC, DISM, CHKDSK, Microsoft Store reset, Windows Update settings, and UAC management.
- **System Cleanup** — temporary files, thumbnail cache, Recycle Bin, and DNS cache cleanup.
- **BSOD Monitor** — BugCheck event monitoring and TXT/HTML report export.
- **Registry** — registry parameter management with backup and rollback during the current session.
- **Disk Manager** — disk and volume information, GPT/MBR, SMART prediction, and BitLocker status.
- **Settings** — Russian and English interface, notifications, and auto-start options.

### Technical Details
- Developed in C++.
- Built using the Win32 API and GDI+.
- MSVC v145 toolset.
- C++20 standard.
- Win32 and x64 architecture configurations.
- Visual Studio 2026 project.

---

## Versioning

- **1.0.x** — Initial release and maintenance updates.
- **1.1.x** — Interface redesign and feature improvements.
- **1.2.x** — Interface refinement and navigation improvements.
- **2.0.0** — Reserved for a future major release.

