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
#include <QtCore/QDateTime>
#include <QtCore/QByteArray>
#include <QtCore/QString>
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
	Paused,
	Completed,
	Failed,
	Cancelled,
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
	void openFile(DownloadTaskId id);
	void showInFolder(DownloadTaskId id);
	[[nodiscard]] const DownloadTask *task(DownloadTaskId id) const;
	[[nodiscard]] DownloadTask *task(DownloadTaskId id);
	[[nodiscard]] auto tasks() const
		-> ranges::any_view<const DownloadTask*, ranges::category::input>;
	[[nodiscard]] int countByState(DownloadState state) const;
	[[nodiscard]] int totalCount() const;
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
};
} // namespace Data
