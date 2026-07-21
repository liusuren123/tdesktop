/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_file_icon.h"

#include "styles/style_downloads_icons.h"
#include "ui/painter.h"
#include "ui/widgets/downloads/downloads_style.h"

#include <QtCore/QString>

namespace Ui {

namespace {

[[nodiscard]] QColor AbbreviationColorFor(Sample::Kind kind) {
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

[[nodiscard]] QString AbbreviationFor(
		Sample::Kind kind,
		const QString &fileName) {
	const auto dotIndex = fileName.lastIndexOf('.');
	const auto ext = (dotIndex >= 0)
		? fileName.mid(dotIndex + 1).toUpper()
		: QString();
	auto pick = [&](Sample::Kind target, std::initializer_list<QString> candidates) -> QString {
		if (kind != target) {
			return QString();
		}
		for (const auto &c : candidates) {
			if (c == ext) {
				return QString(c);
			}
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

} // namespace

DownloadsFileIcon::DownloadsFileIcon(QWidget *parent)
: RpWidget(parent) {
	resize(st::downloadsThumbnailSize, st::downloadsThumbnailSize);
	setAttribute(Qt::WA_TransparentForMouseEvents);
}

QSize DownloadsFileIcon::sizeHint() const {
	return QSize(st::downloadsThumbnailSize, st::downloadsThumbnailSize);
}

void DownloadsFileIcon::setSample(const Sample::Row &row) {
	if (_kind == row.kind && _fileName == row.fileName && _isPlaying == row.isPlaying) {
		return;
	}
	_kind = row.kind;
	_fileName = row.fileName;
	_isPlaying = row.isPlaying;
	update();
}

void DownloadsFileIcon::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	const auto r = rect();

	auto hq = PainterHighQualityEnabler(p);
	const auto radius = st::downloadsThumbnailRadius;
	const auto path = [&](const QRectF &rect) {
		QPainterPath pp;
		pp.addRoundedRect(rect, radius, radius);
		return pp;
	};

	if (_kind == Sample::Kind::Photo || _kind == Sample::Kind::Video) {
		p.fillPath(path(r), DownloadsStyle::thumbnailPlaceholder());
		if (_isPlaying) {
			p.fillPath(path(r), DownloadsStyle::playingOverlay());
			const auto cx = r.width() / 2.0;
			const auto cy = r.height() / 2.0;
			const auto side = r.width() * 0.28;
			QPainterPath triangle;
			triangle.moveTo(cx - side * 0.5, cy - side * 0.7);
			triangle.lineTo(cx + side * 0.7, cy);
			triangle.lineTo(cx - side * 0.5, cy + side * 0.7);
			triangle.closeSubpath();
			p.fillPath(triangle, DownloadsStyle::textFg());
		}
		return;
	}

	p.fillPath(path(r), DownloadsStyle::bg());
	const auto border = QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5);
	p.setPen(QPen(DownloadsStyle::rowBorder(), st::downloadsRowBorderWidth));
	p.drawPath(path(border));

	const auto abbrev = AbbreviationFor(_kind, _fileName);
	if (abbrev.isEmpty()) {
		return;
	}
	p.setPen(AbbreviationColorFor(_kind));
	auto font = st::downloadsFileIconFont->f;
	p.setFont(font);
	const auto metrics = QFontMetrics(font);
	const auto textRect = metrics.boundingRect(abbrev);
	const auto x = (r.width() - textRect.width()) / 2.0;
	const auto y = (r.height() + metrics.ascent() - metrics.descent()) / 2.0;
	p.drawText(QPointF(x, y), abbrev);
}

} // namespace Ui