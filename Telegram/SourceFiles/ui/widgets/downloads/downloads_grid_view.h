/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "rpl/event_stream.h"
#include "rpl/producer.h"
#include "ui/rp_widget.h"
#include "ui/widgets/downloads/downloads_sample.h"

#include <QtCore/QRect>
#include <vector>

namespace Ui {

// Card grid view for the Downloads page (the "table/grid" toggle's grid mode).
//
// Each card is a hand-painted rounded surface:
//
//   ┌──────────────────────┐
//   │                      │  ← thumbnail (photo fill / video + duration
//   │         PDF          │     badge / document abbreviation box)
//   │                      │
//   ├──────────────────────┤
//   │ filename.png         │  ← filename (semibold)
//   │ 3.2 MB        Done   │  ← size (left) + status pill (right)
//   │ Today, 14:32         │  ← date (dim)
//   └──────────────────────┘
//
// Cards flow left-to-right, wrapping into rows; column count is derived from
// the available width (downloadsCardMinWidth + downloadsCardGap). The widget
// reports its own height so it can sit inside a QScrollArea. Clicking a card
// fires `cardClicked(index)`; the selected card gets an accent ring.
class DownloadsGridView : public RpWidget {
public:
	DownloadsGridView(QWidget *parent);

	void setRows(std::vector<Sample::Row> rows);
	void setSelectedIndex(int index);

	[[nodiscard]] rpl::producer<int> cardClicked() const;

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;

private:
	void reflow();

	void paintCard(
		QPainter *p,
		const QRect &card,
		const Sample::Row &row,
		bool selected) const;
	void paintThumbnail(
		QPainter *p,
		const QRect &thumb,
		const Sample::Row &row) const;
	void paintStatusPill(
		QPainter *p,
		const QRect &rowArea,
		const Sample::Row &row) const;

	std::vector<Sample::Row> _rows;
	std::vector<QRect> _cardRects;
	int _selectedIndex = -1;

	rpl::event_stream<int> _cardClicked;

};

} // namespace Ui
