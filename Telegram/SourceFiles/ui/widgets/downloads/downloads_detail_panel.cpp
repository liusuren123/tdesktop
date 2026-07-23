/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_detail_panel.h"

#include "lang/lang_keys.h"
#include "styles/style_downloads_icons.h"
#include "styles/style_widgets.h"
#include "ui/painter.h"
#include "ui/widgets/downloads/downloads_kind.h"
#include "ui/widgets/downloads/downloads_style.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainterPath>

namespace Ui {
namespace {

using KindUtil::AbbreviationColorFor;
using KindUtil::AbbreviationFor;
using KindUtil::SplitContext;

constexpr int kCloseButtonSize = 32;

// Type label for the metadata "Type" row ("Audio", "Document", …).
[[nodiscard]] QString TypeLabelFor(Sample::Kind kind) {
	switch (kind) {
	case Sample::Kind::Audio:    return tr::lng_downloads_type_audio(tr::now);
	case Sample::Kind::Voice:    return tr::lng_downloads_type_voice(tr::now);
	case Sample::Kind::Document: return tr::lng_downloads_type_document(tr::now);
	case Sample::Kind::Archive:  return tr::lng_downloads_type_archive(tr::now);
	case Sample::Kind::Link:     return tr::lng_downloads_type_link(tr::now);
	case Sample::Kind::Photo:    return tr::lng_downloads_type_photo(tr::now);
	case Sample::Kind::Video:    return tr::lng_downloads_type_video(tr::now);
	}
	return QString();
}

// Primary action label depends on download state.
[[nodiscard]] QString PrimaryLabelFor(Sample::State state) {
	switch (state) {
	case Sample::State::Downloading:
	case Sample::State::Paused:
		return tr::lng_downloads_detail_resume(tr::now);
	case Sample::State::Failed:
		return tr::lng_downloads_detail_retry(tr::now);
	case Sample::State::Completed:
	default:
		return tr::lng_downloads_detail_open(tr::now);
	}
}

[[nodiscard]] const style::icon *PrimaryIconFor(Sample::State state) {
	// All states reuse the download glyph; the label disambiguates.
	return &st::downloadsDetailDownload;
}

[[nodiscard]] QString StatusLabelFor(const Sample::Row &row) {
	switch (row.state) {
	case Sample::State::Downloading:
		return tr::lng_downloads_detail_status_downloading(tr::now)
			+ ' ' + QString::number(row.percent) + '%';
	case Sample::State::Paused:
		return tr::lng_downloads_status_paused(tr::now);
	case Sample::State::Failed:
		return tr::lng_downloads_status_failed(tr::now);
	case Sample::State::Completed:
	default:
		return tr::lng_downloads_status_done(tr::now);
	}
}

} // namespace

DownloadsDetailPanel::DownloadsDetailPanel(QWidget *parent)
: RpWidget(parent) {
	setAttribute(Qt::WA_OpaquePaintEvent, false);
	setMouseTracking(true);
	setFixedWidth(int(st::downloadsDetailWidth));
}

void DownloadsDetailPanel::setRow(const Sample::Row &row) {
	_row = row;
	_hasRow = true;
	update();
}

void DownloadsDetailPanel::clear() {
	_hasRow = false;
	update();
}

rpl::producer<> DownloadsDetailPanel::closeClicks() const {
	return _closeClicks.events();
}

rpl::producer<> DownloadsDetailPanel::primaryClicks() const {
	return _primaryClicks.events();
}

rpl::producer<> DownloadsDetailPanel::forwardClicks() const {
	return _forwardClicks.events();
}

rpl::producer<> DownloadsDetailPanel::removeClicks() const {
	return _removeClicks.events();
}

QRect DownloadsDetailPanel::primaryRect() const {
	const auto padH = int(st::downloadsDetailPadH);
	const auto h = int(st::downloadsDetailButtonHeight);
	return QRect(padH, height() - 3 * h - 2 * int(st::downloadsDetailButtonGap) - 12,
		width() - padH * 2, h);
}

QRect DownloadsDetailPanel::secondaryRect(int index) const {
	const auto padH = int(st::downloadsDetailPadH);
	const auto h = int(st::downloadsDetailButtonHeight);
	const auto gap = int(st::downloadsDetailButtonGap);
	const auto primaryBottom = primaryRect().bottom() + gap;
	return QRect(padH, primaryBottom + index * (h + gap),
		width() - padH * 2, h);
}

DownloadsDetailPanel::Hit DownloadsDetailPanel::hitTest(const QPoint &pos) const {
	if (QRect(width() - kCloseButtonSize - 4, 4,
			kCloseButtonSize, kCloseButtonSize).contains(pos)) {
		return Hit::Close;
	}
	if (primaryRect().contains(pos)) {
		return Hit::Primary;
	}
	if (secondaryRect(0).contains(pos)) {
		return Hit::Forward;
	}
	if (secondaryRect(1).contains(pos)) {
		return Hit::Remove;
	}
	return Hit::None;
}

void DownloadsDetailPanel::updateHover(const QPoint &pos) {
	const auto next = hitTest(pos);
	if (next != _hover) {
		_hover = next;
		setCursor(_hover == Hit::None
			? Qt::ArrowCursor
			: Qt::PointingHandCursor);
		update();
	}
}

void DownloadsDetailPanel::mouseMoveEvent(QMouseEvent *e) {
	updateHover(e->pos());
}

void DownloadsDetailPanel::leaveEventHook(QEvent *e) {
	if (_hover != Hit::None) {
		_hover = Hit::None;
		setCursor(Qt::ArrowCursor);
		update();
	}
}

void DownloadsDetailPanel::mousePressEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) return;
	switch (hitTest(e->pos())) {
	case Hit::Close:   _closeClicks.fire({}); break;
	case Hit::Primary: _primaryClicks.fire({}); break;
	case Hit::Forward: _forwardClicks.fire({}); break;
	case Hit::Remove:  _removeClicks.fire({}); break;
	default: break;
	}
}

