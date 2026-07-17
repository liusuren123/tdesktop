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
			LOG(("PDC: dtor cancelling chunk[%1]").arg(chunk.startOffset));
			chunk.loader->cancel();
			LOG(("PDC: dtor chunk[%1] cancelled").arg(chunk.startOffset));
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
				LOG(("PDC: start chunk[%1] FINISHED, skip").arg(chunk.startOffset));
				continue;
			}
			const auto tempSize = QFileInfo(chunk.tempFilePath).size();
			LOG(("PDC: start chunk[%1-%2] reloading loader, tempFile=%3 size=%4 ready=%5")
				.arg(chunk.startOffset).arg(chunk.endOffset)
				.arg(chunk.tempFilePath).arg(tempSize).arg(chunk.ready));
			chunk.loader = _args.document->createFileLoaderForParallel(
								_args.origin,
								chunk.tempFilePath,
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
			LOG(("PDC: start startChunk[%1-%2]").arg(chunk.startOffset).arg(chunk.endOffset));
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
			LOG(("PDC: stop cancelling chunk[%1]").arg(chunk.startOffset));
			chunk.loader->cancel();
			LOG(("PDC: stop chunk[%1] cancelled").arg(chunk.startOffset));
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
			chunk.ready = chunk.loader->currentOffset();
			LOG(("PDC: pause chunk[%1] saved ready=%2").arg(chunk.startOffset).arg(chunk.ready));
			chunk.loader->cancel();
			chunk.loader.reset();
		}
	}
	_started = false;
	LOG(("PDC: pause done"));
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
	const auto basePath = _args.toFile;
	const auto fileInfo = QFileInfo(basePath);
	const auto dir = fileInfo.absolutePath();
	const auto fileName = fileInfo.fileName();
	const auto stamp = QDateTime::currentMSecsSinceEpoch();
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
		chunk.tempFilePath = QDir(dir).absoluteFilePath(
			u"%1.part%2.%3.tmp"_q.arg(fileName).arg(i).arg(stamp));
		chunk.loader = _args.document->createFileLoaderForParallel(
							_args.origin,
							chunk.tempFilePath,
							chunkLength,
							startOffset,
							_args.fullSize,
							LoadFromCloudOrLocal);
				if (!chunk.loader) {
					_chunks.clear();
					return;
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
					: u"Download failed"_q;
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
	chunk.ready = chunk.loader->currentOffset();
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
		if (!concatenateChunks()) {
			LOG(("PDC: concatenate FAILED"));
			ok = false;
		}
	}
	if (!ok) {
		for (const auto &chunk : _chunks) {
			QFile::remove(chunk.tempFilePath);
		}
	}
	auto callback = std::move(_onFinished);
	_onFinished = nullptr;
	_onProgress = nullptr;
	if (callback) {
		callback(ok, ok ? QString() : error);
	}
}
bool ParallelDownloadController::concatenateChunks() {
	const auto finalPath = _args.toFile;
	const auto finalInfo = QFileInfo(finalPath);
	const auto finalDir = finalInfo.absolutePath();
	if (!finalDir.isEmpty() && !QDir().exists(finalDir)) {
		QDir().mkpath(finalDir);
	}
	auto output = QFile(finalPath);
	if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	for (const auto &chunk : _chunks) {
			QString tempPath;
			{
				auto input = QFile(chunk.tempFilePath);
				if (!input.open(QIODevice::ReadOnly)) {
					return false;
				}
				if (!input.seek(chunk.startOffset)) {
					return false;
				}
				constexpr auto kBufferSize = int64(256 * 1024);
				auto buffer = QByteArray(int(kBufferSize), Qt::Uninitialized);
				auto remaining = chunk.endOffset - chunk.startOffset;
				while (remaining > 0) {
					const auto toRead = std::min(int64(buffer.size()), remaining);
					const auto read = input.read(buffer.data(), toRead);
					if (read < 0) {
						return false;
					}
					if (read == 0) {
						break;
					}
					if (output.write(buffer.constData(), read) != read) {
						return false;
					}
					remaining -= read;
				}
				tempPath = chunk.tempFilePath;
			}
			if (!tempPath.isEmpty()) {
				QFile::remove(tempPath);
			}
		}
	output.close();
	return true;
}
} // namespace Storage
