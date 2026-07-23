/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/widgets/downloads/downloads_sample.h"
#include "ui/widgets/downloads/downloads_style.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <utility>

namespace Ui {
namespace KindUtil {

// Short file-type label shown inside thumbnails (PDF / DOCX / MP3 / ...).
// Mirrors the prototype's coloured abbreviation boxes.
[[nodiscard]] inline QString AbbreviationFor(
		Sample::Kind kind,
		const QString &fileName) {
	const auto dotIndex = fileName.lastIndexOf('.');
	const auto ext = (dotIndex >= 0)
		? fileName.mid(dotIndex + 1).toUpper()
		: QString();
	const auto pick = [&](
			Sample::Kind target,
			std::initializer_list<QString> candidates) -> QString {
		if (kind != target) return QString();
		for (const auto &c : candidates) {
			if (c == ext) return QString(c);
		}
		return QString();
	};
	const auto doc = pick(Sample::Kind::Document, { "PDF", "DOCX", "XLSX" });
	if (!doc.isEmpty()) return doc;
	const auto audio = pick(Sample::Kind::Audio, { "MP3", "M4A" });
	if (!audio.isEmpty()) return audio;
	const auto voice = pick(Sample::Kind::Voice, { "OGG" });
	if (!voice.isEmpty()) return voice;
	const auto arch = pick(Sample::Kind::Archive, { "ZIP", "7Z", "TAR", "GZ" });
	if (!arch.isEmpty()) return arch;
	if (kind == Sample::Kind::Link) {
		return u"URL"_q;
	}
	return ext.isEmpty()
		? fileName.left(3).toUpper()
		: ext.left(3);
}

// Foreground colour for a file-type abbreviation, matching the prototype
// palette (PDF red, MP3 purple, DOCX blue, …).
[[nodiscard]] inline QColor AbbreviationColorFor(Sample::Kind kind) {
	switch (kind) {
	case Sample::Kind::Document: return DownloadsStyle::pdfFg();
	case Sample::Kind::Audio:    return DownloadsStyle::mp3Fg();
	case Sample::Kind::Voice:    return DownloadsStyle::oggFg();
	case Sample::Kind::Archive:  return DownloadsStyle::zipFg();
	case Sample::Kind::Link:     return DownloadsStyle::urlFg();
	case Sample::Kind::Photo:
	case Sample::Kind::Video:
	default:                     return DownloadsStyle::textFg();
	}
}

// Human-readable status text shown in badges ("Done", "67%", "Paused", …).
[[nodiscard]] inline QString StatusTextFor(
		const Sample::Row &row,
		const QString &done,
		const QString &paused,
		const QString &failed) {
	switch (row.state) {
	case Sample::State::Downloading: return QString::number(row.percent) + u"%"_q;
	case Sample::State::Paused:     return paused;
	case Sample::State::Failed:     return failed;
	case Sample::State::Completed:
	default:                        return done;
	}
}

// Splits "Saved Messages · 1:02:17" into [chat, extra] on the " · ".
[[nodiscard]] inline std::pair<QString, QString> SplitContext(
		const QString &context) {
	const auto idx = context.indexOf(u"·"_q);
	if (idx < 0) {
		return { context, QString() };
	}
	return {
		context.left(idx).trimmed(),
		context.mid(idx + 1).trimmed(),
	};
}

} // namespace KindUtil
} // namespace Ui
