/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtGui/QImage>
#include <QtCore/QString>
#include <cstdint>
#include <memory>
#include <vector>

namespace Sample {

enum class State {
	Downloading,
	Completed,
	Paused,
	Failed,
};

enum class Kind {
	Photo,
	Video,
	Audio,
	Voice,
	Document,
	Archive,
	Link,
};

struct Row {
	QString fileName;
	QString chatName;
	QString context;
	Kind kind = Kind::Document;
	State state = State::Completed;
	int percent = 100;
	QString sizeText;
	QString dateText;
	bool isPlaying = false;
	// Backing DownloadCenter task id (Data::DownloadTaskId = uint64). 0 for
	// pure-sample rows.
	uint64 taskId = 0;
	// Resolved preview (photo/video poster). Null when not yet loaded or not
	// applicable; the painter falls back to a placeholder.
	QImage thumb;
};

[[nodiscard]] const std::vector<Row> &InitialRows();
[[nodiscard]] int ActiveCount(const std::vector<Row> &rows);

} // namespace Sample