## ADDED Requirements

### Requirement: Embedded Downloads surface opens via `SessionNavigation::showDownloads()`

The system MUST provide a `SessionNavigation::showDownloads()` method. Calling it MUST call `MainWidget::showSection(MainWidget::Section::Downloads)` so that the chat list + main content area are replaced by the embedded `DownloadsSection` widget. The system MUST NOT use `Ui::show(...)` or a `Box` for this surface. The previously cached `BoxPointer` pattern from the rolled-back change MUST NOT be reintroduced.

#### Scenario: First entry swaps the main content
- **WHEN** the user clicks the Downloads rail entry while `Section::Downloads` is not active
- **THEN** `showDownloads()` calls `MainWidget::showSection(MainWidget::Section::Downloads)`; the chat list and main view are destroyed and `DownloadsSection` is constructed in their place

#### Scenario: Subsequent entry is a no-op
- **WHEN** the user clicks the Downloads rail entry while `Section::Downloads` is already active
- **THEN** `showDownloads()` MUST be a no-op (no rebuild, no flicker)

#### Scenario: Navigating away resets the sample
- **WHEN** the user activates a different section (e.g., `Section::Chat`)
- **THEN** the embedded `DownloadsSection` is destroyed; re-entering `Section::Downloads` reconstructs the section with the hardcoded sample reset to its initial state

### Requirement: Header shows title, subtitle, search, sort, and view toggle

The embedded surface's top header MUST render (left to right) the title "Downloads" (`Inter Semi Bold 16px`), a subtitle of the form `N files · M active` (`Inter Regular 11px`) where `N` and `M` are computed from the hardcoded sample, a search input (`240×33.5px`, placeholder `Search downloads…`), a sort button labeled `Sort: <key>` (`30px` high), and a view-toggle button group (`64×32`, list / grid icons). Header height MUST be `56px`. Left padding MUST be `20px`. The header MUST stretch to the full row width.

#### Scenario: Subtitle reflects sample counts
- **WHEN** the sample contains 15 rows with 2 in `Downloading` state
- **THEN** the subtitle renders `15 files · 2 active`

#### Scenario: Search input filters the sample on each keystroke
- **WHEN** the user types `pdf` into the search input
- **THEN** the visible list reduces to sample rows whose `fileName` contains `pdf` (case-insensitive)

#### Scenario: Sort button cycles through keys
- **WHEN** the user clicks the `Sort: Date` button
- **THEN** the sort key advances `Date → Size → Name → Date` and the visible sample re-sorts accordingly; the button label updates to reflect the new key

### Requirement: Seven file-type tabs filter the visible sample

The embedded surface MUST render a tab bar of seven tabs in this exact order: `All`, `Photos`, `Videos`, `Files`, `Music`, `Links`, `Voice`. Each tab MUST display a file-type icon, a label, and a circular count badge. The active tab MUST use Telegram blue (`#2AABEE`) for the label and badge; inactive tabs MUST use white text on a translucent white badge. Tab height MUST be `44px`, padding `0 16px`.

The tab's count badge MUST equal the number of sample rows that pass the tab's filter. The `All` tab MUST count every sample row.

#### Scenario: All tab is selected by default
- **WHEN** the embedded surface first opens
- **THEN** the `All` tab is highlighted and the list shows all 15 sample rows

#### Scenario: Photos tab filters to photo downloads
- **WHEN** the user clicks `Photos`
- **THEN** the list reduces to sample rows whose `kind` is `Photo`, and the badge count matches

#### Scenario: Videos tab filters to video downloads
- **WHEN** the user clicks `Videos`
- **THEN** the list reduces to sample rows whose `kind` is `Video`

#### Scenario: Files tab excludes audio and video
- **WHEN** the user clicks `Files`
- **THEN** the list reduces to sample rows whose `kind` is `Document` or `Archive`

#### Scenario: Music tab filters to audio downloads
- **WHEN** the user clicks `Music`
- **THEN** the list reduces to sample rows whose `kind` is `Audio`

#### Scenario: Voice tab filters to voice messages
- **WHEN** the user clicks `Voice`
- **THEN** the list reduces to sample rows whose `kind` is `Voice`

#### Scenario: Links tab filters to link tasks
- **WHEN** the user clicks `Links`
- **THEN** the list reduces to sample rows whose `kind` is `Link`

### Requirement: List rows show thumbnail, name, source, status badge, size, and date

Each row MUST render, in this order: a 44×44 thumbnail cell, a flexible content column (filename + source line), a status badge cell, a 70×70 right-aligned size cell (`JetBrains Mono 11px`), and a 96×96 right-aligned date cell (`Inter Regular 11px`). Row height MUST be 70px with 12px vertical padding. Rows MUST be horizontally padded by 16px and MUST have a 1px translucent bottom border.

