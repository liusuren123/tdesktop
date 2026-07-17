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
#include "main/main_session.h"
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
namespace Data {
namespace {
constexpr auto kFormatVersion = 1;
constexpr auto kSaveDebounceMs = 500;
[[nodiscard]] QString SerializeState(DownloadState state) {
	switch (state) {
	case DownloadState::Queued: return u"queued"_q;
	case DownloadState::Downloading: return u"downloading"_q;
	case DownloadState::Paused: return u"paused"_q;
	case DownloadState::Completed: return u"completed"_q;
	case DownloadState::Failed: return u"failed"_q;
	case DownloadState::Cancelled: return u"cancelled"_q;
	}
	return u"queued"_q;
}
[[nodiscard]] std::optional<DownloadState> DeserializeState(
		const QString &value) {
	if (value == u"queued"_q) return DownloadState::Queued;
	if (value == u"downloading"_q) return DownloadState::Downloading;
	if (value == u"paused"_q) return DownloadState::Paused;
	if (value == u"completed"_q) return DownloadState::Completed;
	if (value == u"failed"_q) return DownloadState::Failed;
	if (value == u"cancelled"_q) return DownloadState::Cancelled;
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
, _saveTimer([=] { saveToDisk(); }) {
}
DownloadCenter::~DownloadCenter() {
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
	task.state = DownloadState::Queued;
	task.source = DownloadSource::Document;
	task.documentId = document->id;
	task.fileName = document->filename();
	task.savePath = savePath;
	task.mimeType = document->mimeString();
	task.totalSize = document->size;
	task.addedAt = QDateTime::currentDateTime();
	task.origin = origin;
	const auto id = task.id;
	_tasks.emplace(id, std::move(task));
	_taskAdded.fire_copy(id);
	scheduleSave();
	startTask(id);
	return id;
}
DownloadTaskId DownloadCenter::addPhoto(
		not_null<PhotoData*> photo,
		const QString &savePath,
		const Data::FileOrigin &origin) {
	auto task = DownloadTask();
	task.id = allocateNextId();
	task.state = DownloadState::Queued;
	task.source = DownloadSource::Photo;
	task.fileName = QString::number(photo->id) + u".jpg"_q;
	task.savePath = savePath;
	task.mimeType = SourceMimeType(DownloadSource::Photo);
	if (const auto size = photo->size(PhotoSize::Large)) {
		task.totalSize = size->width() * size->height() * 4;
	}
	task.addedAt = QDateTime::currentDateTime();
	task.origin = origin;
	const auto id = task.id;
	_tasks.emplace(id, std::move(task));
	_taskAdded.fire_copy(id);
	scheduleSave();
	startTask(id);
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
				LOG(("DLC: resume id=%1 onProgress ready=%2 total=%3").arg(id).arg(ready).arg(total));
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
	stopTask(id);
	setState(id, DownloadState::Cancelled);
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
	LOG(("DLC: remove id=%1 done").arg(id));
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
	const auto path = it->second.savedAbsolutePath.isEmpty()
		? it->second.savePath
		: it->second.savedAbsolutePath;
	const auto absolute = QFileInfo(path).absoluteFilePath();
	if (absolute.isEmpty()) {
		return;
	}
#if defined Q_OS_WIN
	QProcess::startDetached(u"explorer.exe"_q, {
		QString(),
		u"/select,"_q + QDir::toNativeSeparators(absolute),
	});
#elif defined Q_OS_MAC
	QProcess::startDetached(u"/usr/bin/open"_q, {
		u"-R"_q,
		absolute,
	});
#else
	QFileInfo info(absolute);
	const auto dir = info.isDir() ? absolute : info.absolutePath();
	QProcess::startDetached(u"xdg-open"_q, { dir });
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
	auto args = Storage::ParallelDownloadController::Args{
			.session = _session,
			.origin = it->second.origin,
			.toFile = it->second.savePath,
			.fullSize = it->second.totalSize,
			.config = {
				.chunks = it->second.parallelChunks,
				.chunkSize = it->second.chunkSize,
			},
		};
		if (it->second.source == DownloadSource::Document) {
				auto &owner = _session->data();
				const auto document = owner.document(it->second.documentId);
				args.document = document;
			}
		if (!args.document) {
			LOG(("DLC: startTask id=%1 no_document").arg(id));
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
				LOG(("DLC: startTask id=%1 controller_onProgress ready=%2 total=%3").arg(id).arg(ready).arg(total));
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
	const auto state = DeserializeState(
		object.value(u"state"_q).toString());
	if (!state) {
		return std::nullopt;
	}
	task.state = *state;
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
	auto file = QSaveFile(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return;
	}
	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	file.commit();
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
