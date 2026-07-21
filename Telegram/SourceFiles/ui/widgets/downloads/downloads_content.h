/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/widgets/downloads/downloads_sample.h"
#include "ui/widgets/downloads/downloads_row.h"
#include "ui/widgets/scroll_area.h"

#include <QtCore/QString>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>
#include <vector>

namespace Ui {

class FlatLabel;
class DownloadsRow;

class DownloadsTabButton;

class DownloadsContent : public QWidget {
public:
	DownloadsContent(QWidget *parent);

	[[nodiscard]] QSize sizeHint() const override;

	void pauseAll();
	void cancelAll();

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	enum class SortKey {
		Date,
		Size,
		Name,
	};

	struct Tab {
		QString label;
		bool (*match)(Sample::Kind) = nullptr;
	};

	static bool MatchPhoto(Sample::Kind k)   { return k == Sample::Kind::Photo; }
	static bool MatchVideo(Sample::Kind k)   { return k == Sample::Kind::Video; }
	static bool MatchFile(Sample::Kind k)    {
		return k == Sample::Kind::Document || k == Sample::Kind::Archive;
	}
	static bool MatchMusic(Sample::Kind k)   { return k == Sample::Kind::Audio; }
	static bool MatchLink(Sample::Kind k)    { return k == Sample::Kind::Link; }
	static bool MatchVoice(Sample::Kind k)   { return k == Sample::Kind::Voice; }

	void rebuildVisible();
	void setupHeader();
	void setupTabs();
	void setupColumnHeaders();
	void setupFooter();
	void setupList();

	int countFor(int tabIndex) const;
	int countTotal() const;
	QString activeCountLabel() const;
	QString subtitleLabel() const;
	void updateSubtitle();
	void cycleSortKey();

	std::vector<Sample::Row> _rows;
	std::vector<int> _visible;
	int _activeTab = 0;
	SortKey _sortKey = SortKey::Date;
	QString _search;

	std::vector<Tab> _tabs;

	FlatLabel *_title = nullptr;
	FlatLabel *_subtitle = nullptr;
	QLineEdit *_searchEdit = nullptr;
	QPushButton *_sortButton = nullptr;
	QPushButton *_viewToggleA = nullptr;
	QPushButton *_viewToggleB = nullptr;
	std::vector<DownloadsTabButton*> _tabButtons;

	FlatLabel *_colName = nullptr;
	FlatLabel *_colProgress = nullptr;
	FlatLabel *_colSize = nullptr;
	FlatLabel *_colDate = nullptr;
	FlatLabel *_colActions = nullptr;

	Ui::ScrollArea *_scroll = nullptr;
	QWidget *_listHost = nullptr;
	std::vector<DownloadsRow*> _rowWidgets;

	QLabel *_footerDot = nullptr;
	FlatLabel *_footerCount = nullptr;
	QPushButton *_pauseAll = nullptr;
	QPushButton *_cancelAll = nullptr;

	FlatLabel *_emptyTitle = nullptr;
	FlatLabel *_emptySubtitle = nullptr;

};

} // namespace Ui