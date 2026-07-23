/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_search.h"

#include "styles/style_widgets.h"
#include "ui/painter.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QMouseEvent>

namespace Ui {
namespace {

constexpr int kPaddingH = 12;
constexpr int kPaddingL = 32;
constexpr int kPaddingTop = 7;
constexpr int kPaddingBottom = 7;
constexpr int kIconSize = 14;
constexpr int kIconOffsetX = 12;
constexpr int kRadius = 8;

} // namespace

DownloadsSearch::DownloadsSearch(QWidget *parent)
: AbstractButton(parent) {
	setCursor(Qt::IBeamCursor);
	setFixedHeight(int(st::downloadsSearchHeight));
	setMinimumWidth(int(st::downloadsSearchWidth));

	_edit = Ui::CreateChild<QLineEdit>(this);
	_edit->setFrame(false);
	_edit->setAttribute(Qt::WA_TranslucentBackground);
	_edit->setStyleSheet(QString(
		"QLineEdit { background: transparent; color: white;"
		" selection-background-color: #2AABEE;"
		" selection-color: white; }"
	));
	_edit->setFont(st::downloadsHeaderTitle.style.font);
	_edit->setPlaceholderText(
		QStringLiteral("Search downloads…"));
	connect(_edit, &QLineEdit::textChanged, this, &DownloadsSearch::textChanged);
}

QString DownloadsSearch::text() const {
	return _edit ? _edit->text() : QString();
}

void DownloadsSearch::setText(const QString &text) {
	if (_edit) _edit->setText(text);
}

QSize DownloadsSearch::sizeHint() const {
	return QSize(
		int(st::downloadsSearchWidth),
		int(st::downloadsSearchHeight));
}

void DownloadsSearch::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	p.setRenderHint(QPainter::Antialiasing);
	p.setBrush(QColor(255, 255, 255, 15));
	p.setPen(Qt::NoPen);
	p.drawRoundedRect(
		QRectF(0, 0, width(), height()).adjusted(0.5, 0.5, -0.5, -0.5),
		kRadius,
		kRadius);

	const auto iconY = (height() - kIconSize) / 2.0;
	st::downloadsSearch.paint(
		p,
		kIconOffsetX,
		int(iconY),
		width(),
		QColor(255, 255, 255, 128));
}

void DownloadsSearch::resizeEvent(QResizeEvent *e) {
	_edit->setGeometry(
		kPaddingL,
		kPaddingTop,
		width() - kPaddingL - kPaddingH,
		height() - kPaddingTop - kPaddingBottom);
}

} // namespace Ui