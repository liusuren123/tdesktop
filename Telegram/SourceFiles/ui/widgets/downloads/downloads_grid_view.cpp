/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_grid_view.h"

#include "lang/lang_keys.h"
#include "styles/style_downloads_icons.h"
#include "styles/style_widgets.h"
#include "ui/painter.h"
#include "ui/widgets/downloads/downloads_kind.h"
#include "ui/widgets/downloads/downloads_style.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainterPath>
#include <algorithm>

namespace Ui {
namespace {

using KindUtil::AbbreviationColorFor;
using KindUtil::AbbreviationFor;
using KindUtil::StatusTextFor;

// Geometry constants are intentionally local — the page-level style tokens
// (downloadsCard*) drive the layout, surfaced here for readability.
[[nodiscard]] int cardGap()       { return int(st::downloadsCardGap); }
[[nodiscard]] int cardHeight()    { return int(st::downloadsCardHeight); }
[[nodiscard]] int cardRadius()    { return int(st::downloadsCardRadius); }
[[nodiscard]] int thumbHeight()   { return int(st::downloadsCardThumbnailHeight); }
[[nodiscard]] int minCardWidth()  { return int(st::downloadsCardMinWidth); }
[[nodiscard]] int infoPadH()      { return int(st::downloadsCardInfoPadH); }
[[nodiscard]] int infoPadV()      { return int(st::downloadsCardInfoPadV); }

// The duration string ("4:22") is stored in row.context after the last " · ".
[[nodiscard]] QString DurationFrom(const Sample::Row &row) {
	const auto idx = row.context.lastIndexOf(u"·"_q);
	if (idx < 0) return QString();
	auto s = row.context.mid(idx + 1).trimmed();
	return s;
}

[[nodiscard]] QColor StatusFgFor(Sample::State state) {
	switch (state) {
	case Sample::State::Downloading: return DownloadsStyle::activeFg();
	case Sample::State::Paused:     return DownloadsStyle::pausedFg();
	case Sample::State::Failed:     return DownloadsStyle::failedFg();
	case Sample::State::Completed:
	default:                        return DownloadsStyle::doneFg();
	}
}

[[nodiscard]] QColor StatusBgFor(Sample::State state) {
	switch (state) {
	case Sample::State::Downloading: return DownloadsStyle::activeBg();
	case Sample::State::Paused:     return DownloadsStyle::pausedBg();
	case Sample::State::Failed:     return DownloadsStyle::failedBg();
	case Sample::State::Completed:
	default:                        return DownloadsStyle::doneBg();
	}
}

} // namespace

DownloadsGridView::DownloadsGridView(QWidget *parent)
: RpWidget(parent) {
	setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void DownloadsGridView::setRows(std::vector<Sample::Row> rows) {
	_rows = std::move(rows);
	reflow();
}

void DownloadsGridView::setSelectedIndex(int index) {
	if (_selectedIndex == index) return;
	_selectedIndex = index;
	update();
}

rpl::producer<int> DownloadsGridView::cardClicked() const {
	return _cardClicked.events();
}

void DownloadsGridView::mousePressEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) return;
	for (auto i = 0; i < int(_cardRects.size()); ++i) {
		if (_cardRects[i].contains(e->pos())) {
			_cardClicked.fire_copy(i);
			return;
		}
	}
}

void DownloadsGridView::resizeEvent(QResizeEvent *e) {
	RpWidget::resizeEvent(e);
	reflow();
}

void DownloadsGridView::reflow() {
	const auto w = width();
	if (w <= 0) return;

	const auto gap = cardGap();
	const auto minW = minCardWidth();
	const auto cols = std::max(1, (w + gap) / (minW + gap));
	const auto cardW = (w - gap * (cols - 1)) / cols;
	const auto rowStep = cardHeight() + gap;

	const auto count = int(_rows.size());
	const auto rowsNeeded = (count + cols - 1) / cols;
	const auto totalH = rowsNeeded > 0
		? rowsNeeded * rowStep - gap
		: 0;

	_cardRects.clear();
	_cardRects.reserve(count);
	for (auto i = 0; i < count; ++i) {
		const auto col = i % cols;
		const auto r = i / cols;
		_cardRects.push_back(QRect(
			col * (cardW + gap),
			r * rowStep,
			cardW,
			cardHeight()));
	}

	if (height() != totalH) {
		setFixedHeight(totalH);
	}
	update();
}

