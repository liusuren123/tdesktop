/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_content.h"

#include "lang/lang_keys.h"
#include "styles/style_downloads_icons.h"
#include "ui/painter.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/downloads/downloads_file_icon.h"
#include "ui/widgets/downloads/downloads_status_badge.h"
#include "ui/widgets/downloads/downloads_style.h"

#include <QtWidgets/QLayout>

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <algorithm>

namespace Ui {

DownloadsContent::DownloadsContent(QWidget *parent)
: QWidget(parent) {
	setAttribute(Qt::WA_OpaquePaintEvent);

	_rows = Sample::InitialRows();

	_tabs = {
		{ QString(), nullptr },
		{ QString(), &MatchPhoto },
		{ QString(), &MatchVideo },
		{ QString(), &MatchFile },
		{ QString(), &MatchMusic },
		{ QString(), &MatchLink },
		{ QString(), &MatchVoice },
	};

	setupHeader();
	setupTabs();
	setupColumnHeaders();
	setupFooter();
	setupList();

	rebuildVisible();
}

QSize DownloadsContent::sizeHint() const {
	return QSize(0, 0);
}

void DownloadsContent::setupHeader() {
	_title = Ui::CreateChild<FlatLabel>(this, st::downloadsHeaderTitle);
	_title->setText(tr::lng_downloads_title(tr::now));

	_subtitle = Ui::CreateChild<FlatLabel>(this, st::downloadsHeaderSubtitle);

	_searchEdit = Ui::CreateChild<QLineEdit>(this);
	_searchEdit->setPlaceholderText(tr::lng_downloads_search_placeholder(tr::now));
	_searchEdit->setFixedSize(st::downloadsSearchWidth, int(st::downloadsSearchHeight));
	connect(_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
		_search = text;
		rebuildVisible();
	});

	_sortButton = Ui::CreateChild<QPushButton>(this);
	_sortButton->setFixedHeight(st::downloadsSortButtonHeight);
	_sortButton->setText(tr::lng_downloads_sort_date(tr::now));
	connect(_sortButton, &QPushButton::clicked, this, [this] {
		cycleSortKey();
	});

	_viewToggleA = Ui::CreateChild<QPushButton>(this);
	_viewToggleB = Ui::CreateChild<QPushButton>(this);
	_viewToggleA->setFixedSize(
		st::downloadsViewToggleWidth / 2,
		st::downloadsViewToggleHeight);
	_viewToggleB->setFixedSize(
		st::downloadsViewToggleWidth / 2,
		st::downloadsViewToggleHeight);
	_viewToggleA->setText(u"≡"_q);
	_viewToggleB->setText(u"☰"_q);
}

void DownloadsContent::setupTabs() {
	const auto tabStyle = QStringLiteral(
		"QPushButton { background: transparent; color: %1; border: none; padding: 0 6px; border-radius: 14px; }"
		" QPushButton:hover { background: rgba(255,255,255,0.06); }"
		" QPushButton:checked { background: rgba(42,171,238,0.18); color: #2AABEE; }"
	).arg(QStringLiteral("#aaaaaa"));

	for (size_t i = 0; i < _tabs.size(); ++i) {
		auto button = Ui::CreateChild<QPushButton>(this);
		button->setCheckable(true);
		button->setAutoExclusive(true);
		button->setCursor(Qt::PointingHandCursor);
		button->setStyleSheet(tabStyle);
		const auto index = int(i);
		connect(button, &QPushButton::clicked, this, [this, index] {
			_activeTab = index;
			for (size_t j = 0; j < _tabButtons.size(); ++j) {
				_tabButtons[j]->setChecked(j == size_t(index));
			}
			rebuildVisible();
		});
		_tabButtons.push_back(button);
	}
	_tabButtons[0]->setChecked(true);

	auto labelFor = [&](int i) -> QString {
		switch (i) {
		case 0: return QString::fromUtf8("\xE2\xAC\x87 ") + tr::lng_downloads_tab_all(tr::now);
		case 1: return QString::fromUtf8("\xF0\x9F\x93\xB7 ") + tr::lng_downloads_tab_photos(tr::now);
		case 2: return QString::fromUtf8("\xF0\x9F\x8E\xAC ") + tr::lng_downloads_tab_videos(tr::now);
		case 3: return QString::fromUtf8("\xF0\x9F\x93\x84 ") + tr::lng_downloads_tab_files(tr::now);
		case 4: return QString::fromUtf8("\xF0\x9F\x8E\xB5 ") + tr::lng_downloads_tab_music(tr::now);
		case 5: return QString::fromUtf8("\xF0\x9F\x93\x8E ") + tr::lng_downloads_tab_links(tr::now);
		case 6: return QString::fromUtf8("\xF0\x9F\x8E\xA4 ") + tr::lng_downloads_tab_voice(tr::now);
		}
		return QString();
	};
	for (size_t i = 0; i < _tabs.size(); ++i) {
		_tabs[i].label = labelFor(int(i));
		_tabButtons[i]->setText(_tabs[i].label);
	}
}

