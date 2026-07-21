## Why

The previous download-center UI (`openspec/changes/download-center-ui/`) was rolled back because clicking the menu entry produced a blank box — its 28-widget tree was over-scoped for the actual user need and never reached a renderable state. A redesigned Figma surface (file key `1C7rUBqhsEcu9tV8DO7zjY`, node `11:1303`) defines the target look: single-page list with file-type tabs across the top, one row per download (thumbnail + name + source + status badge + size + date), and a slim status bar at the bottom. The user wants this implemented **as an embedded surface in the main window**, not as a popup box, and the first deliverable should be a **demo page** — the visual layout rendered against hardcoded sample rows that match the Figma exactly, with tabs/search/sort wired against the sample data only. Wiring to `DownloadCenter` and bulk-action methods comes in a follow-up change.

## What Changes

- **Embedded Downloads surface** — a new `DownloadsContent` widget replaces the chat list + main content area when Downloads is selected (full-width, parallel to the Settings takeover). The surface is **not** a `Box`; it lives inside `MainWidget` and is swapped via a new section state. For this change, the swap is exposed through a single new method `SessionNavigation::showDownloads()` and the previous `Box` / `Ui::show` approach is abandoned.
- **Demo page first** — the embedded surface renders against a hardcoded list of 15 sample rows copied verbatim from the Figma (mix of file types and statuses, including 2 active, 1 paused, and the rest completed). Tab clicks, search input, sort cycling, and the footer status-bar count all operate on this sample data. No `DownloadCenter`, `rpl::producer`, or persistence code is touched.
- **Static visuals from the Figma** — header (title + `15 files · 2 active` subtitle + search box + `Sort: Date` button + view-toggle button group), 7-tab filter (`All / Photos / Videos / Files / Music / Links / Voice`), list body with the 15 sample rows, footer (`2 files downloading` + Pause all / Cancel all text buttons).
- **Rail entry with hardcoded badge** — a 40×40 round button in `MainMenu`'s navigation column (matching the Figma) opens the embedded surface. The badge shows a hardcoded `2` while the demo is mounted. The 5 Hz poll is added but reads from a static counter, not `computeStats()`.
- **No data-layer changes** in this change. `Data::DownloadCenter` is untouched. `DownloadState::Waiting` / `Removed` / `computeStats()` / `ChunkState` all stay where `493295aab0` left them — a follow-up change will consume them.

## Capabilities

### New Capabilities

- `downloads-list-view` — the new single-page list: header (title / subtitle / search / sort / view toggle), 7 file-type tabs with count badges, list rendering with thumbnail/name/source/status/size/date columns, footer status bar, empty-state placeholder. Operates against a hardcoded sample list of 15 rows. Row click, context menu, and bulk actions (Pause all / Cancel all) update the in-memory sample but do not call `DownloadCenter`.
- `downloads-entry` — the left-navigation-rail entry button: 40×40 round icon, blue background when the embedded surface is selected, a top-right circular badge showing the hardcoded active count. Click swaps the main-widget content to the embedded surface (no `Box`, no `Ui::show`).

### Modified Capabilities

- None. The data-layer capabilities (`download-task-states`, `download-task-persistence`) shipped in `493295aab0` are unchanged; this proposal only renders their visual target with sample data and will consume them in a follow-up change.

## Impact

- **New files** (UI layer, intentionally small):
  - `Telegram/SourceFiles/history/history_view_downloads_section.{h,cpp}` — `DownloadsSection` widget returned by `PrepareDownloadsSection(controller)`; owns the embedded layout, replaces the main-widget content, hosts `DownloadsContent`.
  - `Telegram/SourceFiles/ui/widgets/downloads/downloads_content.{h,cpp}` — assembles header + tabs + list + footer; holds the hardcoded `Sample::kDownloads` array of 15 rows and the active-tab / search / sort state.
  - `Telegram/SourceFiles/ui/widgets/downloads/downloads_row.{h,cpp}` — one row (thumbnail / name / source / status badge / size / date); consumes the sample struct.
  - `Telegram/SourceFiles/ui/widgets/downloads/downloads_status_badge.{h,cpp}` — the colored pill (Done / `<n>%` / Paused / Failed).
  - `Telegram/SourceFiles/ui/widgets/downloads/downloads_file_icon.{h,cpp}` — 44×44 colored chip with file-type abbreviation (PDF / MP3 / DOCX / OGG / ZIP / URL).
  - `Telegram/SourceFiles/ui/widgets/downloads/downloads_style.h` — color tokens (`downloadsBg`, `downloadsAccent`, `downloadsDone`, `downloadsPaused`, `downloadsActive`, `downloadsFailed`) and sizes.
- **Modified files**:
  - `Telegram/SourceFiles/window/window_main_menu.{h,cpp}` — re-add the Downloads rail entry; use a hardcoded `kDemoActiveCount = 2` for the badge (no `computeStats()` call yet).
  - `Telegram/SourceFiles/window/window_session_controller.{h,cpp}` — re-add `showDownloads()`; this version calls `MainWidget::showSection(Section::Downloads)` instead of `Ui::show(...)`.
  - `Telegram/SourceFiles/window/window_main_widget.{h,cpp}` — extend the section state to include `Section::Downloads`, and the content-swap logic to instantiate `HistoryView::DownloadsSection` for it.
  - `Telegram/SourceFiles/window/window_main_menu.h` — define `Section` (if not already) or extend it.
  - `Telegram/Resources/langs/lang.strings` — add the 12 strings actually rendered by the demo (`lng_menu_downloads`, `lng_downloads_title`, `lng_downloads_count`, tab labels, status labels, footer text, action labels). The ~34 strings from the rolled-back UI that this design does not render are dropped.
  - `Telegram/Resources/qrc/telegram/telegram.qrc` — re-add the 7 SVG aliases used by the new row / status bar (download-small, document, clock, warning, check, chevron-down, search).
  - `Telegram/SourceFiles/ui/menu_icons.style` — re-add `menuIconDownloads` (one line).
  - `Telegram/CMakeLists.txt` — register the ~5 new source files.
- **New content**:
  - `Telegram/Resources/icons/downloads/` — re-create only the 7 SVGs above (down from 21 in the old change).
- **Build impact**: ~5 new translation units, no new third-party dependencies, incremental compile on a warm tree ~10s.
- **Persistence impact**: none. The committed JSON format in `DownloadCenter` is untouched by this change.
- **Risk**: low. The embedded section must coexist with the chat list / settings takeover without leaking its widget when the user switches sections. The `DownloadsSection` widget's lifetime is bound to the section state and is destroyed when the user navigates away.