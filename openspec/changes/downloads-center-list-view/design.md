## Context

The previous download-center change (`download-center-ui`) shipped a 28-widget tree that compiled but rendered as a blank box on the user's machine. The rolled-back code path is gone; only the data-layer changes (`493295aab0`) remain in `feature/allow-saving-restricted-content`. The new Figma design at file key `1C7rUBqhsEcu9tV8DO7zjY` node `11:1303` reduces the surface to a single dark-themed list view.

The user has explicitly redirected this change to be **embedded, not a popup** — the Downloads surface replaces the main-widget content rather than opening a `Box`. They also want this change to deliver a **demo page**: the visual layout rendered against 15 hardcoded sample rows copied from the Figma, with tabs / search / sort operating in-memory only. Real `DownloadCenter` integration comes in a follow-up change. The committed data layer stays available for that follow-up; this change does not consume it.

Stakeholders: the user (single developer) drives the design and is the sole consumer. Constraints from `AGENTS.md`: CRLF endings on Windows, no `Co-Authored-By` trailer, use `auto`, no hardcoded sizes in `.cpp` (use `.style` references), UTF-8 without BOM.

## Goals / Non-Goals

**Goals**
- Render the Figma surface faithfully: 7-tab top filter, table-style row, colored status badge, footer status bar, all against hardcoded sample data.
- Embed the surface in `MainWidget` via a new `Section` value; the rail entry swaps the main content to the embedded section (no `Ui::show`, no modal box).
- Keep the new code surface small: ≤6 new translation units, one small `style.h`.
- Use sample data that matches the Figma row-for-row (same filenames, sizes, statuses, chat names, durations) so the demo looks identical to the design.

**Non-Goals**
- Sidebar (My Downloads / Downloading / Completed / Failed / Trash).
- Per-task detail panel.
- 60-second live speed chart.
- Trash. `DownloadState::Removed` is unused in this change.
- Multi-select. The footer has Pause/Cancel all buttons but they only update the in-memory sample.
- Wiring to `DownloadCenter`. No `computeStats()`, no `rpl::producer`, no `taskAdded` / `taskRemoved` / `taskChanged` subscriptions, no persistence reads, no badge polling against real data.
- Real pause / resume / retry / open / show / copy actions. The context menu exists in code but selecting an item only updates the in-memory sample (e.g., a "Pause" click transitions the sample row's `state` field from `Downloading` to `Paused` and the badge repaints).
- Search beyond filename substring match.

## Decisions

### D1. Embedded section, not a popup Box.

A new `Section::Downloads` value is added to `MainWidget`'s section enum. `MainWidget::showSection(Section)` swaps the chat list + main content for the `DownloadsSection` widget, parallel to how the existing `Section::Settings` already replaces the content with the settings tree. `SessionNavigation::showDownloads()` calls `MainWidget::showSection(Section::Downloads)` and caches nothing. The previous `Box` / `Ui::show(PrepareDownloadsBox(controller))` approach is abandoned.

**Why embedded, not Box**: the user explicitly requested it. Box-style popups float above the chat list and visually disconnect from the navigation rail; the Figma shows Downloads as a full-width page in the same window chrome. The embedded approach also lets us reuse the rail's `selected` state painting directly (the rail entry knows the section is open because the section tells it via `MainWidget`'s current `Section`).

**Alternative considered**: keep the Box approach and ignore the user's redirection. Rejected — the user's instruction is explicit and the embedded approach is closer to how Settings already works in the codebase.

### D2. Row rendering through `ListView` with a custom `DownloadsRow` delegate.

`DownloadsContent` owns a vertical `Ui::ListView` populated from a snapshot of the in-memory `std::vector<Sample::DownloadRow>` filtered by the active tab. Each row is a fixed-height custom-painted widget. The `ListView`'s selection model is single-row; clicks drive the context menu, not selection. This matches how the chat list and filters lists are already wired.

**Alternative considered**: `QListView` with `QStandardItemModel`. Rejected — tdesktop's other lists all use the custom `Ui::ListView` for snap behavior and theme-aware painting.

### D3. Status badge with four render states against the sample.

`DownloadsStatusBadge` is a small fixed-height pill that takes a `Sample::DownloadState` (an enum local to the demo, see D6) and the percent when relevant:

| Sample state | Pill |
|---|---|
| `Downloading` | blue background `rgba(42,171,238,0.15)`, blue text `"<n>%"` |
| `Completed` | green `rgba(39,174,96,0.15)`, text `"Done"` |
| `Paused` | orange `rgba(243,156,18,0.15)`, text `"Paused"` |
| `Failed` | red `rgba(231,76,60,0.15)`, text `"Failed"` |

`Waiting` and `Removed` are deliberately not rendered in the demo — they exist in `DownloadState` but the Figma's sample rows don't include them. A follow-up change will add the `Waiting` mapping and the `Removed` filter.

The `<n>%` value for `Downloading` is read from `sample.percent` (a fixed integer per row, e.g., `67`, `84`). The text uses `style_71ac8853` (Inter Regular 12px, centered). Badge height is `19px`.