void DownloadsContent::setupColumnHeaders() {
	_colName = Ui::CreateChild<FlatLabel>(this, st::downloadsColumnHeader);
	_colName->setText(tr::lng_downloads_col_name(tr::now));

	_colProgress = Ui::CreateChild<FlatLabel>(this, st::downloadsColumnHeader);
	_colProgress->setText(tr::lng_downloads_col_progress(tr::now));

	_colSize = Ui::CreateChild<FlatLabel>(this, st::downloadsColumnHeader);
	_colSize->setText(tr::lng_downloads_col_size(tr::now));

	_colDate = Ui::CreateChild<FlatLabel>(this, st::downloadsColumnHeader);
	_colDate->setText(tr::lng_downloads_col_date(tr::now));

	_colActions = Ui::CreateChild<FlatLabel>(this, st::downloadsColumnHeader);
	_colActions->setText(tr::lng_downloads_col_actions(tr::now));
}

void DownloadsContent::setupFooter() {
	_footerDot = Ui::CreateChild<QLabel>(this);
	_footerDot->setFixedSize(st::downloadsFooterDotSize, st::downloadsFooterDotSize);
	_footerDot->setStyleSheet(QString(
		"background: #2AABEE; border-radius: %1px;"
	).arg(st::downloadsFooterDotSize / 2));

	_footerCount = Ui::CreateChild<FlatLabel>(this, st::downloadsFooterCount);

	_pauseAll = Ui::CreateChild<QPushButton>(this);
	_pauseAll->setText(tr::lng_downloads_action_pause_all(tr::now));
	connect(_pauseAll, &QPushButton::clicked, this, [this] { pauseAll(); });

	_cancelAll = Ui::CreateChild<QPushButton>(this);
	_cancelAll->setText(tr::lng_downloads_action_cancel_all(tr::now));
	connect(_cancelAll, &QPushButton::clicked, this, [this] { cancelAll(); });
}

void DownloadsContent::setupList() {
	_scroll = Ui::CreateChild<Ui::ScrollArea>(this);
	_scroll->setWidgetResizable(true);
	_listHost = Ui::CreateChild<QWidget>(_scroll);
	_scroll->setWidget(_listHost);
	_listHost->setMinimumHeight(1);

	_emptyTitle = Ui::CreateChild<FlatLabel>(_listHost, st::downloadsEmptyTitle);
	_emptyTitle->setText(tr::lng_downloads_empty_no_tasks(tr::now));
	_emptyTitle->setVisible(false);

	_emptySubtitle = Ui::CreateChild<FlatLabel>(_listHost, st::downloadsEmptySubtitle);
	_emptySubtitle->setText(tr::lng_downloads_empty_no_tasks_desc(tr::now));
	_emptySubtitle->setVisible(false);
}

void DownloadsContent::rebuildVisible() {
	_visible.clear();

	for (int i = 0; i < int(_rows.size()); ++i) {
		if (_rows[i].fileName.isEmpty()) {
			continue;
		}
		const auto &row = _rows[i];
		if (_activeTab > 0
			&& _activeTab < int(_tabs.size())
			&& _tabs[_activeTab].match
			&& !_tabs[_activeTab].match(row.kind)) {
			continue;
		}
		if (!_search.isEmpty()
			&& !row.fileName.contains(_search, Qt::CaseInsensitive)) {
			continue;
		}
		_visible.push_back(i);
	}

	auto dateOf = [](const QString &text) {
		return text;
	};
	auto sizeOf = [](const QString &text) {
		return text;
	};
	auto nameOf = [](const QString &text) {
		return text;
	};

	auto sortKey = [&](int a, int b) {
		switch (_sortKey) {
		case SortKey::Date: return dateOf(_rows[a].dateText) < dateOf(_rows[b].dateText);
		case SortKey::Size: return sizeOf(_rows[a].sizeText) < sizeOf(_rows[b].sizeText);
		case SortKey::Name: return nameOf(_rows[a].fileName) < nameOf(_rows[b].fileName);
		}
		return false;
	};
	std::stable_sort(_visible.begin(), _visible.end(), sortKey);

	while (int(_rowWidgets.size()) < int(_visible.size())) {
		auto widget = Ui::CreateChild<DownloadsRow>(_listHost);
		_rowWidgets.push_back(widget);
	}
	for (size_t i = 0; i < _rowWidgets.size(); ++i) {
		if (i < _visible.size()) {
			_rowWidgets[i]->setSample(_rows[_visible[i]]);
			_rowWidgets[i]->show();
		} else {
			_rowWidgets[i]->hide();
		}
	}

	const auto empty = _visible.empty();
	_emptyTitle->setVisible(empty);
	_emptySubtitle->setVisible(empty);
	if (empty) {
		_emptyTitle->move(
			(_listHost->width() - _emptyTitle->width()) / 2,
			st::downloadsHeaderHeight + st::downloadsTabBarHeight + 32);
		_emptySubtitle->move(
			(_listHost->width() - _emptySubtitle->width()) / 2,
			_emptyTitle->y() + _emptyTitle->height() + 8);
	}

	updateSubtitle();
	_listHost->updateGeometry();
	update();
}

