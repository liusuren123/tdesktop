/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/widgets/downloads/downloads_sample.h"

#include <QtCore/QAbstractTableModel>
#include <QtCore/QStringList>
#include <vector>

namespace Ui {

// MVC table model backing the Downloads list view.
// Five logical columns; rendering is handled entirely by
// DownloadsRowDelegate (a single column paints the whole row).
class DownloadsTableModel : public QAbstractTableModel {
	Q_OBJECT
public:
	enum Column {
		NameColumn = 0,
		ProgressColumn,
		SizeColumn,
		DateColumn,
		ActionsColumn,
		ColumnCount,
	};

	enum Role {
		RowDataRole = Qt::UserRole + 1,
	};

	explicit DownloadsTableModel(QObject *parent = nullptr);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	// Replace the entire visible row set.
	void setRows(std::vector<Sample::Row> rows);
	void setHeaderLabels(const QStringList &labels);

	[[nodiscard]] const std::vector<Sample::Row> &rows() const { return _rows; }
	std::vector<Sample::Row> &rows() { return _rows; }
	[[nodiscard]] const Sample::Row &rowAt(int row) const;

private:
	std::vector<Sample::Row> _rows;
	QStringList _headerLabels;
};

} // namespace Ui