void DownloadsGridView::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	// Fill the page background so the area between/around cards matches the
	// rest of the Downloads page (the QScrollArea viewport doesn't inherit
	// DownloadsContent's paintEvent fill on its own).
	p.fillRect(rect(), DownloadsStyle::bg());
	p.setRenderHint(QPainter::Antialiasing);
	const auto n = std::min(int(_cardRects.size()), int(_rows.size()));
	for (auto i = 0; i < n; ++i) {
		paintCard(&p, _cardRects[i], _rows[i], i == _selectedIndex);
	}
}

void DownloadsGridView::paintCard(
		QPainter *p,
		const QRect &card,
		const Sample::Row &row,
		bool selected) const {
	auto hq = PainterHighQualityEnabler(*p);

	// Card surface — subtle elevated tone on the page background.
	QPainterPath cardPath;
	cardPath.addRoundedRect(QRectF(card), cardRadius(), cardRadius());
	p->fillPath(cardPath, QColor(255, 255, 255, selected ? 16 : 8));

	// Selection ring — accent-blue outline on the selected card.
	if (selected) {
		p->setBrush(Qt::NoBrush);
		p->setPen(QPen(DownloadsStyle::accent(), 2.0));
		p->drawRoundedRect(
			QRectF(card).adjusted(1.0, 1.0, -1.0, -1.0),
			cardRadius() - 1.0,
			cardRadius() - 1.0);
	}

	const auto radius = cardRadius();

	// Thumbnail clip: rounded top corners only, flush with the card top.
	const auto thumb = QRect(card.x(), card.y(), card.width(), thumbHeight());
	p->save();
	QPainterPath thumbClip;
	thumbClip.addRoundedRect(
		QRectF(thumb), radius, radius);
	// Square off the bottom corners of the thumbnail clip so it meets the
	// info area without a visible gap.
	thumbClip.addRect(card.x(), card.y() + thumbHeight() - radius,
		card.width(), radius);
	p->setClipPath(thumbClip);
	paintThumbnail(p, thumb, row);
	p->restore();

	// Info area.
	const auto padH = infoPadH();
	const auto padV = infoPadV();
	const auto infoX = card.x() + padH;
	const auto infoW = card.width() - padH * 2;
	auto y = card.y() + thumbHeight() + padV;

	// Filename.
	auto nameFont = st::downloadsRowName.style.font;
	p->setFont(nameFont);
	p->setPen(st::downloadsRowName.textFg->c);
	const auto nameMetrics = QFontMetrics(nameFont);
	p->drawText(
		QRect(infoX, y, infoW, nameMetrics.height()),
		Qt::AlignLeft | Qt::AlignVCenter,
		nameMetrics.elidedText(row.fileName, Qt::ElideRight, infoW));
	y += nameMetrics.height() + 4;

	// Size (left) + status pill (right) on one line.
	auto sizeFont = st::downloadsRowSize.style.font;
	p->setFont(sizeFont);
	p->setPen(st::downloadsRowSize.textFg->c);
	const auto sizeMetrics = QFontMetrics(sizeFont);
	p->drawText(
		QRect(infoX, y, infoW, sizeMetrics.height()),
		Qt::AlignLeft | Qt::AlignVCenter,
		sizeMetrics.elidedText(row.sizeText, Qt::ElideRight, infoW));
	paintStatusPill(p, QRect(infoX, y, infoW, sizeMetrics.height()), row);
	y += sizeMetrics.height() + 4;

	// Date.
	auto dateFont = st::downloadsRowDate.style.font;
	p->setFont(dateFont);
	p->setPen(st::downloadsRowDate.textFg->c);
	const auto dateMetrics = QFontMetrics(dateFont);
	p->drawText(
		QRect(infoX, y, infoW, dateMetrics.height()),
		Qt::AlignLeft | Qt::AlignVCenter,
		dateMetrics.elidedText(row.dateText, Qt::ElideRight, infoW));
}

