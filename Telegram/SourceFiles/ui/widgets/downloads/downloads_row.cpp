/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_row.h"

#include "ui/widgets/labels.h"
#include "lang/lang_keys.h"
#include "ui/painter.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/downloads/downloads_style.h"
#include "styles/style_chat.h"
#include "styles/style_downloads_icons.h"

#include <QtWidgets/QPushButton>

#include <QtGui/QContextMenuEvent>
#include <QtGui/QResizeEvent>

namespace Ui {

DownloadsRow::DownloadsRow(QWidget *parent)
: RpWidget(parent) {
	resize(0, st::downloadsRowHeight + st::downloadsRowOuterPadding * 2);
	setupLabels();
	setMouseTracking(true);
}

QSize DownloadsRow::sizeHint() const {
	return QSize(0, st::downloadsRowHeight + st::downloadsRowOuterPadding * 2);
}

int DownloadsRow::heightForWidth(int width) const {
	return st::downloadsRowHeight + st::downloadsRowOuterPadding * 2;
}

void DownloadsRow::setupLabels() {
	_icon = Ui::CreateChild<DownloadsFileIcon>(this);
	_icon->move(st::downloadsRowHorizontalPadding, st::downloadsRowOuterPadding);

	_nameWrap = Ui::CreateChild<QWidget>(this);
	_name = Ui::CreateChild<FlatLabel>(_nameWrap, st::downloadsRowName);
	_source = Ui::CreateChild<FlatLabel>(_nameWrap, st::downloadsRowSource);

	_badge = Ui::CreateChild<DownloadsStatusBadge>(this);
	_statusText = Ui::CreateChild<FlatLabel>(this, st::downloadsRowStatus);
	_size = Ui::CreateChild<FlatLabel>(this, st::downloadsRowSize);
	_date = Ui::CreateChild<FlatLabel>(this, st::downloadsRowDate);

	const auto actionStyle = QStringLiteral(
		"QPushButton { background: transparent; color: #888; border: none; font-size: 14px; }"
		" QPushButton:hover { color: #fff; }");
	_actionSave = Ui::CreateChild<QPushButton>(this);
	_actionSave->setText(QString::fromUtf8("\xE2\xAC\x87"));
	_actionSave->setFixedSize(28, 28);
	_actionSave->setStyleSheet(actionStyle);

	_actionOpenFolder = Ui::CreateChild<QPushButton>(this);
	_actionOpenFolder->setText(QString::fromUtf8("\xF0\x9F\x93\x81"));
	_actionOpenFolder->setFixedSize(28, 28);
	_actionOpenFolder->setStyleSheet(actionStyle);

	_actionRemove = Ui::CreateChild<QPushButton>(this);
	_actionRemove->setText(QString::fromUtf8("\xF0\x9F\x97\x91"));
	_actionRemove->setFixedSize(28, 28);
	_actionRemove->setStyleSheet(actionStyle);

	connect(_actionRemove, &QPushButton::clicked, this, [this] {
		_row.fileName.clear();
		_row.percent = 0;
		applySample();
	});
}

void DownloadsRow::setSample(const Sample::Row &row) {
	_row = row;
	applySample();
}

void DownloadsRow::applySample() {
	_icon->setSample(_row);

	_name->setText(_row.fileName);

	auto sourceText = u"from %1 · %2"_q.arg(_row.chatName).arg(_row.context);
	_source->setText(sourceText);

	_badge->setState(_row.state, _row.percent);

	switch (_row.state) {
	case Sample::State::Downloading:
		_statusText->setText(QString::number(_row.percent) + u"%"_q);
		break;
	case Sample::State::Paused:
		_statusText->setText(tr::lng_downloads_status_paused(tr::now)
			+ u" · "_q
			+ QString::number(_row.percent)
			+ u"%"_q);
		break;
	case Sample::State::Failed:
		_statusText->setText(tr::lng_downloads_status_failed(tr::now));
		break;
	case Sample::State::Completed:
	default:
		_statusText->setText(tr::lng_downloads_status_done(tr::now));
		break;
	}

	_size->setText(_row.sizeText);
	_date->setText(_row.dateText);

	updateGeometry();
	update();
}

void DownloadsRow::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);

	auto hq = PainterHighQualityEnabler(p);
	const auto borderY = height() - 1;
	p.setPen(QPen(DownloadsStyle::rowBorder(), st::downloadsRowBorderWidth));
	p.drawLine(0, borderY, width(), borderY);

	if (_row.state == Sample::State::Downloading) {
		const auto trackY = height() - st::downloadsRowOuterPadding - st::downloadsProgressHeight;
		const auto track = QRectF(
			0,
			trackY,
			width(),
			st::downloadsProgressHeight);
		p.fillRect(track, DownloadsStyle::rowBorder());

		const auto fillWidth = (width() * _row.percent) / 100;
		p.fillRect(
			QRectF(0, trackY, fillWidth, st::downloadsProgressHeight),
			DownloadsStyle::progressFill());
	}
}

