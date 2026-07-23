/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_download_center.h"
#include "storage/parallel_download_controller.h"
#include "data/data_document.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "data/data_channel.h"
#include "main/main_session.h"
#include "apiwrap.h"
#include "base/timer.h"
#include "base/weak_ptr.h"
#include "base/debug_log.h"
#include "core/application.h"
#include "settings.h"
#include "crl/crl_on_main.h"
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QDir>
#include <QtGui/QDesktopServices>
#include <QtCore/QUrl>
#include <QtCore/QProcess>

#if defined Q_OS_WIN
#include <windows.h>
#include <ShlObj.h>
#endif
namespace Data {
namespace {
constexpr auto kFormatVersion = 1;
constexpr auto kSaveDebounceMs = 500;
[[nodiscard]] QString SerializeState(DownloadState state) {
	switch (state) {
	case DownloadState::Queued: return u"queued"_q;
	case DownloadState::Downloading: return u"downloading"_q;
	case DownloadState::Waiting: return u"waiting"_q;
	case DownloadState::Paused: return u"paused"_q;
	case DownloadState::Completed: return u"completed"_q;
	case DownloadState::Failed: return u"failed"_q;
	case DownloadState::Cancelled: return u"cancelled"_q;
	case DownloadState::Removed: return u"removed"_q;
	}
	return u"queued"_q;
}
[[nodiscard]] std::optional<DownloadState> DeserializeState(
		const QString &value) {
	if (value == u"queued"_q) return DownloadState::Queued;
	if (value == u"downloading"_q) return DownloadState::Downloading;
	if (value == u"waiting"_q) return DownloadState::Waiting;
	if (value == u"paused"_q) return DownloadState::Paused;
	if (value == u"completed"_q) return DownloadState::Completed;
	if (value == u"failed"_q) return DownloadState::Failed;
	if (value == u"cancelled"_q) return DownloadState::Cancelled;
	if (value == u"removed"_q) return DownloadState::Removed;
	return std::nullopt;
}
[[nodiscard]] QString SerializeSource(DownloadSource source) {
	switch (source) {
	case DownloadSource::Document: return u"document"_q;
	case DownloadSource::Photo: return u"photo"_q;
	case DownloadSource::Url: return u"url"_q;
	}
	return u"document"_q;
}
[[nodiscard]] std::optional<DownloadSource> DeserializeSource(
		const QString &value) {
	if (value == u"document"_q) return DownloadSource::Document;
	if (value == u"photo"_q) return DownloadSource::Photo;
	if (value == u"url"_q) return DownloadSource::Url;
	return std::nullopt;
}
[[nodiscard]] QString SourceMimeType(DownloadSource source) {
	switch (source) {
	case DownloadSource::Document: return QString();
	case DownloadSource::Photo: return u"image/jpeg"_q;
	case DownloadSource::Url: return u"application/octet-stream"_q;
	}
	return QString();
}
} // namespace
DownloadCenter::DownloadCenter(not_null<Main::Session*> session)
: _session(session)
, _saveTimer([=] { saveToDisk(); })
, _speedTimer([=] { speedTimerTick(); }) {
	startSpeedSampler();
}
DownloadCenter::~DownloadCenter() {
	stopSpeedSampler();
	if (_saveTimer.isActive()) {
		_saveTimer.cancel();
	}
	_controllers.clear();
	saveToDisk();
}
int DownloadCenter::RecommendedParallelChunks() {
	return 4;
}
DownloadTaskId DownloadCenter::allocateNextId() {
	const auto id = _nextId;
	_nextId = (_nextId == std::numeric_limits<DownloadTaskId>::max())
		? 1
		: _nextId + 1;
	return id;
}
QString DownloadCenter::storagePath() const {
	const auto userId = _session->userId().bare;
	const auto dir = QDir(cWorkingDir() + u"tdata"_q);
	return dir.absoluteFilePath(u"download_center_%1.json"_q.arg(userId));
}
DownloadTaskId DownloadCenter::addDocument(
		not_null<DocumentData*> document,
		const QString &savePath,
		const Data::FileOrigin &origin) {
	auto task = DownloadTask();
	task.id = allocateNextId();
	task.source = DownloadSource::Document;
	task.documentId = document->id;
	task.fileName = document->filename();
	task.savePath = savePath;
	task.mimeType = document->mimeString();
	task.totalSize = document->size;
	task.downloadDcId = document->remoteDcId();
	task.accessHash = document->remoteAccessHash();
	task.fileReference = document->remoteFileReference();
	task.addedAt = QDateTime::currentDateTime();
	task.origin = origin;
	if (const auto msgOrigin
		= std::get_if<Data::FileOriginMessage>(&origin.data)) {
		task.peerId = msgOrigin->peer;
		task.itemId = *msgOrigin;
	}
	const auto id = task.id;
	// Concurrency cap: if we are already at kMaxParallelDownloads
	// active transfers, the new task enters Waiting instead of being
	// started. promoteWaiting() will move it to Downloading once
	// headroom opens up (e.g. after onControllerFinished fires).
	task.state = (activeDownloadCount() >= kMaxParallelDownloads)
		? DownloadState::Waiting
		: DownloadState::Queued;
	_tasks.emplace(id, std::move(task));
	_taskAdded.fire_copy(id);
	scheduleSave();
	if (task.state == DownloadState::Queued) {
		startTask(id);
	}
	return id;
}
DownloadTaskId DownloadCenter::addPhoto(
		not_null<PhotoData*> photo,
		const QString &savePath,
		const Data::FileOrigin &origin) {
	auto task = DownloadTask();
	task.id = allocateNextId();
	task.source = DownloadSource::Photo;
	task.fileName = QString::number(photo->id) + u".jpg"_q;
	task.savePath = savePath;
	task.mimeType = SourceMimeType(DownloadSource::Photo);
	if (const auto size = photo->size(PhotoSize::Large)) {
		task.totalSize = size->width() * size->height() * 4;
	}
	task.addedAt = QDateTime::currentDateTime();
	task.origin = origin;
	if (const auto msgOrigin
		= std::get_if<Data::FileOriginMessage>(&origin.data)) {
		task.peerId = msgOrigin->peer;
		task.itemId = *msgOrigin;
	}
	const auto id = task.id;
	task.state = (activeDownloadCount() >= kMaxParallelDownloads)
		? DownloadState::Waiting
		: DownloadState::Queued;
	_tasks.emplace(id, std::move(task));
	_taskAdded.fire_copy(id);
	scheduleSave();
	if (task.state == DownloadState::Queued) {
		startTask(id);
	}
	return id;
}
void DownloadCenter::pause(DownloadTaskId id) {
	auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		LOG(("DLC: pause id=%1 NOT_FOUND").arg(id));
		return;
	}
	LOG(("DLC: pause id=%1 state=%2").arg(id).arg(int(it->second.state)));
	if (it->second.state != DownloadState::Downloading
		&& it->second.state != DownloadState::Queued) {
		return;
	}
	const auto controllerIt = _controllers.find(id);
	if (controllerIt != _controllers.end()) {
		LOG(("DLC: pause id=%1 has_controller, calling controller->pause").arg(id));
		controllerIt->second->pause();
		LOG(("DLC: pause id=%1 controller->pause returned").arg(id));
	} else {
		LOG(("DLC: pause id=%1 NO_CONTROLLER, stopTask").arg(id));
		stopTask(id);
	}
	setState(id, DownloadState::Paused);
	LOG(("DLC: pause id=%1 done, state=Paused").arg(id));
	// Pausing does not free headroom (the task is still around as
	// Paused, but if a Waiting task exists we can let it start now
	// because Paused does not hold a controller slot). Actually we
	// keep the current behavior: only onControllerFinished/cancel/
	// remove promoteWaiting(); pausing alone never opens a slot.
}
void DownloadCenter::resume(DownloadTaskId id) {
	auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		LOG(("DLC: resume id=%1 NOT_FOUND").arg(id));
		return;
	}
	LOG(("DLC: resume id=%1 state=%2").arg(id).arg(int(it->second.state)));
	const auto state = it->second.state;
	if (state != DownloadState::Paused
		&& state != DownloadState::Failed
		&& state != DownloadState::Queued) {
		return;
	}
	const auto controllerIt = _controllers.find(id);
	if (controllerIt != _controllers.end() && state == DownloadState::Paused) {
		LOG(("DLC: resume id=%1 has_controller, calling controller->start").arg(id));
		const auto weak = base::make_weak(this);
		controllerIt->second->start(
			[=](bool ok, const QString &error) {
				LOG(("DLC: resume id=%1 onFinished ok=%2 err=%3").arg(id).arg(ok).arg(error));
				crl::on_main([=] {
					if (const auto strong = weak.get()) {
						strong->onControllerFinished(id, ok, error);
					}
				});
			},
			[=](int64 ready, int64 total) {
							crl::on_main([=] {
					if (const auto strong = weak.get()) {
						strong->onControllerProgress(id, ready, total);
					}
				});
			});
		LOG(("DLC: resume id=%1 controller->start returned").arg(id));
		setState(id, DownloadState::Downloading);
		LOG(("DLC: resume id=%1 done, state=Downloading").arg(id));
	} else {
		LOG(("DLC: resume id=%1 NO_CONTROLLER or QUEUED, startTask").arg(id));
		if (state != DownloadState::Queued) {
			setState(id, DownloadState::Queued);
		}
		startTask(id);
	}
}
void DownloadCenter::cancel(DownloadTaskId id) {
	auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		return;
	}
	const auto savePath = it->second.savePath;
	stopTask(id);
	// Cancel = abandon the partial progress, so explicitly drop the
	// temp file. (Pause keeps it so resume can continue; Remove does
	// the same explicit cleanup, see ::remove().)
	if (!savePath.isEmpty()) {
		const auto baseInfo = QFileInfo(savePath);
		const auto baseDir = baseInfo.absolutePath();
		const auto baseName = baseInfo.fileName();
		if (!baseDir.isEmpty() && !baseName.isEmpty()) {
			// New design: a single "<savePath>.part.tmp".
			const auto partFile = QDir(baseDir).absoluteFilePath(
				baseName + u".part.tmp"_q);
			if (QFile::exists(partFile)) {
				QFile::remove(partFile);
			}
			// Migration: also clean up any leftover per-chunk files
			// from a previous version of the code.
			QDir dir(baseDir);
			const auto entries = dir.entryInfoList(
				QStringList(u"%1.part*.tmp"_q.arg(baseName)),
				QDir::Files);
			for (const auto &entry : entries) {
				QFile::remove(entry.absoluteFilePath());
			}
		}
	}
	setState(id, DownloadState::Cancelled);
	// Cancellation frees a concurrency slot; promote the next waiting
	// task to Downloading if there is one.
	promoteWaiting();
}
void DownloadCenter::retry(DownloadTaskId id) {
	auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		return;
	}
	if (it->second.state != DownloadState::Failed) {
		return;
	}
	it->second.readySize = 0;
	it->second.errorMessage.clear();
	setState(id, DownloadState::Queued);
	startTask(id);
}
void DownloadCenter::remove(DownloadTaskId id) {
	LOG(("DLC: remove id=%1 enter").arg(id));
	auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		LOG(("DLC: remove id=%1 NOT_FOUND").arg(id));
		return;
	}
	const auto savePath = it->second.savePath;
	const auto stateBefore = it->second.state;
	LOG(("DLC: remove id=%1 state=%2 savePath=%3").arg(id).arg(int(stateBefore)).arg(savePath));
	if (it->second.state == DownloadState::Downloading) {
		LOG(("DLC: remove id=%1 stopTask").arg(id));
		stopTask(id);
		LOG(("DLC: remove id=%1 stopTask done").arg(id));
	}
	LOG(("DLC: remove id=%1 _tasks.erase").arg(id));
	_tasks.erase(it);
	LOG(("DLC: remove id=%1 _controllers.erase").arg(id));
	_controllers.erase(id);
	if (!savePath.isEmpty()) {
		const auto baseInfo = QFileInfo(savePath);
		const auto baseDir = baseInfo.absolutePath();
		const auto baseName = baseInfo.fileName();
		if (!baseDir.isEmpty() && !baseName.isEmpty()) {
			// New design: a single "<savePath>.part.tmp".
			const auto partFile = QDir(baseDir).absoluteFilePath(
				baseName + u".part.tmp"_q);
			if (QFile::exists(partFile)) {
				QFile::remove(partFile);
			}
			// Migration: also clean up any leftover per-chunk files
			// from a previous version of the code.
			QDir dir(baseDir);
			const auto entries = dir.entryInfoList(
				QStringList(u"%1.part*.tmp"_q.arg(baseName)),
				QDir::Files);
			LOG(("DLC: remove id=%1 cleanup temp files count=%2").arg(id).arg(entries.size()));
			for (const auto &entry : entries) {
				QFile::remove(entry.absoluteFilePath());
			}
		}
	}
	_taskRemoved.fire_copy(id);
	scheduleSave();
	promoteWaiting();
	LOG(("DLC: remove id=%1 done").arg(id));
}

