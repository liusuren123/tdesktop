# download-center-ui Specification

## Purpose
TBD - created by archiving change download-center-ui. Update Purpose after archive.
## Requirements
### Requirement: Window structure
The download center SHALL be a 1280×800 dark-themed box with the following regions, all positioned absolutely within the window:

- Title bar (1280×56) at top
- Sidebar (220×684) below the title bar on the left
- Main content area (1060×684) to the right of the sidebar
- Status bar (1280×60) at the bottom

#### Scenario: Window opens with correct dimensions
- **WHEN** the user opens the download center
- **THEN** the window is rendered at 1280×800 and contains the four regions above

#### Scenario: Dark theme is applied
- **WHEN** the window is rendered
- **THEN** the background color of the main window is `#0F0F14`, panels are `#16161E`, and text uses the dark-theme tokens defined in OPENSPEC_PROPOSAL.md §4.1

### Requirement: Sidebar navigation
The in-window sidebar SHALL have two groups: "下载任务" with items 我的下载 / 下载中 / 已完成 / 失败 / 回收站, and "按媒体类型" with items 图片 / 视频 / 文档 / 音频. Each item SHALL show a count badge and SHALL be clickable to filter the task list.

#### Scenario: Group headers
- **WHEN** the sidebar is rendered
- **THEN** the two groups appear with their respective headers and items

#### Scenario: Filter is applied on click
- **WHEN** the user clicks "已完成" in the sidebar
- **THEN** the task list shows only tasks with `status == Completed`

#### Scenario: Counts reflect current state
- **WHEN** a task transitions to `Completed`
- **THEN** the "已完成" badge increments by one and the "下载中" badge decrements by one

#### Scenario: Selected item is highlighted
- **WHEN** an item is the active filter
- **THEN** it has a `#25252C` background, a 3×24 blue left bar (`#3B82F6`), and white bold text

### Requirement: Tab bar
The top of the main content area SHALL contain a tab bar with five tabs: 下载中 / 全部 / 已完成 / 失败 / 回收站, each with a count badge. The active tab SHALL have a 2px blue underline and white text; inactive tabs SHALL be gray (`#A1A1AA`). A right-aligned group of two 32×32 buttons SHALL toggle between list and grid view (list view is the only implemented view in v1).

#### Scenario: Tab count badges
- **WHEN** the tab bar is rendered
- **THEN** each tab shows a count of tasks matching its filter

#### Scenario: Active tab is highlighted
- **WHEN** the user clicks a tab
- **THEN** that tab's text becomes white bold, the count badge becomes blue (`#3B82F6`), and the 2px underline appears

### Requirement: Action bar
Below the tab bar, an action bar SHALL show buttons "下载文件" (primary, `#3B82F6`), "全部继续", "全部暂停", "删除" (secondary, `#25252C`), and two right-aligned dropdowns "来源筛选" and "按时间排序".

#### Scenario: Primary action triggers file picker
- **WHEN** the user clicks "下载文件"
- **THEN** a file picker dialog appears accepting URLs and local paths

#### Scenario: Resume all is enabled when at least one paused task exists
- **WHEN** the task list contains at least one `Paused` task
- **THEN** "全部继续" is enabled

#### Scenario: Resume all is disabled when nothing is paused
- **WHEN** no `Paused` tasks exist
- **THEN** "全部继续" is disabled (visually dimmed)

#### Scenario: Source filter dropdown opens
- **WHEN** the user clicks "来源筛选"
- **THEN** a dropdown menu appears with options "全部来源" / "来自聊天" / "来自频道" / "来自机器人"

### Requirement: Column headers
Above the task list, column headers SHALL read 文件名 / 大小 / 进度 / 速度 / 状态 / 用时. A 16×16 checkbox SHALL be present at the leftmost position to select all visible tasks.

#### Scenario: Click sort indicator
- **WHEN** the user clicks a column header
- **THEN** the task list is sorted by that column, ascending on the first click and descending on the second click

### Requirement: Task row rendering
Each task SHALL be rendered as a 64px-tall row with: a checkbox, a 32×32 type icon (tinted by media kind), the filename (13pt white) above a 11pt gray source/date sub-line, a right-aligned size, a progress bar with 8px track and 8px fill, a right-aligned speed, a status badge, and a right-aligned elapsed-time. Odd and even rows SHALL use alternating background colors `#16161E` and `#1A1A24`; the row matching the current selection SHALL use `#1A1A24`.

#### Scenario: Status badge colors
- **WHEN** a task is in `Downloading` status
- **THEN** its badge is `#10B981` with text "下载中"
- **WHEN** a task is in `Paused` status
- **THEN** its badge is `#F59E0B` with text "已暂停"
- **WHEN** a task is in `Failed` status
- **THEN** its badge is `#EF4444` with text "失败"
- **WHEN** a task is in `Completed` status
- **THEN** its badge is `#10B981` with text "已完成"
- **WHEN** a task is in `Removed` status
- **THEN** its badge is `#71717A` with text "已移除"
- **WHEN** a task is in `Waiting` status
- **THEN** its badge is `#3B82F6` with text "排队中"

