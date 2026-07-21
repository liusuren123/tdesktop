## 1. Foundation

- [ ] 1.1 Add `ui/downloads_icons.style` with the size tokens used by the demo (header 56, tab 44, row 70, footer 36, thumbnail 44, paddings 12/16/20, badge 19, progress 3). Declare `menuIconDownloads` here too so `menu_icons.style` doesn't need to change.
- [ ] 1.2 Add `ui/widgets/downloads/downloads_style.h` with the `namespace DownloadsStyle` color tokens (`bg`, `rowBg`, `rowBorder`, `accent`, `textFg`, `textDim`, `textMuted`, `doneFg/Bg`, `pausedFg/Bg`, `activeFg/Bg`, `failedFg/Bg`, `navActive`, `progressFill`, `progressFillWarn`).
- [ ] 1.3 Re-create `Resources/icons/downloads/` with the 7 SVG files needed for the demo: `download-small.svg`, `document.svg`, `clock.svg`, `warning.svg`, `check.svg`, `chevron-down.svg`, `search.svg`. Use simple inline paths (16×16 viewBox) — these are placeholders that match the Figma's glyph shape enough for the demo.

## 2. Leaf widgets

- [ ] 2.1 Implement `ui/widgets/downloads/downloads_file_icon.{h,cpp}`. `DownloadsFileIcon(QWidget*)` paints a 44×44 rounded (8px) chip. Public method `setKind(Sample::Kind, const QString &fileName)` selects the abbreviation + color. For `Photo`/`Video`, paint the dark `rgba(255,255,255,0.08)` placeholder; add the play overlay when an external flag is set (call site passes it).
- [ ] 2.2 Implement `ui/widgets/downloads/downloads_status_badge.{h,cpp}`. `DownloadsStatusBadge(QWidget*)` paints a 19px tall pill. Public method `setState(Sample::State, int percent)`. Center the text (`Inter Regular 12px`). Pill background and text colors come from `DownloadsStyle`.

## 3. Row

- [ ] 3.1 Implement `ui/widgets/downloads/downloads_row.{h,cpp}`. `DownloadsRow(QWidget*)` is a fixed-height (70 + 12 outer padding) widget. Holds: `DownloadsFileIcon` (44×44, left), a column with two `QLabel`s (filename + source line), `DownloadsStatusBadge`, a 70×70 right-aligned size label (`JetBrains Mono 11px`), a 96×96 right-aligned date label (`Inter Regular 11px`). Public method `setSample(const Sample::Row &)`. Override `paintEvent` to draw the 3px progress bar above the badge when `state == Downloading`. Override `contextMenuEvent` to show a `Ui::PopupMenu` with state-appropriate actions; emit signals back to `DownloadsContent` for each action.
- [ ] 3.2 Implement the play-triangle SVG path as a small inline helper in `downloads_row.cpp` (or in `downloads_file_icon.cpp` if cleaner). It's a simple equilateral triangle, 9×9, painted white at 90% alpha.

## 4. Container

