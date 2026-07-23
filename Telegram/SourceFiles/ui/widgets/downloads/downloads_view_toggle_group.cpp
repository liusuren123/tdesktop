/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_view_toggle_group.h"

#include "ui/painter.h"

#include <QtGui/QPaintEvent>

namespace Ui {

DownloadsViewToggleGroup::DownloadsViewToggleGroup(QWidget *parent)
: QWidget(parent) {
	setFixedSize(64, 32);
	_a = Ui::CreateChild<DownloadsViewToggle>(this, &st::downloadsViewGrid);
	_a->move(0, 0);
	_b = Ui::CreateChild<DownloadsViewToggle>(this, &st::downloadsViewList);
	_b->move(32, 0);
	_a->setClickedCallback([this] { setActive(0); });
	_b->setClickedCallback([this] { setActive(1); });
	_a->setActive(false);
	_b->setActive(true);
}

void DownloadsViewToggleGroup::setActive(int index) {
	if (_active == index) return;
	_active = index;
	_a->setActive(index == 0);
	_b->setActive(index == 1);
	Q_EMIT activeChanged(_active);
}

QSize DownloadsViewToggleGroup::sizeHint() const {
	return QSize(64, 32);
}

void DownloadsViewToggleGroup::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	p.setRenderHint(QPainter::Antialiasing);
	p.setBrush(QColor(255, 255, 255, 12));
	p.setPen(Qt::NoPen);
	p.drawRoundedRect(
		QRectF(0.5, 0.5, width() - 1, height() - 1),
		8.0,
		8.0);
}

} // namespace Ui
