/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/effects/animations.h"
#include "ui/widgets/downloads/downloads_sample.h"

#include <QtWidgets/QStyledItemDelegate>

namespace Ui {

// Delegate that paints one Downloads row inside a QTableView.
//
// Layout (left-to-right):
//   [thumbnail] [name + source] [progress / status badge] [size] [date] [actions]
//
// The whole row is painted in a single column (the "Name" column) by
// every column delegate; layout is identical regardless of which
// logical column Qt asks about, which makes column resizing trivial.
class DownloadsRowDelegate : public QStyledItemDelegate {
	Q_OBJECT
public:
	enum ActionHit {
		NoAction,
		SaveAction,
		FolderAction,
		RemoveAction,
	};

	explicit DownloadsRowDelegate(QObject *parent = nullptr);

	QSize sizeHint(
		const QStyleOptionViewItem &option,
		const QModelIndex &index) const override;
	void paint(
		QPainter *painter,
		const QStyleOptionViewItem &option,
		const QModelIndex &index) const override;
	bool editorEvent(
		QEvent *event,
		QAbstractItemModel *model,
		const QStyleOptionViewItem &option,
		const QModelIndex &index) override;

	// Map a mouse position (in cell-relative coords) to an action button.
	static ActionHit hitAction(const QPoint &pos, const QRect &cellRect);

	// Row that currently has the detail panel open — gets an accent highlight.
	void setSelectedRow(int row) { _selectedRow = row; }
	// Row under the cursor — drives the animated hover wash. Pass -1 on leave.
	void setHoveredRow(int row);

Q_SIGNALS:
	void saveClicked(int row);
	void folderClicked(int row);
	void removeClicked(int row);
	void rowContextMenu(int row, const QPoint &globalPos);
	void rowClicked(int row);

private:
	void paintThumbnail(
		QPainter *painter,
		const QRect &r,
		const Sample::Row &row) const;
	void paintNameBlock(
		QPainter *painter,
		const QRect &cell,
		const Sample::Row &row) const;
	void paintProgressColumn(
		QPainter *painter,
		const QRect &cell,
		const Sample::Row &row) const;
	void paintStatusBadge(
		QPainter *painter,
		const QRect &area,
		const Sample::Row &row,
		bool rightAligned) const;
	void paintSizeColumn(QPainter *painter, const QRect &r, const Sample::Row &row) const;
	void paintDateColumn(QPainter *painter, const QRect &r, const Sample::Row &row) const;
	void paintActionsColumn(
		QPainter *painter,
		const QRect &r,
		const Sample::Row &row,
		bool hovered,
		const QPoint &hoverPos) const;

	void hoverTick();

	int _selectedRow = -1;
	int _hoveredRow = -1;
	float _hoverShown = 0.f;
	Ui::Animations::Simple _hoverAnim;
	mutable QAbstractItemView *_view = nullptr;
};

} // namespace Ui