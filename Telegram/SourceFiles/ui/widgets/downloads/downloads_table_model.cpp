/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_table_model.h"

#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>

namespace Ui {

DownloadsTableModel::DownloadsTableModel(QObject *parent)
: QAbstractTableModel(parent) {
}

int DownloadsTableModel::rowCount(const QModelIndex &parent) const {
	if (parent.isValid()) return 0;
	return int(_rows.size());
}

int DownloadsTableModel::columnCount(const QModelIndex &parent) const {
	if (parent.isValid()) return 0;
	return ColumnCount;
}

QVariant DownloadsTableModel::data(const QModelIndex &index, int role) const {
	if (!index.isValid()) return {};
	if (index.row() < 0 || index.row() >= int(_rows.size())) return {};
	if (index.column() < 0 || index.column() >= ColumnCount) return {};

	if (role == RowDataRole || role == Qt::DisplayRole) {
		// Any column carries the row payload — the delegate renders
		// the full row regardless of which column Qt asks about.
		return QVariant::fromValue<int>(index.row());
	}
	return {};
}

QVariant DownloadsTableModel::headerData(
		int section,
		Qt::Orientation orientation,
		int role) const {
	if (role != Qt::DisplayRole) return {};
	if (orientation != Qt::Horizontal) return {};
	if (section < 0 || section >= _headerLabels.size()) return {};
	return _headerLabels.at(section);
}

void DownloadsTableModel::setRows(std::vector<Sample::Row> rows) {
	beginResetModel();
	_rows = std::move(rows);
	endResetModel();
}

void DownloadsTableModel::setHeaderLabels(const QStringList &labels) {
	_headerLabels = labels;
	if (auto *view = qobject_cast<QTableView*>(parent())) {
		if (auto *h = view->horizontalHeader()) {
			h->setSectionResizeMode(QHeaderView::Fixed);
		}
	}
	headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
}

const Sample::Row &DownloadsTableModel::rowAt(int row) const {
	static const Sample::Row kEmpty;
	if (row < 0 || row >= int(_rows.size())) return kEmpty;
	return _rows[row];
}

} // namespace Ui