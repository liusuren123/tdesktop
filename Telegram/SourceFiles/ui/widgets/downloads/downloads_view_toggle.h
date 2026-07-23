/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/abstract_button.h"
#include "styles/style_downloads_icons.h"

namespace Ui {

class DownloadsViewToggle : public AbstractButton {
public:
	DownloadsViewToggle(
		QWidget *parent,
		const style::icon *icon);

	void setActive(bool active);

	[[nodiscard]] QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	const style::icon *_icon = nullptr;
	bool _hover = false;
	bool _active = false;

};

} // namespace Ui
