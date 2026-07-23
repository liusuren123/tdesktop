## Context

Telegram Desktop has long shipped a transient "psh"-style progress overlay for downloads, but no persistent UI for managing them. The download engine underneath is mature: `Storage::ParallelDownloadController` performs 4-way chunked MTProto downloads, `Storage::DownloadManagerMtproto` schedules requests across sessions, and `Data::DownloadCenter` already aggregates per-account state and persists it to JSON. The missing surface is a real window.

The design source is `designs/download_center_v2.free` (live Lunacy document) plus the `designs/_v3_top_{0..3}_*.json` snapshot files (which contain the full intended layout). A consolidated `designs/OPENSPEC_PROPOSAL.md` captures the visual / data / interaction / style / i18n contracts.

The entry point is a new menu row in the main window's left sidebar, immediately after the night-mode toggle, parallel to the existing "设置" entry. The implementation must integrate without disturbing any of the existing download engine code paths.

## Goals / Non-Goals

**Goals:**
- Ship a 1280×800 dark-themed download center window that renders status, controls, and per-task details without modifying any download engine code.
- Add a sidebar entry that opens the window with a live active-count badge.
- Extend `DownloadTask` with two new statuses (`Waiting`, `Removed`) and three new fields (`sha256`, `startedAt`, `speedHistory60s`), plus a `DownloadStats` aggregate, while keeping the JSON format backward-compatible.
- Persist the new state and fields atomically.
- Hit the performance budgets in `download-center-ui/spec.md` (60 fps at 1000 rows, ≤5 Hz status updates).

**Non-Goals:**
- Grid view (icons reserved, list only in v1).
- Light theme (dark theme tokens are still extracted so a follow-up can implement it; v1 only renders dark).
- Cloud sync of download history.
- Custom keyboard shortcuts (deferred).
- Reorganising the existing chat list or sidebar layout.

## Decisions

### 1. New window hosted by `Window::SessionController`, not a `Box<ContentMemento>`

The download center is a top-level window, not a content memento inside the existing chat history stack. Rationale: a download center is conceptually orthogonal to chat navigation — keeping it as a Box means the user can navigate to chats via the back button, which is wrong. A dedicated window also makes it easier to keep the same instance open while the user peeks at a chat and returns.

A new method `Window::SessionController::showDownloads()` is added parallel to `showSettings()`. The window is created lazily on first entry and reused for subsequent entries (the existing pattern used by `showSettings` / `showArchive`).

**Alternative considered**: a Box inside the existing content stack with its own "back" button. Rejected: produces a confusing back-history interaction, and the window model is what `Settings` already uses.

### 2. Sidebar entry via the same `addAction` mechanism as the other menu rows

The new entry reuses `addAction(text, icon)->setClickedCallback(...)` exactly like the "设置" row at `window_main_menu.cpp:740`. No bespoke layout code, no custom widget. The `_unreadBadge` mechanism is reused to render the active-count badge (the same mechanism `ToggleAccountsButton::validateUnreadBadge` uses, line 233).

**Alternative considered**: a custom widget subclassing `Ui::RippleButton`. Rejected: the menu has a tight visual contract that custom widgets tend to break; `addAction` is what every other sidebar row uses.

### 3. Status enums: extend, do not split

Add `Waiting` and `Removed` to the existing `DownloadTask::Status` enum rather than introducing a parallel "soft delete" flag. Rationale: the UI treats these as first-class filters (回收站 is its own sidebar item), and the persistence layer needs them to be first-class JSON values, not optional flags.

**Alternative considered**: a `bool removed` and `bool waiting` alongside the existing `Status`. Rejected: doubles the state combinations and complicates the JSON schema.

### 4. Speed history is a 60-sample ring inside `DownloadTask`

A `std::deque<int64>` capped at 60 entries, written once per second by a `crl::timer` on the main thread. Persistence is a JSON array of integer bytes-per-second. 60 samples is enough for the visible chart window, small enough to serialise cheaply.

**Alternative considered**: storing the ring outside `DownloadTask` keyed by ID. Rejected: the lifetime of a task is the lifetime of its history; co-locating the data keeps the controller simpler.

### 5. `DownloadStats` is a snapshot, not a stream

A struct built on demand from the current `DownloadCenter` task list. No caching, no incremental updates. The status bar queries it on a 5 Hz timer, which is well within the cost of iterating a few hundred tasks. The sidebar badge queries it on a 5 Hz timer too, to match the visual update rate of the badge.

**Alternative considered**: an `rpl::producer<DownloadStats>` that fires on every change. Rejected: change events come from too many sources (status changes, per-chunk progress, file additions); computing a snapshot on a timer is cheaper and simpler.

### 6. Backward-compat persistence via missing-field detection

The JSON loader checks each new field for presence and defaults to empty / zero when missing. No version field is added; the cost of adding one to support rolling back to an old build is not justified, and a downgrade simply loses the new fields without crashing.

**Alternative considered**: bump a `version` int in the JSON and migrate on load. Rejected: a forward downgrade is the only scenario it solves, and the user can re-trigger downloads from the chat that already knows the source.

### 7. Performance: list view with `QWidget`, ≥1000 rows switches to a virtualised list

A plain `QWidget`-based vertical layout is used for ≤1000 tasks because the layout cost is dominated by the number of currently *visible* rows (10-30), not the total. Above 1000 tasks, the list switches to the existing `HistoryView` virtualised list used by chat history.

