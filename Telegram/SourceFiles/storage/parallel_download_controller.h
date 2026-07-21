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
// Serializable snapshot of a chunk's state. Stored on DownloadTask so
// that after a restart the download can resume from where the previous
// run left off without relying on filename heuristics or file-size
// guesses on disk.
struct ChunkState {
	int64 startOffset = 0;
	int64 endOffset = 0;
	int64 ready = 0;
	bool finished = false;
};
class ParallelDownloadController final : public base::has_weak_ptr {
public:
	struct Args {
		not_null<Main::Session*> session;
		Data::FileOrigin origin;
		DocumentData *document = nullptr;
		QString toFile;
		// Single temp file used by every chunk. Chunks write at their
		// own startOffset inside this file, so the file is dense and
		// its size equals the total bytes downloaded. Derived from
		// toFile by the caller (DownloadCenter) as "<toFile>.part.tmp".
		QString tempFilePath;
		int64 fullSize = 0;
		ParallelDownloadConfig config;
		// If non-empty, prepareChunks reuses these ready/finished
		// values (e.g. after a Telegram restart that loaded them from
		// the DownloadTask JSON). The size of this vector should
		// match config.chunks.
		std::vector<ChunkState> initialChunks;
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
	// Snapshot the current per-chunk state for persistence.
	[[nodiscard]] std::vector<ChunkState> chunkStates() const;
private:
	struct Chunk {
		std::unique_ptr<FileLoader> loader;
		int64 startOffset = 0;
		int64 endOffset = 0;
		int64 ready = 0;
		bool finished = false;
		bool failed = false;
		QString failMessage;
	};
	void prepareChunks();
	void startChunk(Chunk &chunk);
	void onChunkProgress(int index);
	void onChunkFinished(int index, bool ok, const QString &error);
	// Returns the per-chunk data extent reported by the loader (NOT
	// derived from the shared temp file's global size). The high-water
	// mark is the loader's next-request offset clamped to the chunk's
	// own range, so an out-of-order write by a different chunk cannot
	// inflate this chunk's reported progress. When the loader is gone
	// (paused / finished) we fall back to chunk.ready, which was
	// captured at the last event.
	int64 chunkReadyFromLoader(const Chunk &chunk) const;
	void finish(bool ok, const QString &error);
	// Atomically replaces the temp file with the final destination.
	[[nodiscard]] bool finalizeDownload();
	Args _args;
	std::vector<Chunk> _chunks;
	FinishedCallback _onFinished;
	ProgressCallback _onProgress;
	bool _started = false;
};
} // namespace Storage
