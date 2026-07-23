## ADDED Requirements

### Requirement: Task status extension
The download center SHALL recognise the task statuses `Downloading`, `Waiting`, `Paused`, `Completed`, `Failed`, `Removed`, in addition to whatever the existing implementation already defines. Each status SHALL be persistable through `Data::DownloadCenter`'s JSON round-trip and SHALL be visible in the UI as a distinct status badge color.

#### Scenario: New task starts in Downloading
- **WHEN** a user-initiated download is enqueued and there is spare concurrency capacity
- **THEN** the new `DownloadTask::status` is `Downloading` and the task appears in the "下载中" sidebar filter

#### Scenario: Task becomes Waiting when concurrency cap reached
- **WHEN** a new download is enqueued while the existing concurrent downloads already equal `kMaxParallelDownloads`
- **THEN** the task's `status` is `Waiting` and it does not consume a download slot

#### Scenario: Waiting task transitions to Downloading
- **WHEN** any active `Downloading` task completes, fails, or is paused
- **THEN** the oldest `Waiting` task's status becomes `Downloading` and the controller starts a chunk loader for it

#### Scenario: Failed task can be retried
- **WHEN** a user clicks "重新下载" on a `Failed` task
- **THEN** the task transitions `Failed → Waiting` and rejoins the queue

#### Scenario: User removes task to trash
- **WHEN** a user clicks "移除" on a task
- **THEN** the task's `status` is `Removed` and it is moved out of the "全部" filter into the "回收站" filter

#### Scenario: Permanently deleted tasks are removed
- **WHEN** a user clicks "永久删除" on a `Removed` task
- **THEN** the task is erased from `DownloadCenter` and the corresponding temp file is deleted from disk

### Requirement: Task record fields
Each `DownloadTask` SHALL carry `sha256`, `startedAt`, and `speedHistory60s` in addition to its existing fields. Fields absent from older JSON on disk SHALL load as default-constructed values without producing a load error.

#### Scenario: Legacy JSON loads with default new fields
- **WHEN** the user upgrades and `DownloadCenter` reads a pre-existing `downloads.json` that does not contain `sha256`, `startedAt`, or `speedHistory60s`
- **THEN** the affected tasks load with empty SHA-256 string, zero `startedAt`, and an empty speed-history ring, and no error is reported

#### Scenario: SHA-256 captured when known
- **WHEN** a media source supplies a SHA-256 (currently only for some document types and saved GIFs)
- **THEN** the task's `sha256` is populated and is displayed in the detail panel as `SHA-256: 9f1f…a4b3`

#### Scenario: Speed history samples are written
- **WHEN** a task is in `Downloading` status
- **THEN** exactly one sample per second is appended to `speedHistory60s`, the ring is trimmed to the most recent 60 samples, and old samples are dropped

### Requirement: Aggregate stats
`Data::DownloadCenter` SHALL expose a `DownloadStats` aggregate containing `activeCount` (Downloading + Paused), `waitingCount`, `completedCount`, `failedCount`, `removedCount`, `totalDownloadedThisMonth` (sum of `readySize` for tasks that became Completed since the start of the current calendar month), `uploadBps`, and `downloadBps` (sum of per-task `speedBps`).

#### Scenario: Active count is exposed
- **WHEN** the sidebar entry queries the active count
- **THEN** it returns the number of tasks whose status is `Downloading` or `Paused`

#### Scenario: Monthly downloaded sum updates on completion
- **WHEN** a task transitions from `Downloading` to `Completed`
- **THEN** `totalDownloadedThisMonth` is incremented by the task's `totalSize`

#### Scenario: Download rate is aggregated
- **WHEN** the status bar queries `downloadBps`
- **THEN** it returns the sum of `speedBps` across all tasks whose status is `Downloading`