void DownloadsGridView::paintThumbnail(
		QPainter *p,
		const QRect &thumb,
		const Sample::Row &row) const {
	const auto isMedia = row.kind == Sample::Kind::Photo
		|| row.kind == Sample::Kind::Video;

	if (isMedia) {
		// Placeholder fill (no real preview assets in this demo).
		p->fillRect(thumb, DownloadsStyle::thumbnailPlaceholder());

		// Duration badge for videos, bottom-right.
		if (row.kind == Sample::Kind::Video) {
			const auto dur = DurationFrom(row);
			if (!dur.isEmpty()) {
				auto font = st::downloadsBadgeFont->f;
				p->setFont(font);
				const auto fm = QFontMetrics(font);
				const auto tw = fm.horizontalAdvance(dur);
				const auto pillW = tw + 12;
				const auto pillH = fm.height() + 2;
				const auto px = thumb.right() - pillW - 6;
				const auto py = thumb.bottom() - pillH - 6;
				auto pill = QRectF(px, py, pillW, pillH);
				p->setPen(Qt::NoPen);
				p->setBrush(DownloadsStyle::playingOverlay());
				p->drawRoundedRect(pill, pillH / 2.0, pillH / 2.0);
				p->setPen(DownloadsStyle::textFg());
				p->drawText(pill, Qt::AlignCenter, dur);
			}
		}
		return;
	}

	// Document / audio / voice / archive / link — a subtle area with a
	// centred tinted box carrying the file-type abbreviation.
	p->fillRect(thumb, QColor(255, 255, 255, 10));

	const auto abbrev = AbbreviationFor(row.kind, row.fileName);
	if (abbrev.isEmpty()) return;

	const auto color = AbbreviationColorFor(row.kind);
	const auto box = int(st::downloadsCardDocBoxSize);
	const auto boxR = int(st::downloadsCardDocBoxRadius);
	const auto bx = thumb.center().x() - box / 2;
	const auto by = thumb.center().y() - box / 2;

	auto tint = color;
	tint.setAlpha(38);
	p->setPen(Qt::NoPen);
	p->setBrush(tint);
	p->drawRoundedRect(QRectF(bx, by, box, box), boxR, boxR);

	p->setPen(color);
	p->setFont(st::downloadsFileIconFont->f);
	p->drawText(QRect(bx, by, box, box), Qt::AlignCenter, abbrev);
}

void DownloadsGridView::paintStatusPill(
		QPainter *p,
		const QRect &rowArea,
		const Sample::Row &row) const {
	const auto text = StatusTextFor(
		row,
		tr::lng_downloads_status_done(tr::now),
		tr::lng_downloads_status_paused(tr::now),
		tr::lng_downloads_status_failed(tr::now));

	const auto bg = StatusBgFor(row.state);
	const auto fg = StatusFgFor(row.state);

	const auto font = st::downloadsBadgeFont->f;
	p->setFont(font);
	const auto metrics = QFontMetrics(font);
	const auto hPad = st::downloadsBadgePadding;
	const auto tw = metrics.horizontalAdvance(text);
	const auto pillW = tw + hPad * 2;
	const auto pillH = metrics.height();
	const auto pillX = rowArea.right() - pillW + 1;
	const auto pillY = rowArea.y() + (rowArea.height() - pillH) / 2;

	auto hq = PainterHighQualityEnabler(*p);
	QPainterPath path;
	path.addRoundedRect(
		QRectF(pillX, pillY, pillW, pillH),
		pillH / 2.0,
		pillH / 2.0);
	p->fillPath(path, bg);

	p->setPen(fg);
	p->drawText(QRectF(pillX, pillY, pillW, pillH), Qt::AlignCenter, text);
}

} // namespace Ui
