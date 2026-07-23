## ADDED Requirements

### Requirement: Sidebar entry exists
The main window's left sidebar (`Window::MainMenu`) SHALL contain a "下载中心" menu action positioned immediately after the night-mode toggle, identical in visual style to the existing "设置" and "夜间模式" rows.

#### Scenario: Entry is rendered
- **WHEN** the user opens the main window's sidebar
- **THEN** the "下载中心" entry is visible below the "夜间模式" row

#### Scenario: Entry uses the download icon
- **WHEN** the sidebar is rendered
- **THEN** the "下载中心" entry uses the existing Icons8 icon id `20FjgTazh8FG` (registered as `st::menuIconDownloads`)

#### Scenario: Entry has translation key
- **WHEN** the user views the sidebar in any locale
- **THEN** the entry label is rendered from the `lng_menu_downloads` translation key

### Requirement: Click opens the download center
Clicking the sidebar entry SHALL open the download center window, replacing or stacking with the currently shown content in the same way that `showSettings()` does for the settings box.

#### Scenario: Click from default state
- **WHEN** the user clicks "下载中心" while viewing a chat
- **THEN** the download center window is shown

#### Scenario: Click while settings is open
- **WHEN** the user clicks "下载中心" while the settings window is open
- **THEN** the download center window replaces the settings window in the same stack slot

#### Scenario: Back navigation returns to previous content
- **WHEN** the user closes the download center window
- **THEN** the previously visible content (chat list / settings / etc.) is restored

### Requirement: Live badge shows active count
The sidebar entry SHALL display a numeric badge on its right edge showing the number of currently active downloads, styled identically to the existing unread-badge on chat entries. The badge SHALL be hidden when the count is zero.

#### Scenario: Badge appears when downloads are active
- **WHEN** at least one task is in `Downloading` or `Paused` status
- **THEN** the badge shows the active count

#### Scenario: Badge disappears when no active downloads
- **WHEN** no task is in `Downloading` or `Paused` status
- **THEN** the badge is not rendered

#### Scenario: Badge updates on status change
- **WHEN** a task transitions between any status
- **THEN** the badge value is recomputed within 200 ms

### Requirement: Entry integrates with the existing menu system
The new entry SHALL be added through the same `addAction()` mechanism as the other sidebar items, with no bespoke rendering or layout code outside `MainMenu`.

#### Scenario: Entry uses mainMenuButton style
- **WHEN** the entry is added
- **THEN** it is rendered with `st::mainMenuButton` and follows the same `_unreadBadge` mechanism as `ToggleAccountsButton`

#### Scenario: Entry is hidden in support mode
- **WHEN** `controller->session().supportMode()` returns true
- **THEN** the entry is not added, matching the behavior of other user-facing sidebar items
