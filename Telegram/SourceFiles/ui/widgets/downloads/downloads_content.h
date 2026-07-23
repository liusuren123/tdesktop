/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/required.h"
#include "ui/widgets/downloads/downloads_row_delegate.h"
#include "ui/widgets/downloads/downloads_sample.h"
#include "rpl/lifetime.h"

#include <QtCore/QString>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLayout>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QTableView>
#include <QtWidgets/QWidget>
#include <map>
#include <memory>
#include <vector>

namespace base {
class Timer;
} // namespace base

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace Data {
class DownloadCenter;
class DocumentMedia;
struct DownloadTask;
} // namespace Data

namespace Ui {

class FlatLabel;
class DownloadsSearch;
class DownloadsTextButton;
class DownloadsViewToggleGroup;
class DownloadsGridView;
class DownloadsDetailPanel;
class DownloadsTabButton;
class DownloadsTableModel;
class DownloadsFooterBar;
class PopupMenu;

// MVC-based Downloads page.
//
// The whole page is assembled with QLayouts — no hand-rolled coordinate
// math in resizeEvent. The central table is a QTableView backed by a
// QAbstractTableModel (DownloadsTableModel) and a QStyledItemDelegate
// (DownloadsRowDelegate).
class DownloadsContent : public QWidget {
public:
	DownloadsContent(
		QWidget *parent,
		not_null<Window::SessionController*> controller);
	~DownloadsContent() override;

	void pauseAll();
	void cancelAll();

protected:
	bool eventFilter(QObject *obj, QEvent *e) override;
	void paintEvent(QPaintEvent *e) override;

private:
	enum class SortKey {
		Date,
		Name,
		Size,
		Sender,
	};

	struct Tab {
		QString label;
		bool (*match)(Sample::Kind) = nullptr;
	};

	static bool MatchPhoto(Sample::Kind k);
	static bool MatchVideo(Sample::Kind k);
	static bool MatchFile(Sample::Kind k);
	static bool MatchMusic(Sample::Kind k);
	static bool MatchLink(Sample::Kind k);
	static bool MatchVoice(Sample::Kind k);

	void setupUi();
	void setupHeader(QVBoxLayout *root);
	void setupTabs(QVBoxLayout *root);
	void setupTable(QHBoxLayout *content);
	void setupGrid(QHBoxLayout *content);
	void setupFooter(QVBoxLayout *root);

	// Live backend binding.
	void refreshFromBackend();
	[[nodiscard]] Sample::Row buildUiTask(const Data::DownloadTask &task);
	void scheduleRefresh();

	void setViewMode(int toggleIndex);
	void selectItem(int index);
	void closeDetailPanel();
	void rebuildModel();
	int countFor(int tabIndex) const;
	int countTotal() const;
	QString subtitleLabel() const;
	QString activeCountLabel() const;
	void cycleSortKey();
	void showSortMenu();
	void setSortKey(SortKey key);
	[[nodiscard]] QString sortLabel(SortKey key) const;
	void onRowAction(int row, DownloadsRowDelegate::ActionHit action);

	// Root model data.
	std::vector<Sample::Row> _rows;
	int _activeTab = 0;
	SortKey _sortKey = SortKey::Date;
	QString _search;

	std::vector<Tab> _tabs;

	// Header widgets.
	FlatLabel *_title = nullptr;
	FlatLabel *_subtitle = nullptr;
	DownloadsSearch *_searchEdit = nullptr;
	DownloadsTextButton *_sortButton = nullptr;
	DownloadsViewToggleGroup *_viewToggle = nullptr;
	std::vector<DownloadsTabButton*> _tabButtons;

	// Table.
	QTableView *_table = nullptr;
	QHeaderView *_header = nullptr;
	DownloadsTableModel *_model = nullptr;
	DownloadsRowDelegate *_delegate = nullptr;
	QWidget *_emptyOverlay = nullptr;

	// Content row: holds the list table, the grid scroll area, and the
	// detail panel. Exactly one of `_table` / `_gridScroll` is visible at a
	// time (driven by the view toggle); the detail panel is independent so
	// item selection works in either mode.
	QWidget *_contentRow = nullptr;
	QScrollArea *_gridScroll = nullptr;
	DownloadsGridView *_gridView = nullptr;
	DownloadsDetailPanel *_detailPanel = nullptr;
	int _viewMode = 1; // 0 = grid, 1 = list (matches the toggle)
	int _selectedRow = -1; // index into the filtered model
	uint64 _selectedId = 0; // backing DownloadTaskId (0 = none)

	// Live download backend (per-session). Resolved once from the controller.
	Main::Session *_session = nullptr;
	Data::DownloadCenter *_dc = nullptr;
	std::unique_ptr<base::Timer> _refreshTimer;
	// Keeps DocumentMedia views alive per task so preview thumbnails load and
	// stay cached across rebuilds. Keyed by DownloadTaskId.
	std::map<uint64, std::shared_ptr<Data::DocumentMedia>> _documentMedia;

	rpl::lifetime _lifetime;

	// Footer (hand-painted; no child widgets).
	DownloadsFooterBar *_footerBar = nullptr;

	// Live sort dropdown — kept alive while shown so the local
	// unique_ptr in showSortMenu() doesn't tear it down on return.
	std::unique_ptr<Ui::PopupMenu> _activeSortMenu;

};

} // namespace Ui