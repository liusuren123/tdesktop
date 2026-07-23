/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_text_button.h"

#include "ui/painter.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QMouseEvent>

namespace Ui {

namespace {

constexpr int kGap = 6;
constexpr int kPaddingH = 12;
constexpr int kPaddingV = 6;

} // namespace

DownloadsTextButton::DownloadsTextButton(
		QWidget *parent,
		const style::icon *icon,
		const QString &text,
		bool withChevron)
: AbstractButton(parent)
, _icon(icon)
, _text(text)
, _withChevron(withChevron) {
	setCursor(Qt::PointingHandCursor);
}

void DownloadsTextButton::setText(const QString &text) {
	if (_text == text) return;
	_text = text;
	updateGeometry();
	update();
}

QSize DownloadsTextButton::sizeHint() const {
	const auto fm = QFontMetrics(font());
	const auto textW = fm.horizontalAdvance(_text);
	const auto iconW = _icon ? _icon->width() : 0;
	const auto chevronW = _withChevron ? 12 + kGap : 0;
	const auto w = kPaddingH * 2 + iconW + (iconW > 0 ? kGap : 0)
		+ textW + chevronW;
	return QSize(w, kPaddingV * 2 + 18);
}

void DownloadsTextButton::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);

	const auto textColor = _hover ? QColor(255, 255, 255) : QColor(220, 220, 220);
	// Brighter icon colour so the 11px glyph stays visible against the dark
	// footer bg in both themes (was 170 grey, too low-contrast at 11px).
	const auto iconColor = _hover ? QColor(255, 255, 255) : QColor(190, 190, 190);

	const auto fm = p.fontMetrics();
	const auto textW = fm.horizontalAdvance(_text);
	const auto iconW = _icon ? _icon->width() : 0;
	const auto chevronW = _withChevron ? 12 : 0;
	const auto totalW = kPaddingH * 2 + iconW
		+ (iconW > 0 ? kGap : 0)
		+ textW
		+ (_withChevron ? kGap + chevronW : 0);

	// Centre the icon+text block horizontally inside the button widget.
	const auto x0 = (width() - totalW) / 2;
	const auto yMid = height() / 2;

	auto x = x0 + kPaddingH;
	if (_icon) {
		const auto iconY = yMid - _icon->height() / 2;
		_icon->paint(p, x, iconY, width(), iconColor);
		x += iconW + kGap;
	}

	p.setPen(textColor);
	p.setFont(font());
	const auto textY = yMid + fm.ascent() / 2 - 1;
	p.drawText(x, textY, _text);

	if (_withChevron) {
		const auto cx = x + textW + kGap;
		const auto cy = yMid - 6;
		st::downloadsChevronDown.paint(p, cx, cy, width(), iconColor);
	}
}

void DownloadsTextButton::resizeEvent(QResizeEvent *e) {
	updateHover();
}

void DownloadsTextButton::mouseMoveEvent(QMouseEvent *e) {
	const auto was = _hover;
	_hover = rect().contains(e->pos());
	if (_hover != was) update();
}

void DownloadsTextButton::leaveEventHook(QEvent *e) {
	if (_hover) {
		_hover = false;
		update();
	}
}

void DownloadsTextButton::updateHover() {
	_hover = false;
	update();
}

} // namespace Ui