// removeToTrash: keep the on-disk temp files in place but mark the
// task as Removed so the UI can show it in the trash tab. The user
// can restore or permanently delete later.
void DownloadCenter::removeToTrash(DownloadTaskId id) {
	auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		return;
	}
	if (it->second.state == DownloadState::Downloading) {
		stopTask(id);
	}
	setState(id, DownloadState::Removed);
	promoteWaiting();
}

// removeForever: hard-delete the task and its temp files. Mirrors
// the cleanup that remove() already does for active downloads.
void DownloadCenter::removeForever(DownloadTaskId id) {
	remove(id);
}

std::optional<DownloadTaskId> DownloadCenter::oldestWaitingTask() const {
	std::optional<DownloadTaskId> best;
	QDateTime bestAdded;
	for (const auto &[id, task] : _tasks) {
		if (task.state != DownloadState::Waiting) continue;
		if (!best.has_value() || task.addedAt < bestAdded) {
			best = id;
			bestAdded = task.addedAt;
		}
	}
	return best;
}

int DownloadCenter::activeDownloadCount() const {
	auto n = 0;
	for (const auto &[id, task] : _tasks) {
		if (task.state == DownloadState::Downloading) ++n;
	}
	return n;
}

void DownloadCenter::promoteWaiting() {
	if (activeDownloadCount() >= kMaxParallelDownloads) {
		return;
	}
	const auto next = oldestWaitingTask();
	if (!next.has_value()) {
		return;
	}
	const auto id = *next;
	LOG(("DLC: promoteWaiting id=%1").arg(id));
	// Move from Waiting → Queued, then startTask will flip it to
	// Downloading once the controller is inserted.
	setState(id, DownloadState::Queued);
	startTask(id);
}
void DownloadCenter::openFile(DownloadTaskId id) {
	const auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		return;
	}
	if (it->second.state != DownloadState::Completed) {
		return;
	}
	const auto path = it->second.savedAbsolutePath.isEmpty()
		? it->second.savePath
		: it->second.savedAbsolutePath;
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
void DownloadCenter::showInFolder(DownloadTaskId id) {
	const auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		return;
	}
	const auto &task = it->second;
	// Prefer the on-disk path for completed downloads; fall back to the
	// destination the user picked at add-time, which may point at a file
	// that hasn't materialised yet (still in progress / queued).
	QString path = task.savedAbsolutePath;
	if (path.isEmpty()) path = task.savePath;

	QString openDir;
	bool selectFile = false;
	if (!path.isEmpty()) {
		QFileInfo info(path);
		if (info.exists()) {
			// File is on disk — point Explorer at it with /select.
			openDir = info.absolutePath();
			selectFile = true;
		} else if (QFile::exists(info.absolutePath())) {
			// Destination folder exists but the file isn't there yet —
			// open the folder so the user can see where it will land.
			openDir = info.absolutePath();
		} else {
			// Neither file nor parent directory exist. Falling back to
			// the absoluteFilePath() of "" returns the process CWD, which
			// on Windows often resolves to the user's Documents folder —
			// confusing; ignore it instead.
		}
	}
	if (openDir.isEmpty()) {
		// No usable per-task path (legacy data, or task came from a flow
		// that left savePath empty). At least open the Telegram downloads
		// base directory so something useful shows up.
		openDir = storagePath();
	}
	if (openDir.isEmpty()) return;

