## ADDED Requirements

### Requirement: Downloads entry in the left navigation rail

The main menu MUST render a `Downloads` entry in the left navigation column, immediately above the bottom settings icon. The entry MUST be a 40×40 round button (`12px` radius) with `&st::menuIconDownloads` as its icon. Clicking the entry MUST call `controller->showDownloads()`. The entry MUST be hidden when `controller->session().supportMode()` is true.

#### Scenario: Entry renders in the rail
- **WHEN** the main menu opens with `supportMode = false`
- **THEN** the Downloads entry appears in the navigation column

#### Scenario: Entry is hidden in support mode
- **WHEN** the main menu opens with `supportMode = true`
- **THEN** the Downloads entry MUST NOT appear

#### Scenario: Click swaps to the embedded surface
- **WHEN** the user clicks the Downloads entry
- **THEN** `SessionNavigation::showDownloads()` is called and `MainWidget::showSection(MainWidget::Section::Downloads)` replaces the main-widget content with the embedded `DownloadsSection`

### Requirement: Selected state paints the entry background

When `Section::Downloads` is the active section, the entry's background MUST be filled with `rgba(42,171,238,0.20)` (Telegram blue at 20% alpha) and the icon MUST remain white. When the section is not active, the entry MUST use the default icon-button background.

#### Scenario: Selected styling applies when the embedded surface is active
- **WHEN** the user has activated `Section::Downloads`
- **THEN** the entry renders with a translucent blue background

#### Scenario: Selected styling clears when another section becomes active
- **WHEN** the user activates `Section::Chat` (or any other section) while the Downloads entry was selected
- **THEN** the entry's background returns to the default icon-button background on the next `update()`

### Requirement: Active-count badge appears in the entry's top-right

When the demo's hardcoded active count is greater than `0`, the entry MUST render a circular badge at coordinates `(22, 4)` relative to the entry's icon (top-right corner) with `14×14` size. The badge MUST be filled with `#2AABEE` (Telegram blue) and MUST contain the integer in white (`JetBrains Mono Bold 8px`, centered). The badge MUST be hidden when the count is `0`.

For this change, the active count is a hardcoded `MainMenu::kDemoActiveCount = 2` constant — a `static constexpr int` on `MainMenu`. The constant MUST NOT be wired to `DownloadCenter::computeStats()` in this change; that wiring belongs to the follow-up change.

#### Scenario: Badge appears with the demo value
- **WHEN** `kDemoActiveCount` is `2` and the main menu opens
- **THEN** the badge is visible with the digit `2` painted in white

#### Scenario: Badge disappears when the demo value is zero
- **WHEN** `kDemoActiveCount` is `0` (e.g., a future demo variant) and the main menu opens
- **THEN** the badge is hidden

### Requirement: Badge polls the demo value at 5 Hz

The entry MUST poll `kDemoActiveCount` every 200ms via a `base::Timer`. The poll MUST call `update()` only when the value changes (a dirty-check on the previous count). The poll MUST be stopped when the main menu is destroyed.

#### Scenario: Polling does not repaint on every tick
- **WHEN** `kDemoActiveCount` stays at `2` for 5 consecutive ticks
- **THEN** `update()` is called at most once for the entire duration

#### Scenario: Polling stops on menu destruction
- **WHEN** the main menu widget is destroyed
- **THEN** the polling timer's lifetime is released and no further `update()` calls fire

### Requirement: Entry reuses the existing `MainMenu::setupMenu()` pattern

The Downloads entry MUST be appended to `MainMenu::setupMenu()` after the night-mode block (line 784 area), using the same `addAction(label, {&icon}) → setClickedCallback(...)` pattern as the existing rail entries. The 5 Hz timer MUST be added as a `base::Timer` member on `MainMenu` and initialized in the constructor or in `setupMenu()` before its `setCallback(...)` is wired. The dirty-check on `kDemoActiveCount` MUST be in the timer's callback.

#### Scenario: Entry compiles with the existing menu code
- **WHEN** the change is merged and the project is built with `cmake --build out --config Debug --target Telegram`
- **THEN** the build succeeds and `MainMenu::setupMenu()` registers the Downloads entry