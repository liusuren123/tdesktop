/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/widgets/downloads/downloads_sample.h"
#include "ui/rp_widget.h"

namespace Ui {

class DownloadsStatusBadge : public RpWidget {
public:
	DownloadsStatusBadge(QWidget *parent);

	void setState(Sample::State state, int percent);

	[[nodiscard]] QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	Sample::State _state = Sample::State::Completed;
	int _percent = 100;

};

} // namespace Ui