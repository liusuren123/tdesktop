/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/widgets/downloads/downloads_view_toggle.h"
#include "styles/style_downloads_icons.h"

#include <QtWidgets/QWidget>

namespace Ui {

// Two-button segmented control: index 0 = grid, index 1 = list.
// The active segment is highlighted and `activeChanged` lets the host swap
// the content view.
class DownloadsViewToggleGroup : public QWidget {
	Q_OBJECT
public:
	DownloadsViewToggleGroup(QWidget *parent);

	void setActive(int index);
	[[nodiscard]] int active() const { return _active; }

	[[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
	void activeChanged(int index);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	DownloadsViewToggle *_a = nullptr;
	DownloadsViewToggle *_b = nullptr;
	int _active = 1; // default: list view

};

} // namespace Ui

