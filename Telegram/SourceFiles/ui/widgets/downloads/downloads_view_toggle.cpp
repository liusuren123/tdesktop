/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_view_toggle.h"

#include "ui/painter.h"

#include <QtGui/QMouseEvent>

namespace Ui {

DownloadsViewToggle::DownloadsViewToggle(
		QWidget *parent,
		const style::icon *icon)
: AbstractButton(parent)
, _icon(icon) {
	setCursor(Qt::PointingHandCursor);
	setFixedSize(32, 32);
}

void DownloadsViewToggle::setActive(bool active) {
	if (_active == active) return;
	_active = active;
	update();
}

QSize DownloadsViewToggle::sizeHint() const {
	return QSize(32, 32);
}

void DownloadsViewToggle::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	// Active toggle gets a brighter surface so the current view mode reads
	// at a glance; hover alone gives a faint wash.
	if (_active || _hover) {
		p.fillRect(rect(), QColor(255, 255, 255, _active ? 30 : 15));
	}
	if (_icon) {
		const auto x = (width() - _icon->width()) / 2;
		const auto y = (height() - _icon->height()) / 2;
		_icon->paint(
			p,
			x,
			y,
			width(),
			_active ? QColor(255, 255, 255) : QColor(220, 220, 220));
	}
}

void DownloadsViewToggle::mouseMoveEvent(QMouseEvent *e) {
	const auto was = _hover;
	_hover = rect().contains(e->pos());
	if (_hover != was) update();
}

void DownloadsViewToggle::leaveEventHook(QEvent *e) {
	if (_hover) {
		_hover = false;
		update();
	}
}

} // namespace Ui