#### Scenario: Filename and source line render correctly
- **WHEN** a sample row has `fileName = "Q3_Financial_Report_2026.pdf"`, `chatName = "Finance Dept"`, `context = "Company General"`
- **THEN** the row's name cell renders `Q3_Financial_Report_2026.pdf` in `Inter Medium 13px`, and the source line renders `from Finance Dept · Company General` with `Finance Dept` in Telegram blue and Inter Medium 13px

#### Scenario: Size and date are right-aligned in monospace
- **WHEN** a sample row has `sizeText = "8.4 MB"` and `dateText = "Today, 09:47"`
- **THEN** the size cell renders `8.4 MB` in `JetBrains Mono 11px` and the date cell renders `Today, 09:47` in `Inter Regular 11px`, both right-aligned

### Requirement: Status badge renders four states against the sample

The status badge MUST render one of four visual states driven by `Sample::State`:

| Sample state | Background | Foreground | Text |
|---|---|---|---|
| `Downloading` | `rgba(42,171,238,0.15)` | `#2AABEE` | `"<percent>%"` |
| `Completed` | `rgba(39,174,96,0.15)` | `#27AE60` | `"Done"` |
| `Paused` | `rgba(243,156,18,0.15)` | `#F39C12` | `"Paused"` |
| `Failed` | `rgba(231,76,60,0.15)` | `#E74C3C` | `"Failed"` |

For `Downloading`, the percent is read from the sample row's `percent` field (e.g., `67`, `84`). For other states, the text is fixed. Badge height MUST be `19px`.

#### Scenario: Active download shows blue percent badge
- **WHEN** a sample row has `state = Downloading` and `percent = 67`
- **THEN** the row's badge renders a blue-tinted pill with the text `67%`

#### Scenario: Completed row shows green Done badge
- **WHEN** a sample row has `state = Completed`
- **THEN** the badge renders a green-tinted pill with the text `Done`

#### Scenario: Paused row shows orange Paused badge
- **WHEN** a sample row has `state = Paused`
- **THEN** the badge renders an orange-tinted pill with the text `Paused`

#### Scenario: Failed row shows red Failed badge
- **WHEN** a sample row has `state = Failed`
- **THEN** the badge renders a red-tinted pill with the text `Failed`

### Requirement: Active downloads render a 3px progress bar

A sample row whose state is `Downloading` MUST additionally render a 3px progress bar above the status badge, with width proportional to `percent / 100`. The progress fill MUST use `#2AABEE`. The bar MUST be clipped to the row's rounded corners.

#### Scenario: Progress bar reflects sample percent
- **WHEN** a sample row has `state = Downloading` and `percent = 67`
- **THEN** the progress bar fills to 67% of its track width in Telegram blue

### Requirement: Media thumbnails render with active-overlay against the sample

For sample rows whose `kind` is `Photo` or `Video`, the thumbnail cell MUST paint a `rgba(255,255,255,0.08)` rounded (8px) placeholder. When the sample row's `state` is `Downloading`, the cell MUST additionally paint a `rgba(0,0,0,0.35)` overlay covering the placeholder and a centered white play-triangle SVG (a static inline path). The system MUST NOT load real photo data in this change.

#### Scenario: Photo placeholder shows during download
- **WHEN** a sample row has `kind = Photo` and `state = Downloading`
- **THEN** the thumbnail cell shows the dark placeholder with an overlay and a white play-triangle icon

#### Scenario: Completed thumbnail has no overlay
- **WHEN** a sample row has `kind = Photo` and `state = Completed`
- **THEN** the thumbnail cell shows the dark placeholder with no overlay and no play icon

### Requirement: Non-media thumbnails render as a colored abbreviation chip

For sample rows whose `kind` is not `Photo` or `Video`, the thumbnail cell MUST render a colored 44×44 rounded (8px) chip with the file extension's uppercase abbreviation. The background MUST be dark and the text MUST be color-coded by file type, matching the Figma:

| Kind | Abbreviation | Text color |
|---|---|---|
| `Document` (PDF) | `PDF` | `#E74C3C` |
| `Audio` | `MP3` | `#9B59B6` |
| `Document` (DOCX) | `DOCX` | `#2980B9` |
| `Voice` | `OGG` | `#27AE60` |
| `Archive` | `ZIP` | `#F39C12` |
| `Link` | `URL` | `#2AABEE` |

The abbreviation MUST be painted in `JetBrains Mono Bold 9px` with `0.05em` letter-spacing, centered horizontally and vertically.

#### Scenario: PDF chip renders red PDF
- **WHEN** a sample row has `kind = Document` and `fileName = "Q3_Financial_Report_2026.pdf"`
- **THEN** the thumbnail cell renders a dark chip with `PDF` in red

