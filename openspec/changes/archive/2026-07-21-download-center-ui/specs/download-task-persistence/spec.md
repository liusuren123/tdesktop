## ADDED Requirements

### Requirement: Status values persist
`Data::DownloadCenter`'s JSON serialization SHALL round-trip the `Status` values `Downloading`, `Waiting`, `Paused`, `Completed`, `Failed`, and `Removed`. The serializer SHALL accept each value as a string and deserialize each string back to the same enum value.

#### Scenario: Status round-trips
- **WHEN** a task with `status == Waiting` is saved to JSON and the file is reloaded
- **THEN** the task loads with `status == Waiting`

#### Scenario: Unknown status values do not crash
- **WHEN** the JSON contains a `status` value that the current build does not recognise (forward-compat case)
- **THEN** the loader maps the value to `Failed` and emits a one-time warning to the log

### Requirement: New fields persist
The serializer SHALL write the `sha256`, `startedAt`, and `speedHistory60s` fields when they are non-default, and SHALL load them back to the corresponding task. The speed-history ring SHALL be persisted as a JSON array of integer bytes-per-second samples in chronological order.

#### Scenario: Round-trip with new fields populated
- **WHEN** a task has `sha256 == "abc..."`, `startedAt == 2024-08-01T10:00:00Z`, and `speedHistory60s == [12, 13, 14, ...]`
- **THEN** the same values load back after a save/load cycle

#### Scenario: Empty speed history round-trips
- **WHEN** a task has an empty `speedHistory60s`
- **THEN** the serialized JSON either omits the field or writes an empty array, and the loader produces an empty ring

#### Scenario: Backward compat with old JSON
- **WHEN** the JSON file does not contain `sha256`, `startedAt`, or `speedHistory60s`
- **THEN** the loader populates the task with default-constructed values: empty SHA-256, zero `startedAt`, and an empty speed-history ring, with no error

### Requirement: Per-write debounce
The serializer SHALL debounce writes with a 500 ms idle interval. Consecutive `taskUpdated` events SHALL coalesce into a single write.

#### Scenario: Bursty updates write once
- **WHEN** 50 `taskUpdated` events arrive within 500 ms
- **THEN** the JSON file is written at most once, 500 ms after the last event

### Requirement: Atomic save
The save routine SHALL write the JSON to a temporary file in the same directory and atomically rename it over the target, so a crash during write cannot corrupt the existing JSON file.

#### Scenario: Crash mid-write leaves previous file intact
- **WHEN** the process is killed while the temp file is being written
- **THEN** the existing `downloads.json` is unchanged on next startup

#### Scenario: Successful save replaces previous file
- **WHEN** the temp file is fully written and renamed
- **THEN** the new file is in place and the old file no longer exists
