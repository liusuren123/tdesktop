/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/abstract_button.h"
#include "styles/style_downloads_icons.h"

#include <QtCore/QObject>
#include <QtWidgets/QLineEdit>

namespace Ui {

class DownloadsSearch : public AbstractButton {
	Q_OBJECT

public:
	DownloadsSearch(QWidget *parent);

	[[nodiscard]] QString text() const;
	void setText(const QString &text);

	[[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
	void textChanged(const QString &text);

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	QLineEdit *_edit = nullptr;

};

} // namespace Ui