#### Scenario: MP3 chip renders purple MP3
- **WHEN** a sample row has `kind = Audio` and `fileName = "Ambient_Focus_Session_01.mp3"`
- **THEN** the thumbnail cell renders a dark chip with `MP3` in purple

### Requirement: Row context menu offers state-appropriate actions against the sample

Right-clicking (or long-pressing) a row MUST show a context menu with actions appropriate to the sample row's `state`:

- `Downloading` → `Pause`, `Open file` (disabled), `Show in folder` (disabled), `Copy link`, `Remove`
- `Paused` → `Resume`, `Open file` (disabled), `Show in folder` (disabled), `Copy link`, `Remove`
- `Failed` → `Retry`, `Open file` (disabled), `Show in folder` (disabled), `Copy link`, `Remove`
- `Completed` → `Open file` (disabled), `Show in folder` (disabled), `Copy link`, `Remove`

Selecting an action MUST mutate the sample row's `state` field and trigger a re-filter:
- `Pause`: `Downloading → Paused`
- `Resume`: `Paused → Downloading` (sample-only — no real download is restarted)
- `Retry`: `Failed → Downloading`
- `Remove`: any → invisible (the row disappears from every tab)

#### Scenario: Pausing an active sample row
- **WHEN** the user right-clicks a `Downloading` sample row and selects `Pause`
- **THEN** the row's `state` transitions to `Paused`, the badge updates to orange `Paused`, and the progress bar disappears

#### Scenario: Resuming a paused sample row
- **WHEN** the user right-clicks a `Paused` sample row and selects `Resume`
- **THEN** the row's `state` transitions to `Downloading`, the badge updates to blue `<percent>%`, and the progress bar reappears

#### Scenario: Retrying a failed sample row
- **WHEN** the user right-clicks a `Failed` sample row and selects `Retry`
- **THEN** the row's `state` transitions to `Downloading`, the badge updates to blue `<percent>%`, and the progress bar reappears

#### Scenario: Removing a sample row
- **WHEN** the user right-clicks any sample row and selects `Remove`
- **THEN** the row disappears from every tab and the tab count badges decrement

### Requirement: Footer status bar shows active count and bulk actions on the sample

The footer MUST render at the bottom of the embedded surface: a blue 8×8 round dot (or gray when count is `0`), the text `N files downloading` (Inter Regular 12px) where `N` equals the number of sample rows whose `state == Downloading`, a flex spacer, and two text-button rows labeled `Pause all` and `Cancel all`. Footer height MUST be `36px`. The dot MUST be `#2AABEE` when at least one row is `Downloading` and gray (`#FFFFFF` at 30% alpha) otherwise.

#### Scenario: Footer count tracks the sample
- **WHEN** the user pauses one of the two `Downloading` rows via the row context menu
- **THEN** the footer text updates from `2 files downloading` to `1 files downloading`

#### Scenario: Pause all pauses every active sample row
- **WHEN** the user clicks `Pause all` while 2 rows are `Downloading`
- **THEN** both rows transition to `Paused`, both badges turn orange, and the footer count drops to `0`

#### Scenario: Cancel all marks every active sample row as completed
- **WHEN** the user clicks `Cancel all` while 2 rows are `Downloading`
- **THEN** both rows transition to `Completed`, both badges turn green `Done`, and the footer count drops to `0`. The rows remain visible in the list (this is a demo stand-in for `Cancelled`).

### Requirement: Sample reset on section navigation

The hardcoded sample MUST be reset to its initial state whenever the user navigates away from `Section::Downloads` and back. The sample MUST be a `constexpr`-friendly static array; mutations in one session MUST NOT survive a section swap.

#### Scenario: Pause all then leave and return
- **WHEN** the user clicks `Pause all` (both rows become `Paused`), then clicks the chat rail entry, then clicks the Downloads rail entry again
- **THEN** the embedded surface is reconstructed with the sample reset to its initial state: both rows are `Downloading` again, the footer shows `2 files downloading`

### Requirement: Embedded surface renders empty state when no rows match

When the visible sample is empty (after filtering or after the user removes every row), the surface MUST render a centered empty-state placeholder with a short title (`Inter Medium 14px`) and a one-line subtitle (`Inter Regular 11px`) drawn from translation keys `lng_downloads_empty_no_tasks` / `lng_downloads_empty_no_tasks_desc`. The footer MUST still render with count `0` so the user can see bulk-action state.

#### Scenario: Empty state appears after removing every row
- **WHEN** the user removes all 15 sample rows via the row context menu
- **THEN** the list area renders the empty-state placeholder; the footer remains visible at the bottom