int DownloadsContent::countFor(int tabIndex) const {
	if (tabIndex <= 0 || tabIndex >= int(_tabs.size())) {
		return countTotal();
	}
	int n = 0;
	for (const auto &row : _rows) {
		if (row.fileName.isEmpty()) {
			continue;
		}
		if (_tabs[tabIndex].match && _tabs[tabIndex].match(row.kind)) {
			++n;
		}
	}
	return n;
}

int DownloadsContent::countTotal() const {
	int n = 0;
	for (const auto &row : _rows) {
		if (!row.fileName.isEmpty()) {
			++n;
		}
	}
	return n;
}

QString DownloadsContent::activeCountLabel() const {
	const auto count = Sample::ActiveCount(_rows);
	return tr::lng_downloads_files_downloading(tr::now, lt_count, count);
}

QString DownloadsContent::subtitleLabel() const {
	return tr::lng_downloads_count(tr::now, lt_count, countTotal())
		+ u" · "_q
		+ tr::lng_downloads_active(tr::now, lt_count, Sample::ActiveCount(_rows));
}

void DownloadsContent::updateSubtitle() {
	_subtitle->setText(subtitleLabel());
	_footerCount->setText(activeCountLabel());
	for (size_t i = 0; i < _tabButtons.size(); ++i) {
		const auto label = _tabs[i].label;
		const auto count = countFor(int(i));
		_tabButtons[i]->setText(
			u"%1  %2"_q.arg(label).arg(count));
	}
}

void DownloadsContent::cycleSortKey() {
	switch (_sortKey) {
	case SortKey::Date: _sortKey = SortKey::Size; break;
	case SortKey::Size: _sortKey = SortKey::Name; break;
	case SortKey::Name: _sortKey = SortKey::Date; break;
	}
	switch (_sortKey) {
	case SortKey::Date: _sortButton->setText(tr::lng_downloads_sort_date(tr::now)); break;
	case SortKey::Size: _sortButton->setText(tr::lng_downloads_sort_size(tr::now)); break;
	case SortKey::Name: _sortButton->setText(tr::lng_downloads_sort_name(tr::now)); break;
	}
	rebuildVisible();
}

void DownloadsContent::pauseAll() {
	for (auto &row : _rows) {
		if (row.state == Sample::State::Downloading) {
			row.state = Sample::State::Paused;
		}
	}
	rebuildVisible();
}

void DownloadsContent::cancelAll() {
	for (auto &row : _rows) {
		if (row.state == Sample::State::Downloading) {
			row.state = Sample::State::Completed;
			row.percent = 100;
		}
	}
	rebuildVisible();
}

void DownloadsContent::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	p.fillRect(rect(), DownloadsStyle::bg());
}