- [ ] 4.1 Implement `ui/widgets/downloads/downloads_content.h` declaring `DownloadsContent(QWidget*, not_null<SessionController*> controller)`. Holds the sample (`std::vector<Sample::Row>`), a `Ui::ListView*`, a header row (title + subtitle + search `QLineEdit` + sort button + view toggle), a tab bar (7 buttons in a single row), and a footer row (dot + count + Pause all / Cancel all).
- [ ] 4.2 Implement `Sample` namespace in `downloads_content.cpp` with the 15 hardcoded rows copied verbatim from the Figma node `11:1303`. Include all required fields (`fileName`, `chatName`, `context`, `kind`, `state`, `percent`, `sizeText`, `dateText`). Match the Figma rows exactly (filenames, chat names, sizes, dates, durations). Helper `kActiveCount()` returns the count of `Downloading` rows.
- [ ] 4.3 Implement the header layout. Title is a `QLabel` with `Inter Semi Bold 16px`; subtitle is a `QLabel` with `Inter Regular 11px` updated whenever the sample's counts change. Search input is a 240×33.5 `QLineEdit` with placeholder text from `lng_downloads_search_placeholder`. Sort button cycles keys (Date → Size → Name → Date) and updates its label. View toggle is a 64×32 button group (two icons, list/grid).
- [ ] 4.4 Implement the tab bar as 7 `QPushButton`s (or a single `ButtonGroup`) in a 44px row. Active tab gets `DownloadsStyle::accent` text + badge fill; inactive tabs get white text on translucent white badge. Tab clicks update `_activeTab` and trigger `_visible` rebuild.
- [ ] 4.5 Implement the `Ui::ListView` body. `setItemsCount` is `static_cast<int>(_visible.size())`. The list delegate (a small adapter class inside the `.cpp`) returns a `DownloadsRow*` for each visible index and rebinds `setSample` whenever the underlying sample row mutates. Connect `clicked` and `contextMenu` signals from the delegate to the content's handlers.
- [ ] 4.6 Implement the footer. Dot is a small 8×8 round `QLabel` (paint in `paintEvent`). Count label is `Inter Regular 12px` driven by `Sample::kActiveCount()`. Pause all and Cancel all are `QPushButton`s with text-only rendering. Connect their `clicked` to mutate the sample (`Downloading → Paused` and `Downloading → Completed` respectively), then trigger a list rebuild and footer count update.
- [ ] 4.7 Implement context-menu handlers. For each row state, build a `Ui::PopupMenu` with the actions listed in the spec. Wire each action's `clicked` to mutate the sample row's `state` field and trigger a list rebuild. `Remove` deletes the row from the sample.
- [ ] 4.8 Implement the empty-state placeholder. A simple centered `QLabel` (title + subtitle, `Inter Medium 14px` + `Inter Regular 11px`) shown when `_visible.empty()`. The footer still renders in empty state.

## 5. Embedding

- [ ] 5.1 Add `Section::Downloads` to `MainWidget`'s section enum (check the existing enum in `window/window_main_widget.h`; add the new value next to `Section::Settings` or wherever matches the existing pattern).
- [ ] 5.2 Implement `history/history_view_downloads_section.{h,cpp}`. `DownloadsSection(QWidget*, not_null<SessionController*> controller)` owns a `DownloadsContent` and lays it out to fill the section area. Public factory `PrepareDownloadsSection(controller)` returns `std::unique_ptr<DownloadsSection>`. Provide a method `setActive(bool)` so the parent `MainWidget` can pause/resume polling or animations if needed (no-op for this change but the API is set).
- [ ] 5.3 Wire `MainWidget::showSection(Section)` to instantiate `DownloadsSection` for `Section::Downloads` and destroy the previous content. Reuse the existing destroy/replace pattern used for `Section::Settings`.
- [ ] 5.4 In `window/window_session_controller.{h,cpp}`, add `void showDownloads()` that calls `MainWidget::showSection(MainWidget::Section::Downloads)`. No `Ui::show`, no `BoxPointer`.

## 6. Rail entry

- [ ] 6.1 In `window/window_main_menu.h`, declare `static constexpr int kDemoActiveCount = 2;` and add `base::Timer _downloadsBadgeTimer; int _lastDownloadsBadgeCount = 0;` as members.
- [ ] 6.2 In `window/window_main_menu.cpp`, in `setupMenu()` after the night-mode block (~line 784), add the Downloads entry guarded by `!controller->session().supportMode()`:
  ```cpp
  auto button = addAction(tr::lng_menu_downloads(), { &st::menuIconDownloads });
  button->setClickedCallback([=] { controller->showDownloads(); });
  _downloadsBadgeTimer.callEach(200);
  _downloadsBadgeTimer.setCallback([=] {
      if (kDemoActiveCount != _lastDownloadsBadgeCount) {
          _lastDownloadsBadgeCount = kDemoActiveCount;
          update();
      }
  });
  ```