### D4. Thumbnail: static image placeholder for image previews, colored chip otherwise.

For this demo, the thumbnail cell does **not** load real photos — it paints a `rgba(255,255,255,0.08)` rounded (8px) placeholder for any row whose sample `kind` is `Photo` or `Video`. When the sample `state` is `Downloading`, the cell additionally paints a `rgba(0,0,0,0.35)` overlay covering the placeholder and a centered white play-triangle SVG (a static inline path), mirroring the Figma's "active media" overlay. For non-media rows, `DownloadsFileIcon` paints a colored chip with the file-type abbreviation as described in the proposal.

**Why no real thumbnails**: the demo is about layout fidelity, not media rendering. Loading real thumbnails would require wiring `Data::PhotoData` and a thumbnail loader — exactly the data-layer integration this change defers. The dark placeholder is visually distinct from a missing image and matches the Figma's image-fill intent well enough for design review.

**Alternative considered**: bundle the Figma's sample images locally and load them as `QPixmap`s. Rejected — adds binary assets to the repo and pulls in the Figma node-id-by-id download pipeline (out of scope for this change).

### D5. Tab filter operates on the sample in memory.

`DownloadsContent` keeps a `std::vector<int> _visible` of indices into `Sample::kDownloads`. The filter is recomputed on:
- tab change (button group → single selected tab id)
- search text change (`QLineEdit::textChanged`)
- sort change (button cycle)
- sample mutation (a context-menu action or the Pause-all / Cancel-all bulk button mutates the in-memory row and triggers a re-filter).

The tab count badge is `Sample::kDownloads` filtered by the same predicate.

### D6. Hardcoded sample lives in `downloads_content.cpp` as `namespace Sample`.

```cpp
namespace Sample {
    enum class State { Downloading, Completed, Paused, Failed };
    enum class Kind { Photo, Video, Audio, Voice, Document, Archive, Link };

    struct Row {
        QString fileName;     // "Q3_Financial_Report_2026.pdf"
        QString chatName;     // "Finance Dept"  (rendered blue + bold)
        QString context;      // "Company General" or "Product Launch · 4:22"
        Kind    kind;
        State   state;
        int     percent = 100; // for Downloading rows
        QString sizeText;    // "8.4 MB"
        QString dateText;    // "Today, 14:32"
    };

    inline const std::vector<Row> kDownloads = {
        // 15 entries copied verbatim from Figma node 11:1303
        // mix: 2 Downloading, 1 Paused, 12 Completed
        // kinds: Photo (5), Video (3), Document (PDF/DOCX, 4), Audio (2), Voice (1), Link (1), Archive (1)
    };
} // namespace Sample
```

The 15 entries match the Figma: filenames, chat names, sizes, dates, and durations are copied verbatim from the design. A follow-up change replaces this namespace with a `Data::DownloadCenter`-backed data source.

### D7. Footer status bar shows a hardcoded count and bulk-mutates the sample.

The footer reads `Sample::kActiveCount()` (a function returning the count of rows whose `state == Downloading`) and renders the localized `N files downloading` plus a green 8×8 dot. The `Pause all` and `Cancel all` buttons mutate the sample: `Pause all` sets every `Downloading` row's `state` to `Paused`; `Cancel all` sets every `Downloading` row's `state` to `Completed` (a stand-in — the demo has no `Cancelled` state) and the rows stay visible. After mutation, the list re-filters and the footer count updates. No `DownloadCenter::pauseAll()` / `cancelAll()` is called.

### D8. New `downloads_style.h` with color tokens.

All new colors live in `ui/widgets/downloads/downloads_style.h` as a `namespace DownloadsStyle` (matching the local `st::` pattern). Tokens match the Figma exactly:

```cpp
inline constexpr QColor bg          = QColor(0x1F, 0x18, 0x18);
inline constexpr QColor rowBg       = QColor(0, 0, 0, 0);
inline constexpr QColor rowBorder   = QColor(255, 255, 255, 15);
inline constexpr QColor accent      = QColor(0x2A, 0xAB, 0xEE);
inline constexpr QColor textFg      = QColor(0xFF, 0xFF, 0xFF);
inline constexpr QColor textDim     = QColor(0xFF, 0xFF, 0xFF, 140);
inline constexpr QColor textMuted   = QColor(0xFF, 0xFF, 255, 90);
inline constexpr QColor doneFg      = QColor(0x27, 0xAE, 0x60);
inline constexpr QColor doneBg      = QColor(0x27, 0xAE, 0x60, 38);
inline constexpr QColor pausedFg    = QColor(0xF3, 0x9C, 0x12);
inline constexpr QColor pausedBg    = QColor(0xF3, 0x9C, 0x12, 38);
inline constexpr QColor activeFg    = QColor(0x2A, 0xAB, 0xEE);
inline constexpr QColor activeBg    = QColor(0x2A, 0xAB, 0xEE, 38);
inline constexpr QColor failedFg    = QColor(0xE7, 0x4C, 0x3C);
inline constexpr QColor failedBg    = QColor(0xE7, 0x4C, 0x3C, 38);
inline constexpr QColor navActive   = QColor(0x2A, 0xAB, 0xEE, 51);
inline constexpr QColor progressFill= QColor(0x2A, 0xAB, 0xEE);
inline constexpr QColor progressFillWarn = QColor(0xF3, 0x9C, 0x12);
```