void DownloadsDetailPanel::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);

	// Panel surface — elevated above the page bg, with a left separator.
	p.fillRect(rect(), QColor(255, 255, 255, 6));
	p.fillRect(QRect(0, 0, 1, height()), QColor(255, 255, 255, 15));

	if (!_hasRow) return;

	const auto padH = int(st::downloadsDetailPadH);

	// Header: title + close.
	{
		const auto headerH = int(st::downloadsDetailHeaderHeight);
		auto f = st::downloadsHeaderTitle.style.font->f;
		f.setPixelSize(13);
		p.setFont(f);
		p.setPen(DownloadsStyle::textFg());
		p.drawText(QRect(padH, 0, width() - padH * 2, headerH),
			Qt::AlignLeft | Qt::AlignVCenter,
			tr::lng_downloads_detail_title(tr::now));

		// Close (X) — drawn as two crossed strokes.
		const auto btn = QRect(width() - kCloseButtonSize - 4,
			(headerH - kCloseButtonSize) / 2,
			kCloseButtonSize, kCloseButtonSize);
		if (_hover == Hit::Close) {
			p.setBrush(QColor(255, 255, 255, 15));
			p.setPen(Qt::NoPen);
			p.drawRoundedRect(btn, 6, 6);
		}
		const auto cx = btn.center();
		const auto r = 5;
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(DownloadsStyle::textDim(), 1.5));
		p.drawLine(cx.x() - r, cx.y() - r, cx.x() + r, cx.y() + r);
		p.drawLine(cx.x() + r, cx.y() - r, cx.x() - r, cx.y() + r);
	}

	auto y = int(st::downloadsDetailHeaderHeight);

	// File-type abbreviation box.
	{
		const auto box = int(st::downloadsDetailAbbrevBoxSize);
		const auto boxR = int(st::downloadsDetailAbbrevBoxRadius);
		const auto bx = (width() - box) / 2;
		const auto by = y + 16;
		const auto color = AbbreviationColorFor(_row.kind);
		auto tint = color; tint.setAlpha(38);
		p.setPen(Qt::NoPen);
		p.setBrush(tint);
		p.drawRoundedRect(QRectF(bx, by, box, box), boxR, boxR);
		p.setPen(color);
		auto f = st::downloadsFileIconFont->f;
		p.setFont(f);
		p.drawText(QRect(bx, by, box, box), Qt::AlignCenter,
			AbbreviationFor(_row.kind, _row.fileName));
		y = by + box + 16;
	}

	// Filename (wraps up to 2 lines, centred).
	{
		auto f = st::downloadsRowName.style.font;
		p.setFont(f);
		p.setPen(DownloadsStyle::textFg());
		const auto nameRect = QRect(padH, y, width() - padH * 2, f->height * 2 + 2);
		auto option = QTextOption();
		option.setAlignment(Qt::AlignHCenter);
		option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
		p.drawText(nameRect, _row.fileName, option);
		y = nameRect.bottom() + 12;
	}

	// Metadata rows.
	const auto [chatPart, durationPart] = SplitContext(_row.context);
	const struct MetaRow {
		QString label;
		QString value;
	} rows[] = {
		{ tr::lng_downloads_detail_size(tr::now),     _row.sizeText },
		{ tr::lng_downloads_detail_type(tr::now),     TypeLabelFor(_row.kind) },
		{ tr::lng_downloads_detail_from(tr::now),     _row.chatName },
		{ tr::lng_downloads_detail_chat(tr::now),     chatPart },
		{ tr::lng_downloads_detail_date(tr::now),     _row.dateText },
		{ tr::lng_downloads_detail_duration(tr::now), durationPart },
		{ tr::lng_downloads_detail_status(tr::now),   StatusLabelFor(_row) },
	};
	{
		const auto rowH = int(st::downloadsDetailRowHeight);
		auto labelFont = st::downloadsRowSource.style.font;
		auto valueFont = st::downloadsRowSize.style.font;
		const auto labelMetrics = QFontMetrics(labelFont);
		const auto valueMetrics = QFontMetrics(valueFont);
		for (const auto &r : rows) {
			if (r.value.isEmpty()) {
				y += rowH;
				continue;
			}
			p.setFont(labelFont);
			p.setPen(DownloadsStyle::textDim());
			p.drawText(QRect(padH, y, width() - padH * 2, rowH),
				Qt::AlignLeft | Qt::AlignVCenter, r.label);
			p.setFont(valueFont);
			p.setPen(DownloadsStyle::textFg());
			const auto valueRect = QRect(padH, y, width() - padH * 2, rowH);
			p.drawText(valueRect, Qt::AlignRight | Qt::AlignVCenter,
				valueMetrics.elidedText(r.value, Qt::ElideRight, valueRect.width()));
			y += rowH;
		}
	}

	// Primary action button.
	{
		const auto r = primaryRect();
		const auto radius = int(st::downloadsDetailButtonRadius);
		const auto hovered = (_hover == Hit::Primary);
		auto bg = DownloadsStyle::accent();
		if (hovered) {
			bg = bg.lighter(115);
		}
		p.setPen(Qt::NoPen);
		p.setBrush(bg);
		p.drawRoundedRect(r, radius, radius);

		const auto *icon = PrimaryIconFor(_row.state);
		const auto iconSize = 11;
		const auto label = PrimaryLabelFor(_row.state);
		auto f = st::downloadsRowName.style.font->f;
		f.setPixelSize(12);
		const auto metrics = QFontMetrics(f);
		const auto tw = metrics.horizontalAdvance(label);
		const auto totalW = iconSize + 6 + tw;
		auto x = r.center().x() - totalW / 2;
		icon->paint(p, x, r.center().y() - iconSize / 2 + 1, iconSize,
			DownloadsStyle::textFg());
		x += iconSize + 6;
		p.setFont(f);
		p.setPen(DownloadsStyle::textFg());
		p.drawText(QRect(x, r.y(), r.width(), r.height()),
			Qt::AlignLeft | Qt::AlignVCenter, label);
	}

	// Secondary buttons: Forward, Remove.
	{
		const auto radius = int(st::downloadsDetailButtonRadius);
		const struct Sec { Hit hit; QString label; const style::icon *icon; } secs[] = {
			{ Hit::Forward, tr::lng_downloads_detail_forward(tr::now), &st::downloadsDetailForward },
			{ Hit::Remove,  tr::lng_downloads_detail_remove(tr::now),  &st::downloadsDetailRemove },
		};
		for (auto i = 0; i < 2; ++i) {
			const auto &s = secs[i];
			const auto r = secondaryRect(i);
			if (_hover == s.hit) {
				p.setPen(Qt::NoPen);
				p.setBrush(QColor(255, 255, 255, 15));
				p.drawRoundedRect(r, radius, radius);
			}
			const auto iconSize = 11;
			auto f = st::downloadsRowSize.style.font->f;
			f.setPixelSize(12);
			const auto metrics = QFontMetrics(f);
			const auto tw = metrics.horizontalAdvance(s.label);
			const auto totalW = iconSize + 6 + tw;
			auto x = r.center().x() - totalW / 2;
			const auto iconY = r.center().y() - iconSize / 2 + 1;
			const auto iconColor = (s.hit == Hit::Remove)
				? DownloadsStyle::failedFg()
				: DownloadsStyle::textFg();
			s.icon->paint(p, x, iconY, iconSize, iconColor);
			x += iconSize + 6;
			p.setFont(f);
			p.setPen((s.hit == Hit::Remove)
				? DownloadsStyle::failedFg()
				: DownloadsStyle::textFg());
			p.drawText(QRect(x, r.y(), r.width(), r.height()),
				Qt::AlignLeft | Qt::AlignVCenter, s.label);
		}
	}
}

} // namespace Ui
