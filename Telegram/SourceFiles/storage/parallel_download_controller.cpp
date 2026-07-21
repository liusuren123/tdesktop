/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "storage/parallel_download_controller.h"
#include "storage/file_download.h"
#include "storage/file_download_mtproto.h"
#include "data/data_document.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "base/debug_log.h"
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <cerrno>
namespace Storage {
namespace {
int64 AlignDown(int64 value, int64 alignment) {
	return (value / alignment) * alignment;
}
int64 AlignUp(int64 value, int64 alignment) {
	return ((value + alignment - 1) / alignment) * alignment;
}
} // namespace
ParallelDownloadController::ParallelDownloadController(Args &&args)
: _args(std::move(args)) {
	LOG(("PDC: ctor toFile=%1 fullSize=%2 chunks=%3")
		.arg(_args.toFile).arg(_args.fullSize).arg(_args.config.chunks));
}
ParallelDownloadController::~ParallelDownloadController() {
	LOG(("PDC: dtor toFile=%1 started=%2 chunks=%3")
		.arg(_args.toFile).arg(_started).arg(_chunks.size()));
	if (!_started) {
		LOG(("PDC: dtor early return (not started)"));
		return;
	}
	for (auto &chunk : _chunks) {
		if (chunk.loader) {
			chunk.loader->cancel();
		}
	}
	LOG(("PDC: dtor done"));
}
bool ParallelDownloadController::startable() const {
	return _args.session != nullptr
		&& _args.document != nullptr
		&& _args.fullSize > 0
		&& !_args.toFile.isEmpty();
}
void ParallelDownloadController::start(
		FinishedCallback onFinished,
		ProgressCallback onProgress) {
	LOG(("PDC: start toFile=%1 started=%2 chunks=%3 onFinished=%4")
		.arg(_args.toFile).arg(_started).arg(_chunks.size())
		.arg(onFinished ? "set" : "null"));
	if (_started) {
		LOG(("PDC: start ALREADY_STARTED, return"));
		return;
	}
	if (!startable()) {
		LOG(("PDC: start NOT_STARTABLE"));
		if (onFinished) {
			onFinished(false, u"Invalid download parameters"_q);
		}
		return;
	}
	_started = true;
	_onFinished = std::move(onFinished);
	_onProgress = std::move(onProgress);
	if (_chunks.empty()) {
		LOG(("PDC: start prepareChunks first time"));
		prepareChunks();
	} else {
		LOG(("PDC: start RESUME chunks.size=%1").arg(_chunks.size()));
		for (auto &chunk : _chunks) {
			if (chunk.finished) {
				continue;
			}
			chunk.loader = _args.document->createFileLoaderForParallel(
								_args.origin,
								_args.tempFilePath,
								chunk.endOffset - chunk.startOffset,
								chunk.startOffset,
								_args.fullSize,
								LoadFromCloudOrLocal);
		}
	}
	if (_chunks.empty()) {
		LOG(("PDC: start EMPTY_CHUNKS"));
		finish(false, u"No chunks created"_q);
		return;
	}
	for (auto &chunk : _chunks) {
		if (!chunk.finished) {
			startChunk(chunk);
		}
	}
	LOG(("PDC: start done"));
}
void ParallelDownloadController::stop() {
	LOG(("PDC: stop started=%1 chunks=%2").arg(_started).arg(_chunks.size()));
	if (!_started) {
		return;
	}
	for (auto &chunk : _chunks) {
		if (chunk.loader && !chunk.finished) {
			chunk.loader->cancel();
		}
	}
	_started = false;
	LOG(("PDC: stop done"));
}
void ParallelDownloadController::pause() {
	LOG(("PDC: pause started=%1 chunks=%2").arg(_started).arg(_chunks.size()));
	if (!_started) {
		return;
	}
	for (auto &chunk : _chunks) {
		if (chunk.loader && !chunk.finished) {
			chunk.ready = chunkReadyFromLoader(chunk);
			chunk.loader->cancel();
			chunk.loader.reset();
		}
	}
	_started = false;
	LOG(("PDC: pause done"));
}
int64 ParallelDownloadController::chunkReadyFromLoader(const Chunk &chunk) const {
	int64 result = chunk.ready;
	if (chunk.loader) {
		const auto offset = chunk.loader->readyForParallelChunk();
		const auto chunkLength = chunk.endOffset - chunk.startOffset;
		const auto fromLoader = std::clamp(
			offset - chunk.startOffset,
			int64(0),
			chunkLength);
		result = std::max(result, fromLoader);
	}
	return result;
}
int64 ParallelDownloadController::readySize() const {
	int64 result = 0;
	for (const auto &chunk : _chunks) {
		result += chunk.ready;
	}
	return result;
}
int64 ParallelDownloadController::totalSize() const {
	return _args.fullSize;
}
int ParallelDownloadController::chunksCompleted() const {
	int result = 0;
	for (const auto &chunk : _chunks) {
		if (chunk.finished) {
			++result;
		}
	}
	return result;
}
std::vector<ChunkState> ParallelDownloadController::chunkStates() const {
	auto result = std::vector<ChunkState>();
	result.reserve(_chunks.size());
	for (const auto &chunk : _chunks) {
		auto state = ChunkState();
		state.startOffset = chunk.startOffset;
		state.endOffset = chunk.endOffset;
		state.ready = chunkReadyFromLoader(chunk);
		state.finished = chunk.finished;
		result.push_back(std::move(state));
	}
	return result;
}
void ParallelDownloadController::prepareChunks() {
	const auto totalSize = _args.fullSize;
		auto chunksCount = _args.config.chunks;
		if (chunksCount <= 0) {
			chunksCount = 1;
		}
		const auto perChunk = (totalSize + chunksCount - 1) / chunksCount;
	const auto alignedPerChunk = std::max(
		int64(Storage::kDownloadPartSize),
		AlignDown(perChunk, Storage::kDownloadPartSize));

	// If the caller supplied persistent chunk state (loaded from the
	// DownloadTask JSON after a Telegram restart), reuse those ready
	// values. The temp file itself is the single _args.tempFilePath
	// shared by all chunks - mtpFileLoader::startLoading detects its
	// size and skips ahead to resume each chunk at the right offset.
	const bool useInitial = (_args.initialChunks.size()
			== size_t(chunksCount));

	_chunks.clear();
		_chunks.reserve(chunksCount);
		for (int i = 0; i < chunksCount; ++i) {
			const auto startOffset = int64(i) * alignedPerChunk;
			if (startOffset >= totalSize) {
				break;
			}
			const auto naturalEnd = startOffset + alignedPerChunk;
			const auto endOffset = (i + 1 >= chunksCount || naturalEnd >= totalSize)
				? totalSize
				: std::min(naturalEnd, totalSize);
			const auto chunkLength = endOffset - startOffset;
		auto chunk = Chunk();
		chunk.startOffset = startOffset;
		chunk.endOffset = endOffset;
		if (useInitial) {
			const auto &initial = _args.initialChunks[i];
			chunk.ready = std::clamp(initial.ready,
				int64(0),
				chunkLength);
			chunk.finished = initial.finished
				|| (chunk.ready >= chunkLength);
			if (chunk.finished) {
				chunk.ready = chunkLength;
			}
			LOG(("PDC: prepareChunks part[%1] from_initial ready=%2 chunkLength=%3 finished=%4")
				.arg(i)
				.arg(chunk.ready).arg(chunkLength)
				.arg(chunk.finished ? "yes" : "no"));
		} else {
			LOG(("PDC: prepareChunks part[%1] fresh chunkLength=%2")
				.arg(i).arg(chunkLength));
		}
		if (!chunk.finished) {
			chunk.loader = _args.document->createFileLoaderForParallel(
								_args.origin,
								_args.tempFilePath,
								chunkLength,
								chunk.startOffset,
								_args.fullSize,
								LoadFromCloudOrLocal);
			if (!chunk.loader) {
				_chunks.clear();
				return;
			}
		}
		_chunks.push_back(std::move(chunk));
	}
}
void ParallelDownloadController::startChunk(Chunk &chunk) {
	const auto index = int(&chunk - _chunks.data());
	const auto weak = base::make_weak(this);
	const auto id = index;
	chunk.loader->updates() | rpl::on_next_error_done(
			[=](rpl::empty_value) {
				crl::on_main([=] {
					if (const auto strong = weak.get()) {
						strong->onChunkProgress(id);
					}
				});
			},
			[=](FileLoader::Error error) {
							auto message = error.failureReason == FileLoader::FailureReason::FileWriteFailure
								? u"File write failure"_q
								: u"Download failed. The file may no longer be available - open the source chat and retry."_q;
				crl::on_main([=] {
					if (const auto strong = weak.get()) {
						strong->onChunkFinished(id, false, message);
					}
				});
			},
			[=] {
				crl::on_main([=] {
					if (const auto strong = weak.get()) {
						strong->onChunkFinished(id, true, QString());
					}
				});
			},
			chunk.loader->lifetime());
	chunk.loader->start();
}
void ParallelDownloadController::onChunkProgress(int index) {
	if (index < 0 || index >= int(_chunks.size())) {
		return;
	}
	auto &chunk = _chunks[index];
	chunk.ready = chunkReadyFromLoader(chunk);
	if (_onProgress) {
		_onProgress(readySize(), totalSize());
	}
}
void ParallelDownloadController::onChunkFinished(
		int index,
		bool ok,
		const QString &error) {
	LOG(("PDC: onChunkFinished index=%1 ok=%2 err=%3").arg(index).arg(ok).arg(error));
	if (!_started || index < 0 || index >= int(_chunks.size())) {
		LOG(("PDC: onChunkFinished IGNORE index=%1 started=%2 chunks=%3")
			.arg(index).arg(_started).arg(_chunks.size()));
		return;
	}
	auto &chunk = _chunks[index];
	chunk.finished = true;
	chunk.failed = !ok;
	chunk.failMessage = error;
	if (ok) {
		chunk.ready = chunk.endOffset - chunk.startOffset;
	}
	const auto allDone = ranges::all_of(_chunks, [](const Chunk &c) {
		return c.finished;
	});
	if (!allDone) {
		if (_onProgress) {
			_onProgress(readySize(), totalSize());
		}
		return;
	}
	const auto anyFailed = ranges::any_of(_chunks, [](const Chunk &c) {
		return c.failed;
	});
	if (anyFailed) {
		auto message = QString();
		for (const auto &c : _chunks) {
			if (c.failed) {
				message = c.failMessage;
				break;
			}
		}
		finish(false, message);
		return;
	}
	finish(true, QString());
}
void ParallelDownloadController::finish(bool ok, const QString &error) {
	LOG(("PDC: finish ok=%1 err=%2").arg(ok).arg(error));
	if (!_started) {
		LOG(("PDC: finish NOT_STARTED"));
		return;
	}
	_started = false;
	if (ok) {
		if (!finalizeDownload()) {
			LOG(("PDC: finalize FAILED"));
			ok = false;
		}
	}
	// Note: the temp file is intentionally NOT deleted when the
	// download fails. It is kept on disk so subsequent retries /
	// resumes can pick up partial progress. Cleanup happens
	// explicitly in DownloadCenter::remove() / DownloadCenter::cancel()
	// when the user removes or cancels the task. On a successful
	// finish the temp file has already been renamed away by
	// finalizeDownload().
	if (!ok) {
		LOG(("PDC: finish not ok, keeping temp file for resume"));
	}
	auto callback = std::move(_onFinished);
	_onFinished = nullptr;
	_onProgress = nullptr;
	if (callback) {
		callback(ok, ok ? QString() : error);
	}
}
bool ParallelDownloadController::finalizeDownload() {
	// All chunks wrote contiguously into a single temp file
	// (_args.tempFilePath). The file's bytes are already in the right
	// order - we just rename it to the final destination. If the
	// rename fails (e.g. target already exists and is locked), we
	// try removing the target first and retrying.
	const auto finalPath = _args.toFile;
	const auto finalInfo = QFileInfo(finalPath);
	const auto finalDir = finalInfo.absolutePath();
	if (!finalDir.isEmpty() && !QDir().exists(finalDir)) {
		if (!QDir().mkpath(finalDir)) {
			return false;
		}
	}
	if (QFile::exists(finalPath)) {
		QFile::remove(finalPath);
	}
	if (!QFile::rename(_args.tempFilePath, finalPath)) {
		LOG(("PDC: rename failed tempFile=%1 toFile=%2 err=%3")
			.arg(_args.tempFilePath).arg(finalPath).arg(int(errno)));
		return false;
	}
	return true;
}
} // namespace Storage