Sizes (header height 56, tab height 44, row height 70, footer height 36, thumbnail 44×44, padding 12/16/20) live in a new `ui/downloads_icons.style` file (the rolled-back version of that file is gone). No hardcoded pixel values inside `.cpp`.

### D9. Rail entry reads a hardcoded active count.

`MainMenu::setupMenu()` re-adds the `addAction(tr::lng_menu_downloads(), {&st::menuIconDownloads})` block. The 5 Hz `_downloadsBadgeTimer` polls a `MainMenu::kDemoActiveCount = 2` constant (a `static constexpr int` on `MainMenu`), calls `update()` only when the value changes. The click handler calls `controller->showDownloads()`. The entry is hidden when `controller->session().supportMode()` is true.

The badge is a small blue dot (`14×14` round, painted at `(22, 4)` inside the icon) when `kDemoActiveCount > 0`, containing the integer in white if `count < 100`. The dot color and size match the Figma's `2` digit exactly.

**Alternative considered**: make the badge driven by `computeStats().activeCount` since the data layer is already there. Rejected — the user explicitly wants this change to be a demo. Pulling in `computeStats()` would couple the demo to a real download mechanism and defeat the point of having a static, reviewable demo page.

### D10. The `Section` swap lives in `MainWidget`.

`MainWidget` already has a section enum driving content (chat / archive / settings). `DownloadsSection` slots in as a new value. The `MainWidget::showSection(...)` method (or the equivalent setter) takes the new value, calls `destroyOldContent()`, and constructs the new widget in place of the chat list + main view. The `DownloadsSection` is built with a pointer to `SessionController` so it can later call back for download actions; for the demo, that pointer is unused.

## Risks / Trade-offs

- **[Risk]** The embedded section replaces the chat list, so opening Downloads disconnects the user from any open chat. **Mitigation**: the rail entry's selected state makes the navigation reversible — clicking any other nav button swaps back. The user explicitly wants embedded, so this is accepted.
- **[Risk]** The sample data is hardcoded; if the Figma changes (e.g., a row is added), the demo must be edited by hand. **Mitigation**: keep the sample as a small static array in one `.cpp`; the diff is local and easy to review. A follow-up change will replace it with a live data source.
- **[Risk]** Buttons labeled `Pause all` / `Cancel all` only mutate the sample, so the demo looks interactive but the actions don't survive a restart or section swap. **Mitigation**: on `MainWidget::showSection(Section::Chat)` (i.e., navigating away from Downloads), the sample is reset to its hardcoded initial state. The demo's reset behavior is intentional — it keeps the design review deterministic.
- **[Risk]** The rail entry's badge poll is 5 Hz even though the value never changes. **Mitigation**: same dirty-check guard as the rolled-back version (`if (count != _lastDownloadsBadgeCount) update()`). With a `constexpr` source, the value never changes after construction, so `update()` is called at most once.
- **[Trade-off]** `MainWidget`'s section enum grows by one. **Trade-off accepted**: this is a normal pattern in the codebase (`Section::Settings`, `Section::Archive` already coexist).
- **[Trade-off]** The demo does not exercise any of the data-layer code paths committed in `493295aab0`. **Trade-off accepted**: that's the explicit point of the user's redirection. A follow-up change will replace the sample with `DownloadCenter`-driven data.

## Migration Plan

The change is additive. No data migration. The user can navigate between Chat and Downloads via the rail; both sections are stateless w.r.t. each other.

Rollback: revert the single UI commit (this change's commit). The committed data-layer changes in `493295aab0` stay. `MainWidget`'s section enum loses the `Downloads` value, restoring the previous binary.

## Open Questions

- **Q1.** Should `DownloadsSection` accept a `SessionController*` now (for later use) or take it later when wired up? I lean toward taking it now to keep the API stable when the follow-up change wires real data — the pointer is unused in this change but the construction site is set.
- **Q2.** Should the sort cycle be `Date → Size → Name → Date` (3 keys) or just `Date → Size → Date` (2 keys)? The Figma button label is `Sort: Date`. I lean toward 3 keys for parity with the rolled-back `lng_downloads_sort_*` keys we kept in the strings file.
- **Q3.** The Figma's `All / Photos / Videos / Files / Music / Links / Voice` tab order — should the sample rows actually be filterable into 7 non-overlapping buckets, or do we collapse `Links` and `Voice` into a single "Other" bucket because the sample has only 1 of each? The Figma is explicit about 7 tabs; the sample data is too sparse to show meaningful counts in 4 of them. **Decision: keep 7 tabs but expect several to show count `1` or `0`.** This matches the Figma and surfaces the design as-is.
- **Q4.** When the section is embedded, do we hide the chat list (full-width takeover) or keep it visible (split layout)? The Figma is 1549px wide with no chat list column — full-width takeover. The sample rows also assume this width. **Decision: full-width takeover; the chat list column is hidden when `Section::Downloads` is active.**