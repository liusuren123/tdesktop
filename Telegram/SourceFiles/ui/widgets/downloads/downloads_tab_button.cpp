/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_tab_button.h"

#include "styles/style_widgets.h"
#include "ui/painter.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QMouseEvent>

namespace Ui {
namespace {

constexpr int kPillHPad = 6;
constexpr int kPillVPad = 2;
constexpr int kPillGap = 6;

int pillWidthFor(const QFontMetrics &fm, int count) {
	if (count <= 0) return 0;
	return fm.horizontalAdvance(QString::number(count)) + kPillHPad * 2;
}

} // namespace

DownloadsTabButton::DownloadsTabButton(
		QWidget *parent,
		const style::icon &icon,
		const QString &text)
: AbstractButton(parent)
, _icon(icon)
, _text(text) {
	setCursor(Qt::PointingHandCursor);
}

void DownloadsTabButton::setText(const QString &text) {
	_text = text;
	updateGeometry();
	update();
}

void DownloadsTabButton::setActive(bool active) {
	if (_active == active) return;
	_active = active;
	update();
}

void DownloadsTabButton::setCount(int count) {
	if (_count == count) return;
	_count = count;
	updateGeometry();
	update();
}

QSize DownloadsTabButton::sizeHint() const {
	const auto fm = QFontMetrics(font());
	const auto textWidth = fm.horizontalAdvance(_text);
	const auto pillWidth = pillWidthFor(fm, _count);
	const auto gap = (_count > 0) ? kPillGap : 0;
	const auto w = textWidth + gap + pillWidth
		+ _icon.width() + 12 + 12;
	return QSize(w, int(st::downloadsTabBarHeight));
}

QRect DownloadsTabButton::contentRect() const {
	const auto fm = QFontMetrics(font());
	const auto textWidth = fm.horizontalAdvance(_text);
	const auto pillWidth = pillWidthFor(fm, _count);
	const auto gap = (_count > 0) ? kPillGap : 0;
	const auto w = textWidth + gap + pillWidth + _icon.width() + 12;
	const auto h = _icon.height();
	return QRect(
		(width() - w) / 2,
		(height() - h) / 2,
		w,
		h);
}

void DownloadsTabButton::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);

	const auto cr = contentRect();
	const auto fm = p.fontMetrics();
	const auto textY = cr.y() + (_icon.height() + fm.ascent()) / 2;

	_icon.paint(
		p,
		cr.x(),
		cr.y(),
		width(),
		_active ? QColor(42, 171, 238) : QColor(170, 170, 170));

	p.setFont(font());
	p.setPen(_active ? QColor(42, 171, 238) : QColor(220, 220, 220));
	if (_active) p.setFont(st::downloadsHeaderTitle.style.font);
	p.drawText(
		cr.x() + _icon.width() + 6,
		textY,
		_text);

	if (_count > 0) {
		const auto textWidth = fm.horizontalAdvance(_text);
		const auto pillW = pillWidthFor(fm, _count);
		const auto pillH = fm.ascent() + kPillVPad * 2;
		const auto pillX = cr.x() + _icon.width() + 6 + textWidth + kPillGap;
		const auto pillY = textY - fm.ascent() - kPillVPad + 1;
		const auto pillRect = QRect(pillX, pillY, pillW, pillH);
		const auto radius = pillH / 2;

		p.setPen(Qt::NoPen);
		p.setBrush(_active
			? QColor(42, 171, 238)
			: QColor(255, 255, 255, 15));
		p.drawRoundedRect(pillRect, radius, radius);

		p.setPen(QColor(255, 255, 255));
		p.drawText(
			pillX + kPillHPad,
			textY,
			QString::number(_count));
	}
}

void DownloadsTabButton::mousePressEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		setActive(true);
	}
}

void DownloadsTabButton::mouseReleaseEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton && rect().contains(e->pos())) {
		clicked(e->modifiers(), Qt::LeftButton);
	}
}

} // namespace Ui