**Alternative considered**: a `QListView` model from day one. Rejected: 1000 rows is rare in practice (most users have tens of downloads); the switching cost is low because the same `DownloadTaskRow` widget is reused as the delegate.

### 8. i18n via 46 new keys in `lang/strings.txt`

All new visible strings (window title, tab labels, action button labels, dialog body text, status bar copy) are registered as `lng_*` keys. The sidebar entry uses `lng_menu_downloads` for parity with the existing `lng_menu_settings`.

**Alternative considered**: hardcoding English strings and shipping them. Rejected: this codebase enforces localised UI in review; new strings are expected to be registered.

### 9. Icons: register 21 new SVG paths under `Telegram/Resources/icons/downloads/`

The user has supplied the SVGs (or has indicated they will be supplied). They live in a new directory under `Resources/icons/downloads/` and are registered through the existing `qrc` mechanism. The `designs/_icons.json` manifest maps each SVG to an Icons8-style `iconId` and a `webp` thumbnail. For the sidebar entry, the existing Icons8 icon `20FjgTazh8FG` is reused and registered as `st::menuIconDownloads` to match the menu-icon style.

**Alternative considered**: keep using pattern-fill webp images from the existing design system. Rejected: the codebase moves toward SVG-native icons and the new icons are SVGs by construction.

### 10. Failure handling: per-task error log, plus a single failure dialog

Each task carries a `QStringList errors` (already in `DownloadTask`). The failure dialog binds to a specific task at the moment of failure; it does not aggregate across tasks. This keeps the message short and the action clear ("重试 this task").

**Alternative considered**: an error log window with every failure ever. Rejected: out of scope for v1.

## Risks / Trade-offs

- **[Risk]** New `Waiting` and `Removed` states are loaded as `Downloading` and `Failed` respectively by older clients, hiding them from the user. → **Mitigation**: document the behavior in the upgrade notes. The `DownloadCenter` is per-account, and an old build can at worst cause the user to think a download is in progress when it is actually waiting.

- **[Risk]** Backward-compat loader silently drops new fields when downgrading to an old build. → **Mitigation**: the on-disk JSON is still readable by old builds because missing fields are treated as defaults; the only data loss is the speed-history ring, which is regenerable.

- **[Risk]** Status bar 5 Hz updates with 1000 tasks can become expensive on slow CPUs. → **Mitigation**: the snapshot is built in O(n) over the task list with no per-task work beyond reading atomic fields. On a 1 GHz Atom with 1000 tasks the iteration cost is sub-millisecond. The 5 Hz rate is conservative; a 2 Hz fallback kicks in if frame time exceeds 8 ms.

- **[Risk]** Adding an `addAction` in `MainMenu::MainMenu()` at the very end of the ctor risks an off-by-one if the night-mode block is later refactored. → **Mitigation**: insert immediately before the closing `}` of the ctor at line 785, and add a one-line comment marking the insertion point. Code review catches regressions.

- **[Risk]** `HistoryViewDownloads` depends on `Window::SessionController` lifetime; if the controller is destroyed mid-window we get a dangling reference. → **Mitigation**: the window holds a `QPointer` to the controller, and the controller holds a `base::weak_ptr` to the window. Both directions are weak.

- **[Risk]** Icon resource path conflicts: adding 21 SVGs under `Telegram/Resources/icons/downloads/` could collide with existing icons. → **Mitigation**: choose a directory name unique enough that no existing path overlaps (`downloads/` does not exist today).

- **[Risk]** Lint and clang-tidy may flag the new `_unreadBadge` interaction because the menu's `validateUnreadBadge` is currently only called for the accounts toggle. → **Mitigation**: factor a `MenuBadge` helper that both the existing accounts toggle and the new downloads row can use, and route both through it.

## Migration Plan

This change does not require a coordinated migration. The rollout is a single commit (or a small set of commits if the team prefers to split the work):

1. Land the data-layer changes (`Status` extension, new fields, `DownloadStats`, JSON loader changes). No UI is touched.
2. Land the new module files (`history_view_downloads*.{h,cpp}` and the `ui/widgets/downloads/` set). The window is not reachable from the UI yet.
3. Land the sidebar entry in `MainMenu`. This makes the window reachable.
4. Land the icon resource registration in `qrc` and the icon style in `style_menu_icons`.

Rollback: revert commits in reverse order. The JSON format is additive, so the data-layer change can be reverted without corrupting existing files (the new fields are simply ignored by older builds).

User-visible behavior change: none on the wire format, none on the engine. A user with an existing build will see no difference until they新 commits ship; then they will see a new sidebar row.

## Open Questions

- **Light theme**: should the dark theme be the only theme in v1, or should we ship light-theme tokens that are not yet wired? Current plan: ship tokens, render dark only.
- **Custom sort order via drag**: mentioned in OPENSPEC_PROPOSAL.md §3.3, not yet specced. Defer to a follow-up change.
- **Keyboard shortcuts**: the proposed list (`Space`, `Delete`, `Enter`, `Ctrl+A`, `Esc`, `F5`) is in OPENSPEC_PROPOSAL.md §9.5. None are specced here. Defer.
- **Toast position**: bottom-right is the conventional choice but the screenshot-based design does not specify it. Decide during implementation, do not block this change.
- **Sidebar entry visibility in support mode**: the spec requires hiding the entry in support mode (`controller->session().supportMode()` is true). Confirm with the user that this is the desired behavior.
