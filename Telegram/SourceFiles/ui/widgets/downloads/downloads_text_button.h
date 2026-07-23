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

class DownloadsTextButton : public AbstractButton {
public:
	DownloadsTextButton(
		QWidget *parent,
		const style::icon *icon,
		const QString &text,
		bool withChevron = false);

	void setText(const QString &text);

	[[nodiscard]] QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	void updateHover();

	const style::icon *_icon = nullptr;
	QString _text;
	bool _withChevron = false;
	bool _hover = false;

};

} // namespace Ui