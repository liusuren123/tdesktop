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

class DownloadsFileIcon : public RpWidget {
public:
	DownloadsFileIcon(QWidget *parent);

	void setSample(const Sample::Row &row);
	[[nodiscard]] QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	Sample::Kind _kind = Sample::Kind::Document;
	QString _fileName;
	bool _isPlaying = false;

};

} // namespace Ui