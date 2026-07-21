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
}

void DownloadsContent::setupTabs() {
	for (size_t i = 0; i < _tabs.size(); ++i) {
		auto button = Ui::CreateChild<QPushButton>(this);
		button->setCheckable(true);
		button->setAutoExclusive(true);
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
		case 0: return tr::lng_downloads_tab_all(tr::now);
		case 1: return tr::lng_downloads_tab_photos(tr::now);
		case 2: return tr::lng_downloads_tab_videos(tr::now);
		case 3: return tr::lng_downloads_tab_files(tr::now);
		case 4: return tr::lng_downloads_tab_music(tr::now);
		case 5: return tr::lng_downloads_tab_links(tr::now);
		case 6: return tr::lng_downloads_tab_voice(tr::now);
		}
		return QString();
	};
	for (size_t i = 0; i < _tabs.size(); ++i) {
		_tabs[i].label = labelFor(int(i));
		_tabButtons[i]->setText(_tabs[i].label);
	}
}

void DownloadsContent::setupFooter() {
	_footerDot = Ui::CreateChild<QLabel>(this);
	_footerDot->setFixedSize(st::downloadsFooterDotSize, st::downloadsFooterDotSize);

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
	_listHost = Ui::CreateChild<QWidget>(_scroll->widget());
	_scroll->widget()->layout()->addWidget(_listHost);

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
		const auto btnWidth = _tabButtons[i]->sizeHint().width() + 16;
		_tabButtons[i]->setFixedWidth(btnWidth);
		_tabButtons[i]->setFixedHeight(st::downloadsTabBarHeight);
		_tabButtons[i]->move(tabX, tabY);
		tabX += btnWidth + st::downloadsTabGap;
	}

	const auto listY = tabY + st::downloadsTabBarHeight;
	const auto listH = height() - listY - st::downloadsFooterHeight;
	_scroll->setGeometry(0, listY, w, listH);

	const auto footerY = height() - st::downloadsFooterHeight;
	_footerDot->move(pad, footerY + (st::downloadsFooterHeight - st::downloadsFooterDotSize) / 2);
	_footerCount->move(
		_footerDot->x() + st::downloadsFooterDotSize + 8,
		footerY);

	_cancelAll->adjustSize();
	_pauseAll->adjustSize();
	_cancelAll->move(w - pad - _cancelAll->sizeHint().width(), footerY + 4);
	_pauseAll->move(
		_cancelAll->x() - _pauseAll->sizeHint().width() - 16,
		footerY + 4);

	for (size_t i = 0; i < _rowWidgets.size(); ++i) {
		const auto y = int(i)
			* (st::downloadsRowHeight + st::downloadsRowOuterPadding * 2);
		_rowWidgets[i]->setGeometry(0, y, w, st::downloadsRowHeight + st::downloadsRowOuterPadding * 2);
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