#if defined Q_OS_WIN
	// Bypass the shell command-line entirely — it mangles Unicode paths
	// (and sometimes spaces) leading Explorer to fall back to Documents.
	// Use the Win32 Shell API directly via PIDLs, which are UTF-16 native
	// and immune to shell parsing.
	const auto native = selectFile
		? QDir::toNativeSeparators(path)
		: QDir::toNativeSeparators(openDir);
	const std::wstring wpath = native.toStdWString();
	PIDLIST_ABSOLUTE pidlItem = ILCreateFromPathW(wpath.c_str());
	if (pidlItem) {
		if (selectFile) {
			// File pidl → clone and strip last component to get the
			// parent folder PIDL; pass the file PIDL as the item to
			// select, so Explorer highlights exactly this file.
			PIDLIST_ABSOLUTE pidlFolder = ILClone(pidlItem);
			if (pidlFolder) {
				ILRemoveLastID(pidlFolder);
				LPCITEMIDLIST items[] = { pidlItem };
				SHOpenFolderAndSelectItems(
					pidlFolder,
					1,
					items,
					0);
				ILFree(pidlFolder);
			}
		} else {
			// Folder pidl — opening the containing folder of the file
			// pointed to by selectFile isn't applicable here; openDir
			// is already a directory.
			SHOpenFolderAndSelectItems(pidlItem, 0, nullptr, 0);
		}
		ILFree(pidlItem);
	} else {
		// ILCreateFromPathW failed — usually means the path is malformed
		// or reaches outside the namespace. Fall through to the parent
		// folder via QDesktopServices.
		LOG(("DLC: showInFolder id=%1 ILCreateFromPathW FAILED path=%2")
			.arg(id).arg(native));
		if (!selectFile && !openDir.isEmpty()) {
			QDesktopServices::openUrl(QUrl::fromLocalFile(openDir));
		}
	}
