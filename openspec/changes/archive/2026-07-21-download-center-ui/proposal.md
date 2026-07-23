## Why

Telegram Desktop has no dedicated UI for managing downloads — users get a single momentary progress overlay from a `psh` tooltip, with no way to see what's queued, pause/resume a specific transfer, inspect a failed download, or check what was received this month. The new download center window (`designs/OPENSPEC_PROPOSAL.md`, Lunacy source `designs/download_center_v2.free` + `_v3_top_*.json`) provides that surface, and the existing `Storage::ParallelDownloadController` / `Data::DownloadCenter` are already wired enough to back it — the missing piece is purely the UI layer plus a small data-layer extension for two new task states (`Waiting`, `Removed`).

The entry point lives in the main window's left sidebar, immediately below the "夜间模式" toggle, alongside "设置".

## What Changes

- **New download center window** — 1280×800 dark-themed box with sidebar (我的下载 / 下载中 / 已完成 / 失败 / 回收站 + 按媒体类型), top tab/action bar, task list (5 status variants rendered), per-task detail panel, 60-second live speed chart, and a 5-metric status bar.
- **Sidebar entry in `MainMenu`** — new menu action "下载中心" appended after the night-mode toggle, with a live badge showing the active download count.
- **`DownloadTask::Status` extension** — add `Waiting` (queued behind concurrency cap) and `Removed` (in trash) values. Persistence already in `DownloadCenter` JSON round-trips these via the new fields.
- **`DownloadTask` new fields** — `sha256`, `startedAt`, `speedHistory60s` (60-sample ring) for the detail panel.
- **`DownloadStats` aggregate type** — global counters driving the status bar (`activeCount`, `waitingCount`, `completedCount`, `failedCount`, `removedCount`, `totalDownloadedThisMonth`, `uploadBps`, `downloadBps`).
- **UI primitives** — five status badges (Downloading/Paused/Failed/Completed/Removed/Waiting), progress bar component, speed chart component, type-tinted file icon set, checkbox, action buttons (下载文件/全部继续/全部暂停/删除).
- **Dialogs and overlays** — delete confirmation dialog, failure dialog, completion toast, context menu, source/sort dropdowns, empty-state and error-state placeholders.
- **i18n** — 46 new translation keys (1 sidebar entry + 45 in-window strings).
- **New style/icon entries** — `menuIconDownloads` plus the 21 user-provided SVGs catalogued in `designs/_icons.json`.

No existing download engine code changes. `Storage::ParallelDownloadController` / `Storage::FileLoader` / `mtpFileLoader` / `Data::DownloadCenter` core paths are untouched.

## Capabilities

### New Capabilities

- `download-center-ui` — the download center window itself: window lifecycle, navigation between filter tabs, selection model, task list rendering, detail panel binding, status bar aggregation, dialogs, and toasts.
- `download-center-entry` — the sidebar entry point in `MainMenu`, including the live `activeCount` badge and the click handler that opens the window.
- `download-task-states` — the `Status` enum extension (`Waiting`, `Removed`), the `DownloadTask` field additions (`sha256`, `startedAt`, `speedHistory60s`), and the `DownloadStats` aggregate.
- `download-task-persistence` — `DownloadCenter` JSON serialization round-trips the new `Status` values and new fields, with backward-compat for older JSON files that omit them.

## Impact

- **New files** (UI layer):
  - `Telegram/SourceFiles/history/history_view_downloads.{h,cpp}`
  - `Telegram/SourceFiles/history/history_view_downloads_box.{h,cpp}`
  - `Telegram/SourceFiles/history/history_view_downloads_inner.{h,cpp}`
  - `Telegram/SourceFiles/ui/widgets/downloads/` (row / detail / status bar / chart / badge / icon helper widgets)
  - `Telegram/SourceFiles/ui/widgets/downloads/downloads_style.h` (color tokens, sizes, fonts)
- **Modified files**:
  - `Telegram/SourceFiles/data/data_download_center.h` / `.cpp` — new `Status` values, new fields, new `DownloadStats` aggregate
  - `Telegram/SourceFiles/window/window_main_menu.cpp` — append `addAction` after night-mode block (line 784) for the new sidebar entry
  - `Telegram/SourceFiles/window/window_session_controller.{h,cpp}` — add `showDownloads()` method parallel to `showSettings()`
  - `Telegram/SourceFiles/styles/style_menu_icons.{h,cpp}` — add `menuIconDownloads`
  - `Telegram/SourceFiles/lang/strings.txt` — register 46 new keys
  - `Telegram/SourceFiles/qrc/qrc_resources.cpp` (or equivalent) — register new icon resources
  - `designs/_icons.json` — register 21 new icon entries from the user-provided SVGs
- **New content**:
  - `Telegram/Resources/icons/downloads/` — 21 SVG files
- **Build impact**: new translation unit count ~12, new compile time ~30s, no new third-party dependencies.
- **Persistence impact**: existing `DownloadCenter` JSON in user data directory continues to load. Older tasks load with `Waiting` mapped to `Downloading` (default) and `Removed` items shown as `Failed` until user opens them.

## Out of Scope

- Grid view (icons reserved, list view only for v1)
- Cloud sync of download history
- Export/share of download list
- Themed UI for v1 (dark theme only; light theme tokens stubbed but not rendered)
- Custom keyboard shortcuts (deferred — see OPENSPEC_PROPOSAL.md §9.5)