void DownloadsRow::mousePressEvent(QMouseEvent *e) {
	if (e->button() == Qt::RightButton) {
		contextMenuEvent(nullptr);
	}
}

void DownloadsRow::contextMenuEvent(QContextMenuEvent *e) {
	const auto guard = Ui::CreateChild<QWidget>(this);
	guard->setAttribute(Qt::WA_TransparentForMouseEvents);
	guard->show();

	const auto menu = std::make_unique<Ui::PopupMenu>(
		this,
		st::popupMenuExpandedSeparator);

	switch (_row.state) {
	case Sample::State::Downloading:
		menu->addAction(tr::lng_downloads_menu_pause(tr::now), [this] {
			_row.state = Sample::State::Paused;
			applySample();
		});
		break;
	case Sample::State::Paused:
		menu->addAction(tr::lng_downloads_menu_resume(tr::now), [this] {
			_row.state = Sample::State::Downloading;
			applySample();
		});
		break;
	case Sample::State::Failed:
		menu->addAction(tr::lng_downloads_menu_retry(tr::now), [this] {
			_row.state = Sample::State::Downloading;
			applySample();
		});
		break;
	case Sample::State::Completed:
		break;
	}

	menu->addAction(tr::lng_downloads_menu_open(tr::now), [] {});
	menu->addAction(tr::lng_downloads_menu_show_in_folder(tr::now), [] {});
	menu->addAction(tr::lng_downloads_menu_copy_link(tr::now), [] {});
	menu->addAction(tr::lng_downloads_menu_remove(tr::now), [this] {
		_row.fileName.clear();
		_row.percent = 0;
		applySample();
	});

	menu->popup(QCursor::pos());
}

void DownloadsRow::resizeEvent(QResizeEvent *e) {
	RpWidget::resizeEvent(e);

	const auto y = st::downloadsRowOuterPadding;
	const auto rowH = st::downloadsRowHeight;

	_icon->setGeometry(
		st::downloadsRowHorizontalPadding,
		y,
		st::downloadsThumbnailSize,
		st::downloadsThumbnailSize);

	const auto actionsX = width() - st::downloadsRowHorizontalPadding - st::downloadsActionsColumnWidth;
	const auto sizeX = actionsX - st::downloadsSizeColumnWidth;
	const auto dateX = actionsX - st::downloadsSizeColumnWidth - st::downloadsDateColumnWidth;
	const auto nameX = st::downloadsRowHorizontalPadding + st::downloadsThumbnailSize + st::downloadsTabGap;

	_statusText->resizeToWidth(st::downloadsSizeColumnWidth);
	_statusText->move(sizeX, y + rowH - st::downloadsRowStatusHeight);

	_badge->move(dateX + st::downloadsDateColumnWidth - st::downloadsBadgeHeight,
		y + rowH - st::downloadsBadgeHeight - 2);

	_nameWrap->setGeometry(nameX, y,
		std::max(0, sizeX - nameX - st::downloadsTabGap), rowH);
	_name->resizeToWidth(_nameWrap->width());
	_name->moveToLeft(0, 0);
	_source->resizeToWidth(_nameWrap->width());
	_source->moveToLeft(0, _name->height() + 2);

	_size->resizeToWidth(st::downloadsSizeColumnWidth);
	_size->move(sizeX, y + (rowH - _size->height()) / 2 - st::downloadsRowStatusHeight - 2);

	_date->resizeToWidth(st::downloadsDateColumnWidth);
	_date->move(dateX, y + (rowH - _date->height()) / 2 - st::downloadsRowStatusHeight - 2);

	const auto actionY = y + (rowH - 28) / 2 - st::downloadsRowStatusHeight;
	_actionRemove->move(actionsX + st::downloadsActionsColumnWidth - 28, actionY);
	_actionOpenFolder->move(actionsX + st::downloadsActionsColumnWidth - 60, actionY);
	_actionSave->move(actionsX, actionY);
}

} // namespace Ui