void DownloadsContent::resizeEvent(QResizeEvent *e) {
	const auto w = width();
	const auto pad = st::downloadsHeaderPadding;
	const auto headerY = 0;

	_title->move(pad, headerY + (st::downloadsHeaderHeight - _title->height()) / 2);
	_subtitle->move(
		pad,
		_title->y() + _title->height() - 4);

	_sortButton->adjustSize();
	const auto sortWidth = _sortButton->sizeHint().width() + 24;
	_sortButton->setFixedWidth(sortWidth);
	_sortButton->move(
		w - pad - st::downloadsViewToggleWidth - 12 - sortWidth,
		headerY + (st::downloadsHeaderHeight - st::downloadsSortButtonHeight) / 2);

	_viewToggleA->move(
		w - pad - st::downloadsViewToggleWidth,
		headerY + (st::downloadsHeaderHeight - st::downloadsViewToggleHeight) / 2);
	_viewToggleB->move(
		_viewToggleA->x() + _viewToggleA->width(),
		_viewToggleA->y());

	_searchEdit->move(
		_sortButton->x() - 12 - st::downloadsSearchWidth,
		headerY + (st::downloadsHeaderHeight - st::downloadsSearchHeight) / 2);

	const auto tabY = headerY + st::downloadsHeaderHeight;
	const auto tabPad = st::downloadsTabBarPadding;
	auto tabX = tabPad;
	for (size_t i = 0; i < _tabButtons.size(); ++i) {
		_tabButtons[i]->adjustSize();
		const auto btnWidth = _tabButtons[i]->sizeHint().width() + 12;
		_tabButtons[i]->setFixedWidth(btnWidth);
		_tabButtons[i]->setFixedHeight(st::downloadsTabBarHeight);
		_tabButtons[i]->move(tabX, tabY);
		tabX += btnWidth + st::downloadsTabGap;
	}

	const auto listY = tabY + st::downloadsTabBarHeight + st::downloadsColumnHeaderHeight;
	const auto listH = height() - listY - st::downloadsFooterHeight;
	_scroll->setGeometry(0, listY, w, listH);

	if (_colName) {
		const auto colHeaderY = tabY + st::downloadsTabBarHeight;
		const auto padL = st::downloadsRowHorizontalPadding;
		const auto nameX = padL + st::downloadsThumbnailSize + st::downloadsTabGap;
		const auto actionsX = w - padL - st::downloadsActionsColumnWidth;
		const auto sizeX = actionsX - st::downloadsSizeColumnWidth;
		const auto dateX = actionsX - st::downloadsSizeColumnWidth - st::downloadsDateColumnWidth;
		const auto progressW = std::max(0, sizeX - nameX - 100);
		_colName->move(nameX, colHeaderY);
		_colProgress->move(nameX + 80, colHeaderY);
		_colSize->move(sizeX, colHeaderY);
		_colDate->move(dateX, colHeaderY);
		_colActions->move(actionsX, colHeaderY);
		_colSize->resizeToWidth(st::downloadsSizeColumnWidth);
		_colDate->resizeToWidth(st::downloadsDateColumnWidth);
		_colActions->resizeToWidth(st::downloadsActionsColumnWidth);
		_colProgress->resizeToWidth(progressW);
	}

	const auto footerY = height() - st::downloadsFooterHeight;
	_footerDot->move(pad, footerY + (st::downloadsFooterHeight - st::downloadsFooterDotSize) / 2);
	_footerCount->move(
		_footerDot->x() + st::downloadsFooterDotSize + 8,
		footerY);

	_cancelAll->setFixedHeight(st::downloadsFooterHeight - 8);
	_pauseAll->setFixedHeight(st::downloadsFooterHeight - 8);
	const auto cancelText = tr::lng_downloads_action_cancel_all(tr::now);
	const auto pauseText = tr::lng_downloads_action_pause_all(tr::now);
	const auto fm = QFontMetrics(_cancelAll->font());
	const auto cancelWidth = fm.horizontalAdvance(cancelText) + 24;
	const auto pauseWidth = fm.horizontalAdvance(pauseText) + 24;
	_cancelAll->setFixedWidth(cancelWidth);
	_pauseAll->setFixedWidth(pauseWidth);
	_cancelAll->move(w - pad - cancelWidth, footerY + 4);
	_pauseAll->move(
		_cancelAll->x() - pauseWidth - 8,
		footerY + 4);

	for (size_t i = 0; i < _rowWidgets.size(); ++i) {
		const auto y = int(i)
			* (st::downloadsRowHeight + st::downloadsRowOuterPadding * 2);
		_rowWidgets[i]->setGeometry(0, y, w, st::downloadsRowHeight + st::downloadsRowOuterPadding * 2);
	}
	const auto totalListHeight = int(_rowWidgets.size())
		* (st::downloadsRowHeight + st::downloadsRowOuterPadding * 2);
	if (_listHost) {
		_listHost->setGeometry(0, 0, w, std::max(totalListHeight, 1));
	}
	if (_visible.empty()) {
		_emptyTitle->move(
			(_listHost->width() - _emptyTitle->width()) / 2,
			48);
		_emptySubtitle->move(
			(_listHost->width() - _emptySubtitle->width()) / 2,
			_emptyTitle->y() + _emptyTitle->height() + 8);
	}
	_listHost->setMinimumHeight(
		int(_rowWidgets.size()) * (st::downloadsRowHeight + st::downloadsRowOuterPadding * 2)
		+ st::downloadsHeaderHeight);
}

} // namespace Ui