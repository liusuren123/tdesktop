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

class DownloadsTabButton : public AbstractButton {
public:
	DownloadsTabButton(QWidget *parent, const style::icon &icon, const QString &text);

	void setText(const QString &text);
	[[nodiscard]] QString text() const { return _text; }

	void setActive(bool active);
	[[nodiscard]] bool active() const { return _active; }

	void setCount(int count);

	[[nodiscard]] QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

private:
	[[nodiscard]] QRect contentRect() const;

	const style::icon &_icon;
	QString _text;
	bool _active = false;
	int _count = 0;

};

} // namespace Ui