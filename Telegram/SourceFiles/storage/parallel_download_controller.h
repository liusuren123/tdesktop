/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once
#include "base/weak_ptr.h"
#include "base/flat_map.h"
#include "data/data_file_origin.h"
#include <QtCore/QString>
#include <functional>
#include <memory>
#include <vector>
class DocumentData;
class PhotoData;
class FileLoader;
namespace Main {
class Session;
} // namespace Main
namespace Storage {
struct ParallelDownloadConfig {
	int chunks = 4;
	int64 chunkSize = 128 * 1024;
};
class ParallelDownloadController final : public base::has_weak_ptr {
public:
	struct Args {
		not_null<Main::Session*> session;
		Data::FileOrigin origin;
		DocumentData *document = nullptr;
		QString toFile;
		int64 fullSize = 0;
		ParallelDownloadConfig config;
	};
	using FinishedCallback = std::function<void(bool ok, const QString &error)>;
	using ProgressCallback = std::function<void(int64 ready, int64 total)>;
	explicit ParallelDownloadController(Args &&args);
	~ParallelDownloadController();
	ParallelDownloadController(const ParallelDownloadController &) = delete;
	ParallelDownloadController &operator=(
		const ParallelDownloadController &) = delete;
	[[nodiscard]] bool startable() const;
	void start(FinishedCallback onFinished, ProgressCallback onProgress);
		void stop();
		void pause();
	[[nodiscard]] int64 readySize() const;
	[[nodiscard]] int64 totalSize() const;
	[[nodiscard]] int chunksCompleted() const;
private:
	struct Chunk {
		std::unique_ptr<FileLoader> loader;
		int64 startOffset = 0;
		int64 endOffset = 0;
		QString tempFilePath;
		int64 ready = 0;
		bool finished = false;
		bool failed = false;
		QString failMessage;
	};
	void prepareChunks();
	void startChunk(Chunk &chunk);
	void onChunkProgress(int index);
	void onChunkFinished(int index, bool ok, const QString &error);
	void finish(bool ok, const QString &error);
	[[nodiscard]] bool concatenateChunks();
	Args _args;
	std::vector<Chunk> _chunks;
	FinishedCallback _onFinished;
	ProgressCallback _onProgress;
	bool _started = false;
};
} // namespace Storage
