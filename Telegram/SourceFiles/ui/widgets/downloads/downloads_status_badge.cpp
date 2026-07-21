/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_status_badge.h"

#include "lang/lang_keys.h"
#include "styles/style_downloads_icons.h"
#include "ui/painter.h"
#include "ui/widgets/downloads/downloads_style.h"

namespace Ui {

DownloadsStatusBadge::DownloadsStatusBadge(QWidget *parent)
: RpWidget(parent) {
	resize(0, st::downloadsBadgeHeight);
	setAttribute(Qt::WA_TransparentForMouseEvents);
}

QSize DownloadsStatusBadge::sizeHint() const {
	return QSize(0, st::downloadsBadgeHeight);
}

void DownloadsStatusBadge::setState(Sample::State state, int percent) {
	if (_state == state && _percent == percent) {
		return;
	}
	_state = state;
	_percent = percent;
	update();
}

void DownloadsStatusBadge::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	const auto text = [&]() -> QString {
		switch (_state) {
		case Sample::State::Downloading:
			return u"%1%"_q.arg(_percent);
		case Sample::State::Paused:
			return tr::lng_downloads_status_paused(tr::now);
		case Sample::State::Failed:
			return tr::lng_downloads_status_failed(tr::now);
		case Sample::State::Completed:
		default:
			return tr::lng_downloads_status_done(tr::now);
		}
	}();

	QColor bg;
	QColor fg;
	switch (_state) {
	case Sample::State::Downloading: bg = DownloadsStyle::activeBg();  fg = DownloadsStyle::activeFg();  break;
	case Sample::State::Paused:     bg = DownloadsStyle::pausedBg();  fg = DownloadsStyle::pausedFg();  break;
	case Sample::State::Failed:     bg = DownloadsStyle::failedBg();  fg = DownloadsStyle::failedFg();  break;
	case Sample::State::Completed:
	default:                        bg = DownloadsStyle::doneBg();    fg = DownloadsStyle::doneFg();    break;
	}

	p.setFont(st::downloadsBadgeFont->f);
	const auto metrics = QFontMetrics(st::downloadsBadgeFont->f);
	const auto hPad = st::downloadsBadgePadding;
	const auto vPad = (height() - metrics.height()) / 2;
	const auto textWidth = metrics.horizontalAdvance(text);
	const auto r = QRectF(hPad, vPad, textWidth + hPad, metrics.height());

	auto hq = PainterHighQualityEnabler(p);
	QPainterPath path;
	path.addRoundedRect(r, r.height() / 2.0, r.height() / 2.0);
	p.fillPath(path, bg);

	p.setPen(fg);
	p.drawText(r.adjusted(hPad, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, text);

	const auto newWidth = int(r.right() + hPad);
	if (newWidth != width()) {
		resize(newWidth, height());
	}
}

} // namespace Ui