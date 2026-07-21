/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/widgets/downloads/downloads_file_icon.h"
#include "ui/widgets/downloads/downloads_status_badge.h"
#include "ui/widgets/downloads/downloads_sample.h"
#include "ui/rp_widget.h"

#include <QtCore/QString>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QResizeEvent>

namespace Ui {

class FlatLabel;

class DownloadsRow : public RpWidget {
public:
	DownloadsRow(QWidget *parent);

	void setSample(const Sample::Row &row);

	[[nodiscard]] QSize sizeHint() const override;
	[[nodiscard]] int heightForWidth(int width) const override;

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;

private:
	void setupLabels();
	void applySample();

	Sample::Row _row;

	DownloadsFileIcon *_icon = nullptr;
	QWidget *_nameWrap = nullptr;
	FlatLabel *_name = nullptr;
	FlatLabel *_source = nullptr;
	DownloadsStatusBadge *_badge = nullptr;
	FlatLabel *_size = nullptr;
	FlatLabel *_date = nullptr;

};

} // namespace Ui