#elif defined Q_OS_MAC
	if (selectFile) {
		QProcess::startDetached(u"/usr/bin/open"_q, { u"-R"_q, path });
	} else {
		QProcess::startDetached(u"/usr/bin/open"_q, { openDir });
	}
#else
	QProcess::startDetached(u"xdg-open"_q, { openDir });
#endif
}
const DownloadTask *DownloadCenter::task(DownloadTaskId id) const {
	const auto it = _tasks.find(id);
	return (it == _tasks.end()) ? nullptr : &it->second;
}
DownloadTask *DownloadCenter::task(DownloadTaskId id) {
	const auto it = _tasks.find(id);
	return (it == _tasks.end()) ? nullptr : &it->second;
}
auto DownloadCenter::tasks() const
-> ranges::any_view<const DownloadTask*, ranges::category::input> {
	return _tasks | ranges::views::values
		| ranges::views::transform([](const DownloadTask &task) {
			return &task;
		});
}
int DownloadCenter::countByState(DownloadState state) const {
	auto result = 0;
	for (const auto &[id, task] : _tasks) {
		if (task.state == state) {
			++result;
		}
	}
	return result;
}
int DownloadCenter::totalCount() const {
	return int(_tasks.size());
}

DownloadStats DownloadCenter::computeStats() const {
	auto stats = DownloadStats();
	const auto monthStart = QDate(
		QDate::currentDate().year(),
		QDate::currentDate().month(),
		1).startOfDay();
	for (const auto &[id, task] : _tasks) {
		switch (task.state) {
		case DownloadState::Downloading: ++stats.activeCount; break;
		case DownloadState::Waiting:      ++stats.waitingCount; break;
		case DownloadState::Paused:       ++stats.pausedCount; break;
		case DownloadState::Completed:
			++stats.completedCount;
			if (task.completedAt.isValid()
					&& task.completedAt >= monthStart) {
				stats.totalDownloadedThisMonth += task.totalSize;
			}
			break;
		case DownloadState::Failed:       ++stats.failedCount; break;
		case DownloadState::Removed:      ++stats.removedCount; break;
		default: break;
		}
	}
	// downloadBps = sum of the most recent sample across active tasks.
	for (const auto &[id, task] : _tasks) {
		if (task.state != DownloadState::Downloading) continue;
		if (!task.speedHistory60s.empty()) {
			stats.downloadBps += task.speedHistory60s.back();
		}
	}
	return stats;
}

void DownloadCenter::updateSpeedSample(DownloadTaskId id, int64 bps) {
	auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		return;
	}
	auto &ring = it->second.speedHistory60s;
	if (ring.size() >= 60) {
		ring.pop_front();
	}
	ring.push_back(bps);
}

void DownloadCenter::startSpeedSampler() {
	// 1 Hz tick on the main thread. Cheap iteration over active tasks
	// only, so the cost is dominated by the controller's read; we do
	// not lock here.
	if (!_speedTimer.isActive()) {
		_speedTimer.callEach(1000);
	}
}

void DownloadCenter::stopSpeedSampler() {
	if (_speedTimer.isActive()) {
		_speedTimer.cancel();
	}
}

