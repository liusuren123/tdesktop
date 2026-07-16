/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/widgets/buttons.h"

namespace Ui {

class DownloadCenterBar final : public Ui::RippleButton {
public:
	DownloadCenterBar(QWidget *parent);

	[[nodiscard]] int activeCount() const {
		return _activeCount;
	}
	void setActiveCount(int count);

private:
	void paintEvent(QPaintEvent *e) override;

	int _activeCount = 0;

};

} // namespace Ui