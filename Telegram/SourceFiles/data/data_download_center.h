/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once
#include "base/weak_ptr.h"
#include "base/flat_map.h"
#include "base/timer.h"
#include "data/data_file_origin.h"
#include "storage/parallel_download_controller.h"
#include <QtCore/QDateTime>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <deque>
#include <variant>
class DocumentData;
class PhotoData;
class PeerData;
namespace Main {
class Session;
} // namespace Main
namespace MTP {
using DcId = int32;
} // namespace MTP
namespace Storage {
class ParallelDownloadController;
} // namespace Storage
namespace Data {
using DownloadTaskId = uint64;
enum class DownloadState {
	Queued,
	Downloading,
	Waiting,
	Paused,
	Completed,
	Failed,
	Cancelled,
	Removed,
};
enum class DownloadSource {
	Document,
	Photo,
	Url,
};
struct DownloadTask {
	DownloadTaskId id = 0;
	DownloadState state = DownloadState::Queued;
	QString fileName;
	QString savePath;
	int64 totalSize = 0;
	int64 readySize = 0;
	QString mimeType;
	DownloadSource source = DownloadSource::Document;
	DocumentId documentId = 0;
	FullMsgId itemId;
	PeerId peerId = 0;
	uint64 accessHash = 0;
	QByteArray fileReference;
	MTP::DcId downloadDcId = 0;
	Data::FileOrigin origin;
	QDateTime addedAt;
	QDateTime completedAt;
	int parallelChunks = 4;
	int64 chunkSize = 128 * 1024;
	QString errorMessage;
	QString savedAbsolutePath;
	// SHA-256 of the source media (populated on completion when the
	// server reports the digest; empty otherwise). Drives the detail
	// panel's "文件大小" row and is used for content-addressed retries.
	QString sha256;
	// Wall-clock when the task first entered Downloading. Stays
	// unchanged across pause/resume so the detail panel can show a
	// stable ETA against `addedAt`.
	QDateTime startedAt;
	// 60-sample ring of bytes-per-second readings. Sampled once per
	// second by DownloadCenter::speedTimerTick(); persisted to JSON
	// as an integer array. Bounded so the row never grows.
	std::deque<int64> speedHistory60s;
	// Persistent per-chunk state (temp file paths + ready bytes). On
	// every progress update the controller snapshots its current
	// chunks into this vector and DownloadCenter::scheduleSave()
	// writes it to disk, so the next run can resume the same temp
	// files at the same offsets instead of starting from 0.
	std::vector<Storage::ChunkState> chunks;
};

// Snapshot aggregate built on demand by DownloadCenter::computeStats().
// Drives the status bar and the sidebar badge.
struct DownloadStats {
	int activeCount = 0;            // Downloading
	int waitingCount = 0;            // Waiting (queued behind cap)
	int pausedCount = 0;             // Paused
	int completedCount = 0;          // Completed
	int failedCount = 0;             // Failed
	int removedCount = 0;            // Removed (in trash)
	int totalDownloadedThisMonth = 0;// bytes, completed tasks this calendar month
	int64 uploadBps = 0;             // reserved for future upload support
	int64 downloadBps = 0;           // sum of instantaneous bytes-per-second
};
class DownloadCenter final : public base::has_weak_ptr {
public:
	explicit DownloadCenter(not_null<Main::Session*> session);
	~DownloadCenter();
	DownloadCenter(const DownloadCenter &) = delete;
	DownloadCenter &operator=(const DownloadCenter &) = delete;
	[[nodiscard]] not_null<Main::Session*> session() const {
		return _session;
	}
	[[nodiscard]] DownloadTaskId addDocument(
			not_null<DocumentData*> document,
			const QString &savePath,
			const Data::FileOrigin &origin);
		[[nodiscard]] DownloadTaskId addPhoto(
			not_null<PhotoData*> photo,
			const QString &savePath,
			const Data::FileOrigin &origin);
	void pause(DownloadTaskId id);
	void resume(DownloadTaskId id);
	void cancel(DownloadTaskId id);
	void retry(DownloadTaskId id);
	void remove(DownloadTaskId id);
	void removeToTrash(DownloadTaskId id);
	void removeForever(DownloadTaskId id);
	void openFile(DownloadTaskId id);
	void showInFolder(DownloadTaskId id);
	// Returns the oldest task currently in `Waiting` (or std::nullopt
	// if the queue is empty). Used by promoteWaiting() and exposed
	// for tests.
	[[nodiscard]] std::optional<DownloadTaskId> oldestWaitingTask() const;
	// Promote the oldest Waiting task to Downloading if there is
	// concurrency headroom. No-op if the cap is already met.
	void promoteWaiting();
	[[nodiscard]] const DownloadTask *task(DownloadTaskId id) const;
	[[nodiscard]] DownloadTask *task(DownloadTaskId id);
	[[nodiscard]] auto tasks() const
		-> ranges::any_view<const DownloadTask*, ranges::category::input>;
	[[nodiscard]] int countByState(DownloadState state) const;
	[[nodiscard]] int totalCount() const;
	[[nodiscard]] DownloadStats computeStats() const;
	void updateSpeedSample(DownloadTaskId id, int64 bps);
	[[nodiscard]] rpl::producer<DownloadTaskId> taskAdded() const {
		return _taskAdded.events();
	}
	[[nodiscard]] rpl::producer<DownloadTaskId> taskUpdated() const {
		return _taskUpdated.events();
	}
	[[nodiscard]] rpl::producer<DownloadTaskId> taskRemoved() const {
		return _taskRemoved.events();
	}
	[[nodiscard]] rpl::producer<> tasksReloaded() const {
		return _tasksReloaded.events();
	}
	void saveToDisk();
		void loadFromDisk(bool autoResumeQueued = false);
	[[nodiscard]] static int RecommendedParallelChunks();
private:
	void startTask(DownloadTaskId id);
	void stopTask(DownloadTaskId id);
	void startTaskAfterRefresh(DownloadTaskId id);
	void refreshTaskFileReference(
		DownloadTaskId id,
		Fn<void(bool ok)> done);
	void fetchChannelThenRefresh(
		DownloadTaskId id,
		ChannelId channelId,
		Fn<void(bool ok)> done);
	void requestMessageAndUpdateTask(
		DownloadTaskId id,
		PeerData *peer,
		Fn<void(bool ok)> done);
	void onControllerFinished(
		DownloadTaskId id,
		bool ok,
		const QString &error);
	void onControllerProgress(
		DownloadTaskId id,
		int64 ready,
		int64 total);
	void setState(
		DownloadTaskId id,
		DownloadState state,
		const QString &error = QString());
	void scheduleSave();
	void speedTimerTick();
	void startSpeedSampler();
	void stopSpeedSampler();
	[[nodiscard]] int activeDownloadCount() const;
	static constexpr int kMaxParallelDownloads = 4;
	[[nodiscard]] QString storagePath() const;
	[[nodiscard]] DownloadTaskId allocateNextId();
	struct Snapshot {
		DownloadTaskId nextId = 1;
		std::vector<DownloadTask> tasks;
	};
	[[nodiscard]] Snapshot buildSnapshot() const;
	void restoreSnapshot(const Snapshot &snapshot);
	void writeSnapshot(const Snapshot &snapshot);
	[[nodiscard]] std::optional<Snapshot> readSnapshot();
	const not_null<Main::Session*> _session;
	base::flat_map<DownloadTaskId, DownloadTask> _tasks;
	base::flat_map<DownloadTaskId,
		std::unique_ptr<Storage::ParallelDownloadController>> _controllers;
	DownloadTaskId _nextId = 1;
	rpl::event_stream<DownloadTaskId> _taskAdded;
	rpl::event_stream<DownloadTaskId> _taskUpdated;
	rpl::event_stream<DownloadTaskId> _taskRemoved;
	rpl::event_stream<> _tasksReloaded;
	base::Timer _saveTimer;
	base::Timer _speedTimer;
	int64 _lastProgressStamp = 0; // for 1Hz speed sampling
	base::flat_map<DownloadTaskId, int64> _lastProgressBytes;
};
} // namespace Data
