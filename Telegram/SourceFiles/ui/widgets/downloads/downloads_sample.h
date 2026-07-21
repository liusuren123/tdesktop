/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>
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
};

[[nodiscard]] const std::vector<Row> &InitialRows();
[[nodiscard]] int ActiveCount(const std::vector<Row> &rows);

} // namespace Sample