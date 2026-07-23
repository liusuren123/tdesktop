/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_footer_bar.h"

#include "lang/lang_keys.h"
#include "styles/style_downloads_icons.h"
#include "ui/painter.h"
#include "ui/widgets/downloads/downloads_style.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QMouseEvent>

namespace Ui {
namespace {

constexpr int kDotSize = 8;
constexpr int kDotToTextGap = 8;
constexpr int kTextToButtonsGap = 16;
constexpr int kButtonIconSize = 11;
constexpr int kButtonIconTextGap = 6;
constexpr int kButtonHeight = 30;
constexpr int kPaddingH = 20;

} // namespace

DownloadsFooterBar::DownloadsFooterBar(QWidget *parent)
: RpWidget(parent) {
	setAttribute(Qt::WA_OpaquePaintEvent, false);
	setMouseTracking(true);
	setCursor(Qt::PointingHandCursor);
}

void DownloadsFooterBar::setCountText(const QString &text) {
	if (_countText == text) return;
	_countText = text;
	update();
}

rpl::producer<> DownloadsFooterBar::pauseAllClicks() const {
	return _pauseAllClicks.events();
}

rpl::producer<> DownloadsFooterBar::cancelAllClicks() const {
	return _cancelAllClicks.events();
}

std::array<DownloadsFooterBar::ButtonRect, 2>
DownloadsFooterBar::buttons() const {
	const auto fm = QFontMetrics(font());

	const auto pauseLabel = tr::lng_downloads_action_pause_all(tr::now);
	const auto cancelLabel = tr::lng_downloads_action_cancel_all(tr::now);
	const auto pauseW = fm.horizontalAdvance(pauseLabel);
	const auto cancelW = fm.horizontalAdvance(cancelLabel);

	// Each button: [icon 11px] [gap 6] [text]
	const auto pauseTotal = kButtonIconSize + kButtonIconTextGap + pauseW;
	const auto cancelTotal = kButtonIconSize + kButtonIconTextGap + cancelW;

	// Anchor right edge first (Cancel all is rightmost), then back-calculate.
	const auto buttonY = (height() - kButtonHeight) / 2;
	auto right = width() - kPaddingH;
	const auto cancelRect = QRect(
		right - cancelTotal, buttonY, cancelTotal, kButtonHeight);
	right = cancelRect.left() - kTextToButtonsGap;
	const auto pauseRect = QRect(
		right - pauseTotal, buttonY, pauseTotal, kButtonHeight);

	return {{
		{ pauseRect,  Action::Pause  },
		{ cancelRect, Action::Cancel },
	}};
}

DownloadsFooterBar::Action DownloadsFooterBar::actionAt(const QPoint &pos) const {
	for (const auto &b : buttons()) {
		if (b.rect.contains(pos)) return b.action;
	}
	return Action::None;
}

void DownloadsFooterBar::updateHover(const QPoint &pos) {
	const auto newHover = actionAt(pos);
	if (newHover != _hover) {
		_hover = newHover;
		setCursor(_hover == Action::None
			? Qt::ArrowCursor
			: Qt::PointingHandCursor);
		update();
	}
}

void DownloadsFooterBar::mouseMoveEvent(QMouseEvent *e) {
	updateHover(e->pos());
}

void DownloadsFooterBar::mousePressEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) return;
	switch (actionAt(e->pos())) {
	case Action::Pause:  _pauseAllClicks.fire({}); break;
	case Action::Cancel: _cancelAllClicks.fire({}); break;
	default: break;
	}
}

void DownloadsFooterBar::leaveEventHook(QEvent *e) {
	if (_hover != Action::None) {
		_hover = Action::None;
		setCursor(Qt::ArrowCursor);
		update();
	}
}

void DownloadsFooterBar::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);

	// Blue dot — left side.
	const auto dotY = (height() - kDotSize) / 2;
	const auto dotRect = QRectF(kPaddingH, dotY, kDotSize, kDotSize);
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(0x2A, 0xAB, 0xEE));
	p.drawEllipse(dotRect);

	// Count text — right after the dot.
	if (!_countText.isEmpty()) {
		p.setPen(DownloadsStyle::textFg());
		auto f = font();
		f.setPixelSize(12);
		p.setFont(f);
		const auto fm = QFontMetrics(f);
		const auto textX = int(dotRect.right()) + kDotToTextGap;
		const auto textY = (height() + fm.ascent() - fm.descent()) / 2;
		p.drawText(textX, textY, _countText);
	}

	// Buttons — Pause all first, Cancel all second (matches prototype).
	const auto iconColor = QColor(190, 190, 190);
	for (const auto &b : buttons()) {
		// Hover background.
		if (b.action == _hover) {
			p.setBrush(QColor(255, 255, 255, 15));
			p.setPen(Qt::NoPen);
			p.drawRoundedRect(b.rect, 6, 6);
		}

		// Icon.
		const auto iconY = b.rect.y() + (kButtonHeight - kButtonIconSize) / 2;
		const auto iconX = b.rect.x();
		const style::icon *icon = (b.action == Action::Pause)
			? &st::downloadsPause
			: &st::downloadsCancel;
		icon->paint(p, iconX, iconY, width(), iconColor);

		// Label.
		p.setPen(iconColor);
		auto f = font();
		f.setPixelSize(12);
		p.setFont(f);
		const auto fm = QFontMetrics(f);
		const auto label = (b.action == Action::Pause)
			? tr::lng_downloads_action_pause_all(tr::now)
			: tr::lng_downloads_action_cancel_all(tr::now);
		const auto labelX = iconX + kButtonIconSize + kButtonIconTextGap;
		const auto labelY = b.rect.y()
			+ (kButtonHeight + fm.ascent() - fm.descent()) / 2;
		p.drawText(labelX, labelY, label);
	}
}

} // namespace Ui