#### Scenario: Progress bar reflects chunk completion
- **WHEN** the controller reports `readySize` and `totalSize` for a task
- **THEN** the bar's fill width is `readySize / totalSize` and the right-aligned text is the integer percent

#### Scenario: Selecting a row updates detail panel
- **WHEN** the user clicks a task row
- **THEN** the detail panel updates to show that task's fields

### Requirement: Detail panel
When a task is selected, a 212px-tall panel SHALL appear at the bottom of the main content area with: task title, filename, and labeled rows for 消息 ID, 保存路径, 来自, 文件大小 (with byte count and SHA-256), 添加时间 (with ETA when downloading). On the right side, the panel SHALL show "实时速度" as a 20pt green number and a 370×100 live speed chart displaying the last 60 seconds as 60 vertical bars.

#### Scenario: Detail panel shows selected task data
- **WHEN** a task is selected
- **THEN** every row in the detail panel is populated with that task's data

#### Scenario: Live speed updates per second
- **WHEN** the selected task is in `Downloading` status
- **THEN** the "实时速度" number updates at least once per second

#### Scenario: Speed chart renders 60 bars
- **WHEN** the detail panel is rendered
- **THEN** the speed chart shows up to 60 vertical bars, each 6px wide with 4px gaps, the most recent bar on the right

#### Scenario: Detail panel hides when no task selected
- **WHEN** no task is selected
- **THEN** the detail panel shows an empty-state placeholder

### Requirement: Status bar
The window's bottom 60px SHALL be a status bar showing: upload speed (`↑`), download speed (`↓`, green), active count ("活动中" + integer, orange if > 0), message count ("消息" + integer), and a right-aligned "本月已下载" badge with the monthly byte total.

#### Scenario: Status bar updates from DownloadStats
- **WHEN** any task's `speedBps` changes
- **THEN** the corresponding status-bar value reflects the new aggregate within 200 ms

### Requirement: Empty and error states
When the task list is empty, the main content area SHALL display a centered icon (`#A1A1AA`, 120×120), a primary heading, a secondary description, and a "下载文件" primary button. Distinct empty states SHALL be defined for: no tasks at all, no tasks matching the current filter, no network connection, and insufficient disk space.

#### Scenario: Empty state when no tasks
- **WHEN** the download center is opened with zero tasks
- **THEN** the empty-state placeholder is shown

#### Scenario: Network-loss state
- **WHEN** the system reports no network connectivity
- **THEN** the empty-state placeholder shows the offline icon and the message "当前无网络连接，连接后自动恢复下载"

#### Scenario: Insufficient disk space state
- **WHEN** the download directory has less than 1 GB free and at least one task is queued
- **THEN** the empty-state placeholder shows the disk icon and the message "下载路径剩余空间不足" with a "更改下载路径" button

### Requirement: Dialogs and overlays
The download center SHALL provide: a delete confirmation dialog (title, body, "同时删除磁盘上的文件" checkbox, "取消" and "移除" buttons), a failure dialog (file name, reason, "关闭" and "重新下载" buttons), a context menu (继续/暂停/打开文件/打开文件夹/复制链接/重新下载/移除/永久删除/属性), a completion toast (file name, size, "打开" and "文件夹" buttons, 5-second auto-dismiss), and source/sort dropdowns (rendered on top of the action bar).

#### Scenario: Delete confirmation prevents accidental removal
- **WHEN** the user clicks "删除" in the action bar with at least one selected task
- **THEN** the delete confirmation dialog appears and the tasks are not removed until the user clicks "移除"

#### Scenario: Failure dialog explains the cause
- **WHEN** a task transitions to `Failed`
- **THEN** the failure dialog appears with the task's filename and a localized error message

#### Scenario: Completion toast auto-dismisses
- **WHEN** a task transitions to `Completed`
- **THEN** a toast appears with the file name, byte size, and two buttons, and disappears after 5 seconds unless the user hovers over it

#### Scenario: Context menu reflects status
- **WHEN** the user right-clicks a `Paused` task
- **THEN** the context menu shows "继续" as the first action and "暂停" is hidden

### Requirement: Performance budgets
The download center SHALL meet the following performance budgets: scrolling 1000 task rows at 60 fps; speed chart and status bar updates at most 5 Hz; selection-state propagation at most 10 Hz; column sort at most 200 ms for 1000 rows.

#### Scenario: 1000 rows scroll at 60 fps
- **WHEN** the user scrolls through 1000 visible task rows
- **THEN** the average frame time is at most 16.6 ms

#### Scenario: Speed chart update rate
- **WHEN** 1 second elapses while a task is downloading
- **THEN** the speed chart has appended exactly one new sample

