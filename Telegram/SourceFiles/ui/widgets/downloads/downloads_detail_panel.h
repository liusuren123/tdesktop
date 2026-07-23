/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"
#include "ui/widgets/downloads/downloads_sample.h"

#include <rpl/event_stream.h>

namespace Ui {

// Right-hand inspector panel shown when a grid card is selected.
//
//   ┌──────────────────────┐
//   │ File Info         ✕  │  header
//   │      ┌──────┐        │
//   │      │ MP3  │        │  file-type abbreviation (coloured)
//   │      └──────┘        │
//   │ Podcast_Episode_…    │  filename
//   │ Size       54.7 MB   │
//   │ Type       MP3 file  │  metadata rows
//   │ …                    │
//   │ ┌──────────────────┐ │
//   │ │  ↓ Resume Downl. │ │  primary action (state-aware)
//   │ └──────────────────┘ │
//   │   ↗ Forward         │  secondary actions
//   │   🗑 Remove          │
//   └──────────────────────┘
//
// Hand-painted (no child widgets) for consistency with the footer/grid.
class DownloadsDetailPanel : public RpWidget {
public:
	DownloadsDetailPanel(QWidget *parent);

	void setRow(const Sample::Row &row);
	void clear();

	[[nodiscard]] rpl::producer<> closeClicks() const;
	[[nodiscard]] rpl::producer<> primaryClicks() const;
	[[nodiscard]] rpl::producer<> forwardClicks() const;
	[[nodiscard]] rpl::producer<> removeClicks() const;

protected:
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	enum class Hit {
		None,
		Close,
		Primary,
		Forward,
		Remove,
	};

	void updateHover(const QPoint &pos);
	[[nodiscard]] Hit hitTest(const QPoint &pos) const;
	[[nodiscard]] QRect primaryRect() const;
	[[nodiscard]] QRect secondaryRect(int index) const;

	Sample::Row _row;
	bool _hasRow = false;
	Hit _hover = Hit::None;

	rpl::event_stream<> _closeClicks;
	rpl::event_stream<> _primaryClicks;
	rpl::event_stream<> _forwardClicks;
	rpl::event_stream<> _removeClicks;

};

} // namespace Ui