void DownloadCenter::speedTimerTick() {
	// Iterate active tasks and append one speed sample per second.
	// We track the previous total ready-bytes across ticks; if the
	// controller reports fresh progress, we use that delta. Otherwise
	// we fall back to the last known bps (typically 0 if idle).
	for (const auto &[id, task] : _tasks) {
		if (task.state != DownloadState::Downloading) {
			_lastProgressBytes.remove(id);
			continue;
		}
		auto it = _lastProgressBytes.find(id);
		const auto lastBytes = (it != _lastProgressBytes.end())
			? it->second
			: task.readySize;
		const auto delta = (task.readySize > lastBytes)
			? (task.readySize - lastBytes)
			: 0;
		_lastProgressBytes[id] = task.readySize;
		updateSpeedSample(id, delta);
	}
	_taskUpdated.fire({});
	scheduleSave();
}
void DownloadCenter::refreshTaskInfo(DownloadTaskId id) {
	LOG(("DLC: refreshTaskInfo id=%1 enter").arg(id));
	const auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		LOG(("DLC: refreshTaskInfo id=%1 NOT_FOUND").arg(id));
		return;
	}
	refreshTaskFileReference(id, [=](bool ok) {
		LOG(("DLC: refreshTaskInfo id=%1 done ok=%2").arg(id).arg(ok));
		// Re-fire so the UI re-resolves the document (now populated) and
		// picks up the thumbnail on the next refresh window.
		const auto strong = base::make_weak(this).get();
		if (strong) {
			strong->_taskUpdated.fire_copy(id);
		}
	});
}
void DownloadCenter::startTask(DownloadTaskId id) {
	LOG(("DLC: startTask id=%1 enter").arg(id));
	const auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		LOG(("DLC: startTask id=%1 NOT_FOUND").arg(id));
		return;
	}
	if (it->second.state != DownloadState::Queued) {
		LOG(("DLC: startTask id=%1 wrong_state=%2").arg(id).arg(int(it->second.state)));
		return;
	}
	if (_controllers.contains(id)) {
		LOG(("DLC: startTask id=%1 has_controller already").arg(id));
		return;
	}

	// Refresh the file_reference from the source chat before we create
	// the controller: the persisted reference saved at task creation may
	// have been invalidated by the server (FILE_REFERENCE_EXPIRED). The
	// refresh is best-effort: if it fails (e.g. the user left the channel
	// or the message was deleted) we still proceed and the chunk loader
	// will surface a clear error.
	refreshTaskFileReference(id, [=](bool ok) {
		startTaskAfterRefresh(id);
	});
}
void DownloadCenter::startTaskAfterRefresh(DownloadTaskId id) {
	LOG(("DLC: startTaskAfterRefresh id=%1 enter").arg(id));
	const auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		LOG(("DLC: startTaskAfterRefresh id=%1 NOT_FOUND").arg(id));
		return;
	}
	if (it->second.state != DownloadState::Queued) {
		LOG(("DLC: startTaskAfterRefresh id=%1 wrong_state=%2").arg(id).arg(int(it->second.state)));
		return;
	}
	if (_controllers.contains(id)) {
		LOG(("DLC: startTaskAfterRefresh id=%1 has_controller already").arg(id));
		return;
	}

	// If totalSize was never refreshed (e.g. the user quit before the
		// server reported the real file size), pick it up from DocumentData
		// now so the chunk loader doesn't fire Expects(loadSize > 0).
		if (it->second.totalSize <= 0
				&& it->second.source == DownloadSource::Document) {
			auto &owner = _session->data();
			const auto document = owner.document(it->second.documentId);
			if (document && document->size > 0) {
				LOG(("DLC: startTask id=%1 refreshing totalSize from %2 to %3")
					.arg(id)
					.arg(it->second.totalSize)
					.arg(document->size));
				it->second.totalSize = document->size;
				_taskUpdated.fire_copy(id);
				scheduleSave();
			}
		}
		if (it->second.totalSize <= 0) {
			LOG(("DLC: startTask id=%1 totalSize still unknown, failing").arg(id));
			setState(id, DownloadState::Failed,
				u"File size unavailable, please retry"_q);
			return;
		}

		// Restore DocumentData's remote location from the persisted task if
		// the in-memory copy hasn't been populated yet (e.g. on a fresh
		// restart before the user opened the source chat). Without this
		// setRemoteLocation, isNull() returns true and createFileLoaderForParallel
		// asserts Expects(!isNull()).
		if (it->second.source == DownloadSource::Document) {
			auto &owner = _session->data();
			const auto document = owner.document(it->second.documentId);
			if (document
					&& document->isNull()
					&& !it->second.fileReference.isEmpty()) {
				LOG(("DLC: startTask id=%1 restoring document remote location").arg(id));
				document->setRemoteLocation(
					it->second.downloadDcId,
					it->second.accessHash,
					it->second.fileReference);
			}
		}
	const auto tempFilePath = QFileInfo(it->second.savePath).absoluteFilePath()
			+ u".part.tmp"_q;
	auto args = Storage::ParallelDownloadController::Args{
			.session = _session,
			.origin = it->second.origin,
			.toFile = it->second.savePath,
			.tempFilePath = tempFilePath,
			.fullSize = it->second.totalSize,
			.config = {
				.chunks = it->second.parallelChunks,
				.chunkSize = it->second.chunkSize,
			},
			.initialChunks = it->second.chunks,
		};
		if (it->second.source == DownloadSource::Document) {
					auto &owner = _session->data();
					const auto document = owner.document(it->second.documentId);
					args.document = document;
				}
				if (!args.document) {
					LOG(("DLC: startTask id=%1 no_document").arg(id));
					setState(id, DownloadState::Failed,
						u"Document unavailable, please retry"_q);
					return;
				}
				if (args.document->isNull()) {
					LOG(("DLC: startTask id=%1 document isNull, failing").arg(id));
					setState(id, DownloadState::Failed,
						u"Document unavailable, please retry"_q);
					return;
				}
		auto controller = std::make_unique<Storage::ParallelDownloadController>(
			std::move(args));
		if (!controller->startable()) {
			LOG(("DLC: startTask id=%1 not_startable").arg(id));
			return;
		}
		const auto weak = base::make_weak(this);
		controller->start(
			[=](bool ok, const QString &error) {
				LOG(("DLC: startTask id=%1 controller_onFinished ok=%2").arg(id).arg(ok));
				crl::on_main([=] {
					if (const auto strong = weak.get()) {
						strong->onControllerFinished(id, ok, error);
					}
				});
			},
			[=](int64 ready, int64 total) {
				crl::on_main([=] {
					if (const auto strong = weak.get()) {
						strong->onControllerProgress(id, ready, total);
					}
				});
			});
		_controllers.insert({ id, std::move(controller) });
		setState(id, DownloadState::Downloading);
		LOG(("DLC: startTask id=%1 controller inserted, state=Downloading").arg(id));
}
void DownloadCenter::refreshTaskFileReference(
		DownloadTaskId id,
		Fn<void(bool ok)> done) {
	auto &data = _session->data();
	const auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		LOG(("DLC: refreshFileRef id=%1 NOT_FOUND").arg(id));
		done(false);
		return;
	}
	const auto fullId = it->second.itemId;
	if (!fullId.peer || !fullId.msg) {
		LOG(("DLC: refreshFileRef id=%1 no_origin_info peer=%2 msg=%3")
			.arg(id).arg(fullId.peer.value).arg(fullId.msg.bare));
		done(true);
		return;
	}
	const auto peer = data.peerLoaded(fullId.peer);
	if (peer) {
		LOG(("DLC: refreshFileRef id=%1 peer_loaded peerId=%2 msgId=%3")
			.arg(id).arg(fullId.peer.value).arg(fullId.msg.bare));
		requestMessageAndUpdateTask(id, peer, std::move(done));
		return;
	}
	if (peerIsChannel(fullId.peer)) {
		const auto channelId = fullId.peer.to<ChannelId>();
		LOG(("DLC: refreshFileRef id=%1 channel_not_loaded channelId=%2, fetching")
			.arg(id).arg(channelId.bare));
		fetchChannelThenRefresh(id, channelId, std::move(done));
		return;
	}
	// Non-channel peer (private chat / self): requestMessageData with
	// nullptr peer falls through to messages.getMessages which works for
	// private chats without needing the peer in memory.
	LOG(("DLC: refreshFileRef id=%1 non_channel peer").arg(id));
	requestMessageAndUpdateTask(id, nullptr, std::move(done));
}
void DownloadCenter::fetchChannelThenRefresh(
		DownloadTaskId id,
		ChannelId channelId,
		Fn<void(bool ok)> done) {
	const auto weak = base::make_weak(this);
	_session->api().request(MTPchannels_GetChannels(
		MTP_vector<MTPInputChannel>(1,
			MTP_inputChannel(MTP_long(channelId.bare), MTP_long(0)))
	)).done([=](const MTPmessages_Chats &result) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		const auto fetched = strong->_session->data().processChats(
			result.match([](const auto &d) { return d.vchats(); }));
		if (!fetched) {
			LOG(("DLC: refreshFileRef id=%1 channel_getChannels returned no peer")
				.arg(id));
			done(false);
			return;
		}
		const auto it = strong->_tasks.find(id);
		if (it == strong->_tasks.end() || !fetched->id
				|| fetched->id != it->second.itemId.peer) {
			LOG(("DLC: refreshFileRef id=%1 fetched peer mismatch").arg(id));
			done(true);
			return;
		}
		strong->requestMessageAndUpdateTask(id, fetched, std::move(done));
	}).fail([=] {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		LOG(("DLC: refreshFileRef id=%1 channels.GetChannels failed").arg(id));
		done(false);
	}).send();
}
void DownloadCenter::requestMessageAndUpdateTask(
		DownloadTaskId id,
		PeerData *peer,
		Fn<void(bool ok)> done) {
	const auto weak = base::make_weak(this);
	const auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		done(false);
		return;
	}
	const auto msgId = it->second.itemId.msg;
	_session->api().requestMessageData(peer, msgId, [=] {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		const auto it = strong->_tasks.find(id);
		if (it == strong->_tasks.end()) {
			done(false);
			return;
		}
		bool changed = false;
		if (it->second.source == DownloadSource::Document) {
			const auto doc = strong->_session->data().document(
				it->second.documentId);
			if (doc && !doc->isNull()) {
				const auto newFref = doc->remoteFileReference();
				const auto newAccess = doc->remoteAccessHash();
				const auto newDc = doc->remoteDcId();
				if (newFref != it->second.fileReference) {
					LOG(("DLC: refreshFileRef id=%1 fref updated %2 -> %3")
						.arg(id)
						.arg(it->second.fileReference.size())
						.arg(newFref.size()));
					it->second.fileReference = newFref;
					changed = true;
				}
				if (newAccess != it->second.accessHash) {
					it->second.accessHash = newAccess;
					changed = true;
				}
				if (newDc != it->second.downloadDcId) {
					it->second.downloadDcId = newDc;
					changed = true;
				}
			}
		} else if (it->second.source == DownloadSource::Photo) {
			const auto photo = strong->_session->data().photo(
				it->second.documentId);
			if (photo && !photo->isNull()) {
				const auto newFref = photo->remoteFileReference();
				const auto newAccess = photo->remoteAccessHash();
				const auto newDc = photo->remoteDcId();
				if (newFref != it->second.fileReference) {
					it->second.fileReference = newFref;
					changed = true;
				}
				if (newAccess != it->second.accessHash) {
					it->second.accessHash = newAccess;
					changed = true;
				}
				if (newDc != it->second.downloadDcId) {
					it->second.downloadDcId = newDc;
					changed = true;
				}
			}
		}
		if (changed) {
			strong->_taskUpdated.fire_copy(id);
			strong->scheduleSave();
		}
		LOG(("DLC: refreshFileRef id=%1 done changed=%2").arg(id)
			.arg(changed ? "yes" : "no"));
		done(true);
	});
}