- [ ] 6.3 In `MainMenu::paintEvent` (or wherever the icon-button background is drawn), paint the active-state background `rgba(42,171,238,0.20)` when `MainWidget::isSectionActive(Section::Downloads)`. Add the `isSectionActive` accessor to `MainWidget` if it doesn't already exist.
- [ ] 6.4 In the same paint path, when `kDemoActiveCount > 0`, paint the 14×14 badge at `(22, 4)` (relative to the icon) with `DownloadsStyle::accent` fill and white text (`kDemoActiveCount` as integer, `JetBrains Mono Bold 8px`, centered).

## 7. Build integration

- [ ] 7.1 In `Resources/qrc/telegram/telegram.qrc`, add 7 `<file>` aliases inside the `/qresource prefix="/art"` block: `downloads/download-small.svg`, `downloads/document.svg`, `downloads/clock.svg`, `downloads/warning.svg`, `downloads/check.svg`, `downloads/chevron-down.svg`, `downloads/search.svg`.
- [ ] 7.2 In `Telegram/CMakeLists.txt`, register the new sources under the appropriate `PRIVATE` blocks:
  - `history/history_view_downloads_section.cpp` and `.h` next to the other `history_view_*` entries (~line 1127 area)
  - `ui/widgets/downloads/downloads_content.{cpp,h}`
  - `ui/widgets/downloads/downloads_row.{cpp,h}`
  - `ui/widgets/downloads/downloads_status_badge.{cpp,h}`
  - `ui/widgets/downloads/downloads_file_icon.{cpp,h}`
- [ ] 7.3 In `Resources/langs/lang.strings`, re-add only the keys the demo renders: `lng_menu_downloads`, `lng_downloads_title`, `lng_downloads_count` (with `{count}` placeholder), the 7 tab labels, `lng_downloads_status_done/active/paused/failed`, `lng_downloads_files_downloading` (with `{count}`), `lng_downloads_action_pause_all`, `lng_downloads_action_cancel_all`, `lng_downloads_empty_no_tasks`, `lng_downloads_empty_no_tasks_desc`, `lng_downloads_search_placeholder`. Keep the existing `lng_downloads_sort_*` keys since the sort cycle reuses them.
- [ ] 7.4 Verify no file in the new surface hardcodes pixel values — sizes must come from `st::` (referencing the new `downloads_icons.style`) and colors from `DownloadsStyle::`.

## 8. Smoke test

- [ ] 8.1 Build the project: `cmake --build out --config Debug --target Telegram`. Resolve any compile errors. If build fails with PDB/EXE access errors per `AGENTS.md`, **stop and ask the user** before retrying.
- [ ] 8.2 Launch the app, click the Downloads rail entry. Verify the embedded surface appears with: title, `15 files · 2 active` subtitle, search box, sort button, view toggle, 7 tabs with `All` highlighted, 15 sample rows, footer with `2 files downloading` and Pause all / Cancel all.
- [ ] 8.3 Click each tab in turn. Verify the list filters correctly (Photos → 5, Videos → 3, Files → 3, Music → 2, Links → 1, Voice → 1; counts may differ from Figma's labels if the sample is interpreted slightly differently — adjust the sample if needed).
- [ ] 8.4 Type `pdf` in search. Verify list narrows. Clear search. Click sort button 3 times; verify label cycles Date → Size → Name → Date.
- [ ] 8.5 Right-click a `Downloading` row, select `Pause`. Verify badge turns orange, progress bar disappears, footer count drops.
- [ ] 8.6 Click `Pause all`. Verify both `Downloading` rows turn `Paused`, footer count drops to `0`. Click `Cancel all` while no rows are `Downloading` — should be a no-op (no `Downloading` to cancel).
- [ ] 8.7 Click the chat rail entry, then click Downloads again. Verify sample resets to initial state (both rows are `Downloading` again).
- [ ] 8.8 Verify the rail entry's badge shows `2` (blue circle with white digit) on the Downloads icon while the embedded surface is open, and the icon background turns translucent blue.