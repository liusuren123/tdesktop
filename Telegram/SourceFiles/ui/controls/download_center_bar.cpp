/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/controls/download_center_bar.h"

#include "ui/painter.h"
#include "ui/text/text_utilities.h"
#include "styles/style_dialogs.h"
#include "styles/style_window.h"

namespace Ui {

DownloadCenterBar::DownloadCenterBar(QWidget *parent)
: RippleButton(parent, st::defaultRippleAnimation) {
	resize(st::dialogsFilterSkip + 2 * st::dialogsFilterPadding.x(),
		st::dialogsFilterSkip + 2 * st::dialogsFilterPadding.x());
	setVisible(false);
}

void DownloadCenterBar::setActiveCount(int count) {
	const auto newVisible = (count > 0);
	if (count != _activeCount || newVisible != isVisible()) {
		_activeCount = count;
		setVisible(newVisible);
		update();
	}
}

void DownloadCenterBar::paintEvent(QPaintEvent *e) {
	RippleButton::paintEvent(e);
	auto p = QPainter(this);
	p.setPen(st::dialogsMenuIconFg);
	const auto r = rect();
	const auto iconSize = std::min(r.width(), r.height()) - 2 * 6;
	const auto iconRect = QRect(
		(r.width() - iconSize) / 2,
		(r.height() - iconSize) / 2,
		iconSize,
		iconSize);
	p.setFont(st::semiboldTextStyle.font);
	p.drawText(iconRect, Qt::AlignCenter, u"DL"_q);

	if (_activeCount > 0) {
		const auto badgeSize = 14;
		const auto badgeRect = QRect(
			r.width() - badgeSize - 1,
			1,
			badgeSize,
			badgeSize);
		p.setPen(Qt::NoPen);
		p.setBrush(st::dialogsUnreadBg);
		p.drawEllipse(badgeRect);
		p.setPen(st::dialogsMenuIconFg);
		p.setFont(st::semiboldTextStyle.font);
		p.drawText(badgeRect, Qt::AlignCenter,
			QString::number(std::min(_activeCount, 99)));
	}
}

} // namespace Ui