void DownloadCenter::stopTask(DownloadTaskId id) {
	LOG(("DLC: stopTask id=%1").arg(id));
	const auto it = _controllers.find(id);
	if (it != _controllers.end()) {
		LOG(("DLC: stopTask id=%1 calling controller->stop").arg(id));
		it->second->stop();
		LOG(("DLC: stopTask id=%1 controller->stop returned").arg(id));
	}
	LOG(("DLC: stopTask id=%1 erasing controller").arg(id));
	_controllers.erase(id);
	LOG(("DLC: stopTask id=%1 done").arg(id));
}
void DownloadCenter::onControllerFinished(
		DownloadTaskId id,
		bool ok,
		const QString &error) {
	LOG(("DLC: onControllerFinished id=%1 ok=%2 err=%3").arg(id).arg(ok).arg(error));
	const auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		LOG(("DLC: onControllerFinished id=%1 NOT_FOUND").arg(id));
		return;
	}
	LOG(("DLC: onControllerFinished id=%1 erase_controller").arg(id));
	_controllers.erase(id);
	if (ok) {
		it->second.savedAbsolutePath = it->second.savePath;
		setState(id, DownloadState::Completed);
	} else {
		setState(id, DownloadState::Failed, error);
	}
	// Concurrency headroom just opened: try to promote the oldest
	// Waiting task to Downloading.
	promoteWaiting();
	LOG(("DLC: onControllerFinished id=%1 done").arg(id));
}
void DownloadCenter::onControllerProgress(
		DownloadTaskId id,
		int64 ready,
		int64 total) {
	const auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		return;
	}
	if (total > 0) {
		it->second.totalSize = total;
	}
	it->second.readySize = ready;
	// Snapshot the controller's per-chunk state so a future restart
	// can resume the same temp files at the same offsets.
	const auto controllerIt = _controllers.find(id);
	if (controllerIt != _controllers.end()) {
		it->second.chunks = controllerIt->second->chunkStates();
	}
	_taskUpdated.fire_copy(id);
	scheduleSave();
}
void DownloadCenter::setState(
		DownloadTaskId id,
		DownloadState state,
		const QString &error) {
	const auto it = _tasks.find(id);
	if (it == _tasks.end()) {
		return;
	}
	it->second.state = state;
	if (!error.isEmpty()) {
		it->second.errorMessage = error;
	} else if (state != DownloadState::Failed) {
		it->second.errorMessage.clear();
	}
	if (state == DownloadState::Completed
		|| state == DownloadState::Cancelled
		|| state == DownloadState::Failed) {
		it->second.completedAt = QDateTime::currentDateTime();
	}
	_taskUpdated.fire_copy(id);
	scheduleSave();
}
void DownloadCenter::scheduleSave() {
	if (_saveTimer.isActive()) {
		_saveTimer.cancel();
	}
	_saveTimer.callEach(kSaveDebounceMs);
}
namespace {
[[nodiscard]] QJsonValue SerializeTaskToJson(const DownloadTask &task) {
	auto object = QJsonObject();
	object.insert(u"id"_q, qint64(task.id));
	object.insert(u"state"_q, SerializeState(task.state));
	object.insert(u"fileName"_q, task.fileName);
	object.insert(u"savePath"_q, task.savePath);
	object.insert(u"totalSize"_q, qint64(task.totalSize));
	object.insert(u"readySize"_q, qint64(task.readySize));
	object.insert(u"mimeType"_q, task.mimeType);
	object.insert(u"source"_q, SerializeSource(task.source));
	object.insert(u"documentId"_q, qint64(task.documentId));
	object.insert(u"peerId"_q, qint64(task.peerId.value));
	object.insert(u"itemMsg"_q, qint64(task.itemId.msg.bare));
	object.insert(u"accessHash"_q, qint64(task.accessHash));
	object.insert(u"fileReference"_q, QString::fromLatin1(
		task.fileReference.toBase64()));
	object.insert(u"downloadDcId"_q, task.downloadDcId);
	object.insert(u"parallelChunks"_q, task.parallelChunks);
	object.insert(u"chunkSize"_q, qint64(task.chunkSize));
	object.insert(u"errorMessage"_q, task.errorMessage);
	object.insert(u"savedAbsolutePath"_q, task.savedAbsolutePath);
	if (task.addedAt.isValid()) {
		object.insert(u"addedAt"_q, task.addedAt.toString(Qt::ISODate));
	}
	if (task.completedAt.isValid()) {
		object.insert(u"completedAt"_q,
			task.completedAt.toString(Qt::ISODate));
	}
	if (task.startedAt.isValid()) {
		object.insert(u"startedAt"_q,
			task.startedAt.toString(Qt::ISODate));
	}
	if (!task.sha256.isEmpty()) {
		object.insert(u"sha256"_q, task.sha256);
	}
	if (!task.speedHistory60s.empty()) {
		auto speedArr = QJsonArray();
		for (const auto sample : task.speedHistory60s) {
			speedArr.append(qint64(sample));
		}
		object.insert(u"speedHistory60s"_q, speedArr);
	}
	auto chunks = QJsonArray();
	for (const auto &chunk : task.chunks) {
		auto chunkObj = QJsonObject();
		chunkObj.insert(u"startOffset"_q, qint64(chunk.startOffset));
		chunkObj.insert(u"endOffset"_q, qint64(chunk.endOffset));
		chunkObj.insert(u"ready"_q, qint64(chunk.ready));
		chunkObj.insert(u"finished"_q, chunk.finished);
		chunks.append(chunkObj);
	}
	object.insert(u"chunks"_q, chunks);
	return object;
}
[[nodiscard]] std::optional<DownloadTask> DeserializeTaskFromJson(
		const QJsonObject &object) {
	auto task = DownloadTask();
	const auto id = object.value(u"id"_q).toVariant().toLongLong();
	if (id <= 0) {
		return std::nullopt;
	}
	task.id = DownloadTaskId(id);
	const auto stateRaw = object.value(u"state"_q).toString();
	const auto state = DeserializeState(stateRaw);
	if (!state) {
		// Forward-compat fallback: an unknown state value (e.g. a
		// state added in a newer build) is mapped to Failed with a
		// single log line, so the rest of the file still loads.
		LOG(("DLC: unknown DownloadState value '%1' in JSON, "
			"mapping to Failed").arg(stateRaw));
		task.state = DownloadState::Failed;
	} else {
		task.state = *state;
	}
	task.fileName = object.value(u"fileName"_q).toString();
	task.savePath = object.value(u"savePath"_q).toString();
	task.totalSize = object.value(u"totalSize"_q).toVariant().toLongLong();
	task.readySize = object.value(u"readySize"_q).toVariant().toLongLong();
	task.mimeType = object.value(u"mimeType"_q).toString();
	const auto source = DeserializeSource(
		object.value(u"source"_q).toString());
	task.source = source.value_or(DownloadSource::Document);
	task.documentId = DocumentId(
		object.value(u"documentId"_q).toVariant().toLongLong());
	const auto peerBare = object.value(u"peerId"_q).toVariant().toLongLong();
		task.peerId = PeerId{ PeerIdHelper{ BareId(peerBare) } };
	const auto itemMsg = object.value(u"itemMsg"_q).toVariant().toLongLong();
	task.itemId = FullMsgId{ task.peerId, MsgId(itemMsg) };
	task.accessHash = object.value(u"accessHash"_q).toVariant().toULongLong();
	const auto fileRefB64 = object.value(u"fileReference"_q).toString();
	task.fileReference = QByteArray::fromBase64(fileRefB64.toLatin1());
	task.downloadDcId = object.value(u"downloadDcId"_q).toInt();
	task.parallelChunks = object.value(u"parallelChunks"_q).toInt(
		DownloadCenter::RecommendedParallelChunks());
	task.chunkSize = object.value(u"chunkSize"_q).toVariant().toLongLong();
	if (task.chunkSize <= 0) {
		task.chunkSize = 128 * 1024;
	}
	task.errorMessage = object.value(u"errorMessage"_q).toString();
	task.savedAbsolutePath = object.value(u"savedAbsolutePath"_q).toString();
	const auto addedAt = object.value(u"addedAt"_q).toString();
	if (!addedAt.isEmpty()) {
		task.addedAt = QDateTime::fromString(addedAt, Qt::ISODate);
	}
	const auto completedAt = object.value(u"completedAt"_q).toString();
	if (!completedAt.isEmpty()) {
		task.completedAt = QDateTime::fromString(completedAt, Qt::ISODate);
	}
	const auto startedAt = object.value(u"startedAt"_q).toString();
	if (!startedAt.isEmpty()) {
		task.startedAt = QDateTime::fromString(startedAt, Qt::ISODate);
	}
	task.sha256 = object.value(u"sha256"_q).toString();
	const auto speedValue = object.value(u"speedHistory60s"_q);
	if (speedValue.isArray()) {
		const auto speedArr = speedValue.toArray();
		task.speedHistory60s.clear();
		const auto cap = std::min<int>(speedArr.size(), 60);
		while (int(task.speedHistory60s.size()) < cap) {
			// reserve() not available on std::deque; grow on demand.
			task.speedHistory60s.push_back(0);
		}
		task.speedHistory60s.clear();
		for (const auto &v : speedArr) {
			task.speedHistory60s.push_back(
				v.toVariant().toLongLong());
			if (task.speedHistory60s.size() >= 60) break;
		}
	}
	const auto chunksValue = object.value(u"chunks"_q);
	if (chunksValue.isArray()) {
		const auto chunksArr = chunksValue.toArray();
		task.chunks.reserve(chunksArr.size());
		for (const auto &value : chunksArr) {
			const auto obj = value.toObject();
			auto chunk = Storage::ChunkState();
			// Old JSON entries may still carry a "tempFilePath" field
			// (per-chunk files from the previous design). We just
			// ignore it - the new code uses a single shared temp file
			// derived from savePath.
			chunk.startOffset = obj.value(u"startOffset"_q).toVariant().toLongLong();
			chunk.endOffset = obj.value(u"endOffset"_q).toVariant().toLongLong();
			chunk.ready = obj.value(u"ready"_q).toVariant().toLongLong();
			chunk.finished = obj.value(u"finished"_q).toBool();
			if (chunk.endOffset > chunk.startOffset) {
				task.chunks.push_back(std::move(chunk));
			}
		}
	}
	return task;
}
} // namespace
DownloadCenter::Snapshot DownloadCenter::buildSnapshot() const {
	auto snapshot = Snapshot();
	snapshot.nextId = _nextId;
	snapshot.tasks.reserve(_tasks.size());
	for (const auto &[id, task] : _tasks) {
		snapshot.tasks.push_back(task);
	}
	return snapshot;
}
void DownloadCenter::restoreSnapshot(const Snapshot &snapshot) {
	_tasks.clear();
	_controllers.clear();
	_nextId = snapshot.nextId;
	for (auto task : snapshot.tasks) {
		// Preserve Completed / Cancelled / Failed / Paused as-is so users
		// can see the correct state and resume work. Only Downloading
		// tasks need to be downgraded because their in-flight chunk
		// requests cannot be reliably resumed across a restart.
		if (task.state == DownloadState::Downloading) {
			task.state = DownloadState::Paused;
		}
		const auto id = task.id;
		_tasks.emplace(id, std::move(task));
	}
	_tasksReloaded.fire({});
	for (const auto &[id, task] : _tasks) {
		_taskUpdated.fire_copy(id);
	}
}
void DownloadCenter::writeSnapshot(const Snapshot &snapshot) {
	auto root = QJsonObject();
	root.insert(u"version"_q, kFormatVersion);
	root.insert(u"nextId"_q, qint64(snapshot.nextId));
	auto array = QJsonArray();
	for (const auto &task : snapshot.tasks) {
		array.append(SerializeTaskToJson(task));
	}
	root.insert(u"tasks"_q, array);
	const auto path = storagePath();
	auto dir = QFileInfo(path).dir();
	if (!dir.exists() && !dir.mkpath(u"."_q)) {
		return;
	}
	// Atomic save: QSaveFile writes to "<path>.<random>.tmp" inside the
	// same directory and only renames over the target on commit(). If
	// the process is killed mid-write, the previous downloads.json is
	// intact. (Same-directory rename is atomic on NTFS and POSIX.)
	auto file = QSaveFile(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return;
	}
	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	if (!file.commit()) {
		LOG(("DLC: writeSnapshot commit failed path=%1").arg(path));
	}
}
std::optional<DownloadCenter::Snapshot> DownloadCenter::readSnapshot() {
	auto file = QFile(storagePath());
	if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return std::nullopt;
	}
	const auto root = document.object();
	const auto version = root.value(u"version"_q).toInt();
	if (version != kFormatVersion) {
		return std::nullopt;
	}
	auto snapshot = Snapshot();
	snapshot.nextId = DownloadTaskId(
		root.value(u"nextId"_q).toVariant().toULongLong());
	if (snapshot.nextId == 0) {
		snapshot.nextId = 1;
	}
	const auto tasks = root.value(u"tasks"_q).toArray();
	snapshot.tasks.reserve(tasks.size());
	for (const auto &value : tasks) {
		const auto object = value.toObject();
		const auto task = DeserializeTaskFromJson(object);
		if (task) {
			snapshot.tasks.push_back(*task);
		}
	}
	return snapshot;
}
void DownloadCenter::saveToDisk() {
	const auto snapshot = buildSnapshot();
	writeSnapshot(snapshot);
}
void DownloadCenter::loadFromDisk(bool autoResumeQueued) {
	const auto snapshot = readSnapshot();
	if (!snapshot) {
		return;
	}
	restoreSnapshot(*snapshot);
	if (autoResumeQueued) {
		auto ids = std::vector<DownloadTaskId>();
		ids.reserve(_tasks.size());
		for (const auto &it : _tasks) {
			if (it.second.state == DownloadState::Queued) {
				ids.push_back(it.first);
			}
		}
		for (const auto id : ids) {
			LOG(("DLC: auto-resume queued task id=%1").arg(id));
			resume(id);
		}
	}
}
} // namespace Data
