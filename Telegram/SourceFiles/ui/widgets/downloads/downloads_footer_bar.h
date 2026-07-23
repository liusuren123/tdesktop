/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"
#include "rpl/event_stream.h"

#include <QtCore/QString>
#include <QtCore/QPoint>

namespace Ui {

// Hand-painted footer bar for the Downloads page.
//
// Layout (left → right):
//   [blue dot] [count text] <stretch> [Pause all] [Cancel all]
//
// Drawing + hit-testing live entirely in paintEvent / mousePressEvent —
// no child widgets, so we don't depend on QLabel/FlatLabel/child layout
// behaviour inside the wrap widget.
class DownloadsFooterBar : public RpWidget {
public:
	DownloadsFooterBar(QWidget *parent);

	void setCountText(const QString &text);

	[[nodiscard]] rpl::producer<> pauseAllClicks() const;
	[[nodiscard]] rpl::producer<> cancelAllClicks() const;

protected:
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	enum class Action {
		None,
		Pause,
		Cancel,
	};

	struct ButtonRect {
		QRect rect;
		Action action;
	};

	[[nodiscard]] std::array<ButtonRect, 2> buttons() const;
	[[nodiscard]] Action actionAt(const QPoint &pos) const;
	void updateHover(const QPoint &pos);

	QString _countText;
	Action _hover = Action::None;
	rpl::event_stream<> _pauseAllClicks;
	rpl::event_stream<> _cancelAllClicks;

};

} // namespace Ui