/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_content.h"

#include "base/timer.h"
#include "data/data_download_center.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "styles/style_chat.h" // popupMenuExpandedSeparator
#include "styles/style_downloads_icons.h"
#include "styles/style_widgets.h"
#include "ui/painter.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/downloads/downloads_footer_bar.h"
#include "ui/widgets/downloads/downloads_detail_panel.h"
#include "ui/widgets/downloads/downloads_grid_view.h"
#include "ui/widgets/downloads/downloads_kind.h"
#include "ui/widgets/downloads/downloads_row_delegate.h"
#include "ui/widgets/downloads/downloads_search.h"
#include "ui/widgets/downloads/downloads_style.h"
#include "ui/widgets/downloads/downloads_table_model.h"
#include "ui/widgets/downloads/downloads_tab_button.h"
#include "ui/widgets/downloads/downloads_text_button.h"
#include "ui/widgets/downloads/downloads_view_toggle_group.h"
#include "styles/style_media_player.h" // mediaPlayerMenuCheck
#include "ui/image/image.h"
#include "ui/text/format_values.h" // FormatSizeText
#include "window/window_session_controller.h"

#include <QtCore/QDateTime>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QTableView>

#include <algorithm>

namespace Ui {

namespace {
// Coalesce backend updates (progress ticks) into one refresh per window.
constexpr auto kRefreshTimeoutMs = 100;
} // namespace

bool DownloadsContent::MatchPhoto(Sample::Kind k)   { return k == Sample::Kind::Photo; }
bool DownloadsContent::MatchVideo(Sample::Kind k)   { return k == Sample::Kind::Video; }
bool DownloadsContent::MatchFile(Sample::Kind k)    {
	return k == Sample::Kind::Document || k == Sample::Kind::Archive;
}
bool DownloadsContent::MatchMusic(Sample::Kind k)   { return k == Sample::Kind::Audio; }
bool DownloadsContent::MatchLink(Sample::Kind k)    { return k == Sample::Kind::Link; }
bool DownloadsContent::MatchVoice(Sample::Kind k)   { return k == Sample::Kind::Voice; }

DownloadsContent::DownloadsContent(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
: QWidget(parent)
, _session(&controller->session())
, _dc(&_session->downloadCenter()) {
	setAttribute(Qt::WA_OpaquePaintEvent);

	_tabs = {
		{ QString(), nullptr },
		{ QString(), &MatchPhoto },
		{ QString(), &MatchVideo },
		{ QString(), &MatchFile },
		{ QString(), &MatchMusic },
		{ QString(), &MatchLink },
		{ QString(), &MatchVoice },
	};

	setupUi();

	// Coalesce high-frequency backend updates (e.g. progress ticks) into a
	// single ~100 ms refresh so repaints stay bounded.
	_refreshTimer = std::make_unique<base::Timer>([=] { refreshFromBackend(); });
	const auto arm = [=] { scheduleRefresh(); };
	_dc->taskAdded() | rpl::on_next(arm, _lifetime);
	_dc->taskUpdated() | rpl::on_next(arm, _lifetime);
	_dc->taskRemoved() | rpl::on_next(arm, _lifetime);
	_dc->tasksReloaded() | rpl::on_next(arm, _lifetime);
	// Preview thumbnails load asynchronously; refresh when any download
	// (incl. thumbnail fetches) completes.
	_session->downloaderTaskFinished()
		| rpl::on_next(arm, _lifetime);

	refreshFromBackend();
}

DownloadsContent::~DownloadsContent() = default;

void DownloadsContent::setupUi() {
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	setupHeader(root);
	setupTabs(root);

	// Content row holds the list table, the grid scroll area, and the
	// detail panel side by side. Only one of table / grid scroll is visible
	// at a time (driven by the view toggle); the detail panel is independent
	// so item selection works in either mode.
	_contentRow = new QWidget(this);
	auto *content = new QHBoxLayout(_contentRow);
	content->setContentsMargins(0, 0, 0, 0);
	content->setSpacing(0);

	setupTable(content);
	setupGrid(content);
	root->addWidget(_contentRow, 1);

	setupFooter(root);

	// Default view mode is list: hide the grid and the panel until used.
	if (_gridScroll) _gridScroll->setVisible(false);
	if (_detailPanel) _detailPanel->setVisible(false);
}

void DownloadsContent::setupHeader(QVBoxLayout *root) {
	auto *wrap = new QWidget(this);
	wrap->setFixedHeight(int(st::downloadsHeaderHeight));
	auto *outer = new QHBoxLayout(wrap);
	outer->setContentsMargins(
		int(st::downloadsHeaderPadding), 0,
		int(st::downloadsHeaderPadding), 0);
	outer->setSpacing(0);

	// Title block (title + subtitle stacked vertically).
	auto *titleBlock = new QWidget(wrap);
	auto *titleLayout = new QVBoxLayout(titleBlock);
	titleLayout->setContentsMargins(0, 0, 0, 0);
	titleLayout->setSpacing(2);

	_title = Ui::CreateChild<FlatLabel>(titleBlock, st::downloadsHeaderTitle);
	_title->setText(tr::lng_downloads_title(tr::now));
	titleLayout->addWidget(_title);

	_subtitle = Ui::CreateChild<FlatLabel>(titleBlock, st::downloadsHeaderSubtitle);
	titleLayout->addWidget(_subtitle);
	outer->addWidget(titleBlock);

	outer->addStretch(1);

	_searchEdit = Ui::CreateChild<DownloadsSearch>(wrap);
	connect(_searchEdit, &DownloadsSearch::textChanged, this, [this](const QString &text) {
		_search = text;
		rebuildModel();
	});
	outer->addWidget(_searchEdit);

	outer->addSpacing(12);

	_sortButton = Ui::CreateChild<DownloadsTextButton>(
		wrap,
		&st::downloadsSort,
		u"Sort: "_q + sortLabel(SortKey::Date),
		true);
	_sortButton->setClickedCallback([this] { showSortMenu(); });
	outer->addWidget(_sortButton);

	outer->addSpacing(12);

	_viewToggle = Ui::CreateChild<DownloadsViewToggleGroup>(wrap);
	connect(_viewToggle, &DownloadsViewToggleGroup::activeChanged,
		this, [this](int index) { setViewMode(index); });
	outer->addWidget(_viewToggle);

	root->addWidget(wrap);
}

void DownloadsContent::setupTabs(QVBoxLayout *root) {
	auto *wrap = new QWidget(this);
	wrap->setFixedHeight(int(st::downloadsTabBarHeight));
	auto *outer = new QHBoxLayout(wrap);
	outer->setContentsMargins(
		int(st::downloadsTabBarPadding), 0,
		int(st::downloadsTabBarPadding), 0);
	outer->setSpacing(int(st::downloadsTabGap));

	struct Entry { QString label; const style::icon *icon; };
	const Entry entries[] = {
		{ tr::lng_downloads_tab_all(tr::now),    &st::downloadsTabAll },
		{ tr::lng_downloads_tab_photos(tr::now), &st::downloadsTabPhotos },
		{ tr::lng_downloads_tab_videos(tr::now), &st::downloadsTabVideos },
		{ tr::lng_downloads_tab_files(tr::now),  &st::downloadsTabFiles },
		{ tr::lng_downloads_tab_music(tr::now),  &st::downloadsTabMusic },
		{ tr::lng_downloads_tab_links(tr::now),  &st::downloadsTabLinks },
		{ tr::lng_downloads_tab_voice(tr::now),  &st::downloadsTabVoice },
	};

	for (size_t i = 0; i < _tabs.size(); ++i) {
		auto *button = Ui::CreateChild<DownloadsTabButton>(
			wrap, *entries[i].icon, entries[i].label);
		const auto index = int(i);
		button->setClickedCallback([this, button, index] {
			_activeTab = index;
			for (auto *b : _tabButtons) b->setActive(false);
			button->setActive(true);
			rebuildModel();
		});
		_tabs[i].label = entries[i].label;
		_tabButtons.push_back(button);
		outer->addWidget(button);
	}
	_tabButtons[0]->setActive(true);
	outer->addStretch(1);

	root->addWidget(wrap);
}

void DownloadsContent::setupTable(QHBoxLayout *content) {
	_model = new DownloadsTableModel(this);
	_delegate = new DownloadsRowDelegate(this);

	_table = new QTableView(this);
	_table->setModel(_model);
	_table->setItemDelegate(_delegate);

	// Visual configuration.
	_table->setShowGrid(false);
	_table->setSelectionMode(QAbstractItemView::NoSelection);
	_table->setFocusPolicy(Qt::NoFocus);
	_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

	// Transparent viewport — the QTableView's default viewport background
	// (a light/dark tone depending on theme) shows through whenever the
	// rows don't fill the viewport height. Making the viewport transparent
	// lets the outer DownloadsContent background show through everywhere,
	// so the empty area below the rows matches the row bg.
	_table->setStyleSheet(QString(
		"QTableView { background: transparent; }"
		"QTableView::item { background: transparent; }"
	));
	_table->viewport()->setAutoFillBackground(false);
	// Track hover for the animated row wash + keep the empty overlay centred.
	_table->viewport()->setMouseTracking(true);
	_table->viewport()->installEventFilter(this);

	_table->verticalHeader()->setVisible(false);
	_table->verticalHeader()->setDefaultSectionSize(
		int(st::downloadsRowHeight + st::downloadsRowOuterPadding * 2));
	_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

	_header = _table->horizontalHeader();
	_header->setHighlightSections(false);
	_header->setStretchLastSection(false);
	_header->setSectionsClickable(false);
	_header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	_header->setFixedHeight(int(st::downloadsColumnHeaderHeight));

	// Scrollbar styling — slim, semi-transparent handle on transparent track,
	// matching Telegram's dark theme.
	_table->verticalScrollBar()->setStyleSheet(QString(
		"QScrollBar:vertical {"
		" background: transparent;"
		" width: 6px;"
		" margin: 0;"
		"}"
		"QScrollBar::handle:vertical {"
		" background: rgba(255, 255, 255, 60);"
		" border-radius: 3px;"
		" min-height: 40px;"
		"}"
		"QScrollBar::handle:vertical:hover {"
		" background: rgba(255, 255, 255, 100);"
		"}"
		"QScrollBar::add-line:vertical,"
		"QScrollBar::sub-line:vertical {"
		" background: none;"
		" height: 0;"
		"}"
		"QScrollBar::add-page:vertical,"
		"QScrollBar::sub-page:vertical {"
		" background: none;"
		"}"
	));
	_table->horizontalScrollBar()->setStyleSheet(QString(
		"QScrollBar:horizontal {"
		" background: transparent;"
		" height: 6px;"
		" margin: 0;"
		"}"
		"QScrollBar::handle:horizontal {"
		" background: rgba(255, 255, 255, 60);"
		" border-radius: 3px;"
		" min-width: 40px;"
		"}"
		"QScrollBar::handle:horizontal:hover {"
		" background: rgba(255, 255, 255, 100);"
		"}"
		"QScrollBar::add-line:horizontal,"
		"QScrollBar::sub-line:horizontal {"
		" background: none;"
		" width: 0;"
		"}"
		"QScrollBar::add-page:horizontal,"
		"QScrollBar::sub-page:horizontal {"
		" background: none;"
		"}"
	));

	// Flat, borderless header — no per-section background, no separators,
	// matching the prototype where column titles sit on a single flat strip.
	//
	// The Name column needs extra left padding so its title sits flush with
	// the thumbnail area of the rows below (horizontalPad + thumbnailSize +
	// gap). All other columns use the default horizontalPad.
	const auto nameLeftPad = int(st::downloadsRowHorizontalPadding)
		+ int(st::downloadsThumbnailSize)
		+ int(st::downloadsTabGap);
	const auto defaultLeftPad = int(st::downloadsRowHorizontalPadding);
	_header->setStyleSheet(QString(
		"QHeaderView { background: transparent; }"
		"QHeaderView::section {"
		" background: transparent;"
		" border: none;"
		" padding-left: %1px;"
		" padding-right: %1px;"
		" color: rgba(255, 255, 255, 140);"
		" font-size: 11px;"
		"}"
		"QHeaderView::section:first {"
		" padding-left: %2px;"
		"}"
	).arg(defaultLeftPad).arg(nameLeftPad));

	// Fixed column widths matching the prototype layout.
	// The Progress column reserves an extra `downloadsProgressGap` of empty
	// space on its right side so the badge sits flush with the bar but
	// leaves a visible gap before the Size column.
	const auto progressW = int(st::downloadsProgressColumnWidth)
		+ int(st::downloadsProgressGap);
	const auto sizeW = int(st::downloadsSizeColumnWidth);
	const auto dateW = int(st::downloadsDateColumnWidth);
	const auto actionsW = int(st::downloadsActionsColumnWidth);

	// Name column absorbs all remaining space.
	_table->setColumnWidth(DownloadsTableModel::ProgressColumn, progressW);
	_table->setColumnWidth(DownloadsTableModel::SizeColumn, sizeW);
	_table->setColumnWidth(DownloadsTableModel::DateColumn, dateW);
	_table->setColumnWidth(DownloadsTableModel::ActionsColumn, actionsW);
	_header->setSectionResizeMode(
		DownloadsTableModel::NameColumn, QHeaderView::Stretch);
	_header->setSectionResizeMode(
		DownloadsTableModel::ProgressColumn, QHeaderView::Fixed);
	_header->setSectionResizeMode(
		DownloadsTableModel::SizeColumn, QHeaderView::Fixed);
	_header->setSectionResizeMode(
		DownloadsTableModel::DateColumn, QHeaderView::Fixed);
	_header->setSectionResizeMode(
		DownloadsTableModel::ActionsColumn, QHeaderView::Fixed);

	// Header labels.
	_model->setHeaderLabels({
		tr::lng_downloads_col_name(tr::now),
		tr::lng_downloads_col_progress(tr::now),
		tr::lng_downloads_col_size(tr::now),
		tr::lng_downloads_col_date(tr::now),
		tr::lng_downloads_col_actions(tr::now),
	});

	// Empty-state overlay.
	_emptyOverlay = new QWidget(_table);
	_emptyOverlay->setVisible(false);
	auto *emptyLayout = new QVBoxLayout(_emptyOverlay);
	emptyLayout->setAlignment(Qt::AlignCenter);
	emptyLayout->setSpacing(8);

	auto *emptyTitle = Ui::CreateChild<FlatLabel>(
		_emptyOverlay, st::downloadsEmptyTitle);
	emptyTitle->setText(tr::lng_downloads_empty_no_tasks(tr::now));
	emptyLayout->addWidget(emptyTitle, 0, Qt::AlignCenter);

	auto *emptySubtitle = Ui::CreateChild<FlatLabel>(
		_emptyOverlay, st::downloadsEmptySubtitle);
	emptySubtitle->setText(tr::lng_downloads_empty_no_tasks_desc(tr::now));
	emptyLayout->addWidget(emptySubtitle, 0, Qt::AlignCenter);

	// Delegate signal wiring.
	connect(_delegate, &DownloadsRowDelegate::rowClicked,
		this, [this](int row) { selectItem(row); });
	connect(_delegate, &DownloadsRowDelegate::saveClicked,
		this, [this](int row) { onRowAction(row, DownloadsRowDelegate::SaveAction); });
	connect(_delegate, &DownloadsRowDelegate::folderClicked,
		this, [this](int row) { onRowAction(row, DownloadsRowDelegate::FolderAction); });
	connect(_delegate, &DownloadsRowDelegate::removeClicked,
		this, [this](int row) { onRowAction(row, DownloadsRowDelegate::RemoveAction); });
	connect(_delegate, &DownloadsRowDelegate::rowContextMenu,
		this, [this](int row, const QPoint &globalPos) {
		auto *model = _model;
		if (!model || row < 0 || row >= int(model->rows().size())) return;
		const auto id = model->rows()[row].taskId;
		const auto state = model->rows()[row].state;

		const auto guard = Ui::CreateChild<QWidget>(this);
		guard->setAttribute(Qt::WA_TransparentForMouseEvents);
		guard->show();

		auto menu = std::make_unique<Ui::PopupMenu>(
			this, st::popupMenuExpandedSeparator);

		switch (state) {
		case Sample::State::Downloading:
			menu->addAction(tr::lng_downloads_menu_pause(tr::now),
				[this, id] { _dc->pause(id); });
			break;
		case Sample::State::Paused:
			menu->addAction(tr::lng_downloads_menu_resume(tr::now),
				[this, id] { _dc->resume(id); });
			break;
		case Sample::State::Failed:
			menu->addAction(tr::lng_downloads_menu_retry(tr::now),
				[this, id] { _dc->retry(id); });
			break;
		case Sample::State::Completed:
			break;
		}
		menu->addAction(tr::lng_downloads_menu_open(tr::now),
			[this, id] { _dc->openFile(id); });
		menu->addAction(tr::lng_downloads_menu_show_in_folder(tr::now),
			[this, id] { _dc->showInFolder(id); });
		menu->addAction(tr::lng_downloads_menu_copy_link(tr::now), [] {});
		menu->addAction(tr::lng_downloads_menu_remove(tr::now),
			[this, id] { _dc->removeToTrash(id); });

		menu->popup(globalPos);
	});

	content->addWidget(_table, 1);
}

void DownloadsContent::setupGrid(QHBoxLayout *content) {
	// Card grid view, hosted in its own scroll area styled to match the
	// table's scrollbar.
	_gridScroll = new QScrollArea(this);
	_gridScroll->setWidgetResizable(true);
	_gridScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	_gridScroll->setFrameShape(QFrame::NoFrame);
	_gridScroll->setStyleSheet(QString(
		"QScrollArea { background: transparent; }"
		"QScrollBar:vertical {"
		" background: transparent;"
		" width: 6px;"
		" margin: 0;"
		"}"
		"QScrollBar::handle:vertical {"
		" background: rgba(255, 255, 255, 60);"
		" border-radius: 3px;"
		" min-height: 40px;"
		"}"
		"QScrollBar::handle:vertical:hover {"
		" background: rgba(255, 255, 255, 100);"
		"}"
		"QScrollBar::add-line:vertical,"
		"QScrollBar::sub-line:vertical {"
		" background: none;"
		" height: 0;"
		"}"
		"QScrollBar::add-page:vertical,"
		"QScrollBar::sub-page:vertical {"
		" background: none;"
		"}"
	));
	_gridScroll->viewport()->setAutoFillBackground(true);
	// Paint the viewport with the page background so the area not covered by
	// cards (e.g. below the last row) matches the rest of the page instead of
	// the QScrollArea's default black.
	auto viewportPal = _gridScroll->viewport()->palette();
	viewportPal.setColor(QPalette::Window, DownloadsStyle::bg());
	_gridScroll->viewport()->setPalette(viewportPal);

	_gridView = new DownloadsGridView(_gridScroll->viewport());
	_gridScroll->setWidget(_gridView);
	content->addWidget(_gridScroll, 1);

	// Detail panel (right inspector), hidden until an item is selected.
	// Lives in the shared content row so it works in both list and grid mode.
	_detailPanel = new DownloadsDetailPanel(this);
	_detailPanel->setVisible(false);
	_detailPanel->closeClicks()
		| rpl::on_next([this] { closeDetailPanel(); }, _lifetime);
	_detailPanel->primaryClicks()
		| rpl::on_next([this] {
			if (_selectedId == 0 || !_dc) return;
			const auto *t = _dc->task(_selectedId);
			if (!t) return;
			switch (t->state) {
			case Data::DownloadState::Downloading:
				_dc->pause(_selectedId);
				break;
			case Data::DownloadState::Paused:
				_dc->resume(_selectedId);
				break;
			case Data::DownloadState::Failed:
			case Data::DownloadState::Cancelled:
				_dc->retry(_selectedId);
				break;
			case Data::DownloadState::Completed:
				_dc->openFile(_selectedId);
				break;
			default:
				break;
			}
		}, _lifetime);
	_detailPanel->removeClicks()
		| rpl::on_next([this] {
			if (_selectedId == 0 || !_dc) return;
			_dc->removeToTrash(_selectedId);
			closeDetailPanel();
		}, _lifetime);
	_detailPanel->forwardClicks()
		| rpl::on_next([] {
			// Placeholder: would open the forward/share sheet.
		}, _lifetime);
	content->addWidget(_detailPanel);

	// Selecting a grid card opens the detail panel.
	_gridView->cardClicked()
		| rpl::on_next([this](int index) { selectItem(index); }, _lifetime);
}

void DownloadsContent::setViewMode(int toggleIndex) {
	// toggleIndex matches DownloadsViewToggleGroup: 0 = grid, 1 = list.
	if (_viewMode == toggleIndex) return;
	_viewMode = toggleIndex;
	const auto grid = (toggleIndex == 0);
	if (_table) _table->setVisible(!grid);
	if (_gridScroll) _gridScroll->setVisible(grid);
	// The detail panel and current selection persist across view switches.
}

void DownloadsContent::selectItem(int index) {
	if (!_model
		|| index < 0
		|| index >= int(_model->rows().size())) {
		closeDetailPanel();
		return;
	}
	_selectedRow = index;
	_selectedId = _model->rows()[index].taskId;
	if (_gridView) _gridView->setSelectedIndex(index);
	if (_delegate) _delegate->setSelectedRow(index);
	if (_detailPanel) {
		_detailPanel->setRow(_model->rows()[index]);
		_detailPanel->setVisible(true);
	}
}

void DownloadsContent::closeDetailPanel() {
	_selectedRow = -1;
	_selectedId = 0;
	if (_gridView) _gridView->setSelectedIndex(-1);
	if (_delegate) _delegate->setSelectedRow(-1);
	if (_detailPanel) _detailPanel->setVisible(false);
}

void DownloadsContent::setupFooter(QVBoxLayout *root) {
	// The footer is a single hand-painted widget (no child controls).
	// All content (dot, count text, two action buttons) is drawn in
	// DownloadsFooterBar::paintEvent — bypassing child-widget paint
	// issues that arose with the old FlatLabel/QLabel/TextButton layout.
	_footerBar = Ui::CreateChild<DownloadsFooterBar>(this);
	_footerBar->setFixedHeight(int(st::downloadsFooterHeight));
	_footerBar->pauseAllClicks()
		| rpl::on_next([this] { pauseAll(); }, _lifetime);
	_footerBar->cancelAllClicks()
		| rpl::on_next([this] { cancelAll(); }, _lifetime);
	root->addWidget(_footerBar);
}

void DownloadsContent::scheduleRefresh() {
	if (_refreshTimer) {
		_refreshTimer->callOnce(kRefreshTimeoutMs);
	}
}

Sample::Row DownloadsContent::buildUiTask(const Data::DownloadTask &t) {
	auto r = Sample::Row();
	r.taskId = t.id;
	r.fileName = t.fileName;
	r.sizeText = FormatSizeText(t.totalSize);
	r.percent = (t.totalSize > 0)
		? std::clamp(int(t.readySize * 100 / t.totalSize), 0, 100)
		: 0;
	const auto when = t.completedAt.isValid() ? t.completedAt : t.addedAt;
	r.dateText = when.isValid()
		? when.toString(u"MMM d, HH:mm"_q)
		: QString();
	switch (t.state) {
	case Data::DownloadState::Queued:
	case Data::DownloadState::Waiting:
	case Data::DownloadState::Downloading:
		r.state = Sample::State::Downloading;
		break;
	case Data::DownloadState::Paused:
		r.state = Sample::State::Paused;
		break;
	case Data::DownloadState::Failed:
	case Data::DownloadState::Cancelled:
		r.state = Sample::State::Failed;
		break;
	case Data::DownloadState::Completed:
	default:
		r.state = Sample::State::Completed;
		break;
	}
	// Infer media kind from source + mime type.
	const auto &mime = t.mimeType;
	if (t.source == Data::DownloadSource::Photo) {
		r.kind = Sample::Kind::Photo;
	} else if (mime.startsWith(u"image/"_q)) {
		r.kind = Sample::Kind::Photo;
	} else if (mime.startsWith(u"video/"_q)) {
		r.kind = Sample::Kind::Video;
	} else if (mime.startsWith(u"audio/"_q)) {
		r.kind = mime.startsWith(u"audio/ogg"_q)
			? Sample::Kind::Voice
			: Sample::Kind::Audio;
	} else if (t.source == Data::DownloadSource::Url) {
		r.kind = Sample::Kind::Link;
	} else if (mime.contains(u"zip"_q)
		|| mime.contains(u"compressed"_q)
		|| mime.contains(u"tar"_q)
		|| mime.contains(u"gzip"_q)
		|| mime.contains(u"7z"_q)) {
		r.kind = Sample::Kind::Archive;
	} else {
		r.kind = Sample::Kind::Document;
	}
	r.context = t.errorMessage;
	// Resolve the originating peer for the "From" row + the row caption.
	// session->data() may not have the peer loaded yet; fall back to "—".
	if (t.peerId) {
		if (auto *peer = _session->data().peer(t.peerId).get()) {
			r.chatName = peer->name();
		}
	}
	if (r.chatName.isEmpty()) {
		r.chatName = u"—"_q;
	}
	// Resolve a preview thumbnail for media (photos / video posters).
	if (t.source == Data::DownloadSource::Document && t.documentId) {
		auto *doc = _session->data().document(t.documentId).get();
		auto &media = _documentMedia[t.id];
		if (!media) {
			media = doc->createMediaView();
		}
		if (doc->hasThumbnail()) {
			media->thumbnailWanted(t.origin);
		}
		media->goodThumbnailWanted();
		const auto pick = [](Image *image) -> QImage {
			if (!image || image->isNull()) {
				return QImage();
			}
			auto result = image->original();
			// Cap stored size so per-frame paint scaling stays cheap.
			const auto maxSide = 256;
			if (result.width() > maxSide || result.height() > maxSide) {
				result = result.scaled(
					maxSide, maxSide,
					Qt::KeepAspectRatio,
					Qt::SmoothTransformation);
			}
			return result;
		};
		r.thumb = pick(media->thumbnail());
		if (r.thumb.isNull()) {
			r.thumb = pick(media->goodThumbnail());
		}
	}
	return r;
}

void DownloadsContent::refreshFromBackend() {
	if (!_dc) return;
	std::vector<Sample::Row> rows;
	std::set<uint64> alive;
	for (const auto *task : _dc->tasks()) {
		if (!task) continue;
		// Removed/Cancelled live in the trash; hide from the main list.
		if (task->state == Data::DownloadState::Removed) continue;
		alive.insert(task->id);
		rows.push_back(buildUiTask(*task));
	}
	// Drop media views for tasks that are no longer present.
	for (auto it = _documentMedia.begin(); it != _documentMedia.end();) {
		if (alive.count(it->first)) {
			++it;
		} else {
			it = _documentMedia.erase(it);
		}
	}
	_rows = std::move(rows);
	rebuildModel();
}

void DownloadsContent::rebuildModel() {
	std::vector<Sample::Row> filtered;
	for (const auto &row : _rows) {
		if (row.fileName.isEmpty()) continue;
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
		filtered.push_back(row);
	}

	auto sortKey = [&](const Sample::Row &a, const Sample::Row &b) {
		switch (_sortKey) {
		case SortKey::Date:   return a.dateText < b.dateText;
		case SortKey::Size:   return a.sizeText < b.sizeText;
		case SortKey::Name:   return a.fileName < b.fileName;
		case SortKey::Sender: return a.chatName < b.chatName;
		}
		return false;
	};
	std::stable_sort(filtered.begin(), filtered.end(), sortKey);

	_model->setRows(std::move(filtered));

	// Mirror the filtered/sorted rows into the grid view so both views
	// stay in sync with tabs, search, and sort.
	if (_gridView) {
		_gridView->setRows(_model->rows());
	}
	// Selection is id-based: re-resolve to the (possibly new) row index so it
	// survives inserts/removes/reorders. Drop it if the task is gone.
	if (_selectedId != 0) {
		auto newIndex = -1;
		const auto &rows = _model->rows();
		for (auto i = 0; i < int(rows.size()); ++i) {
			if (rows[i].taskId == _selectedId) {
				newIndex = i;
				break;
			}
		}
		if (newIndex < 0) {
			closeDetailPanel();
		} else if (newIndex != _selectedRow) {
			_selectedRow = newIndex;
			if (_gridView) _gridView->setSelectedIndex(newIndex);
			if (_delegate) _delegate->setSelectedRow(newIndex);
		}
		// Keep the panel's content fresh (progress/state may have changed).
		if (_detailPanel && _detailPanel->isVisible() && newIndex >= 0) {
			_detailPanel->setRow(rows[newIndex]);
		}
	}
	// Row indices shift on filter/sort, so the cached hover is stale.
	if (_delegate) _delegate->setHoveredRow(-1);

	_subtitle->setText(subtitleLabel());
	if (_footerBar) {
		_footerBar->setCountText(activeCountLabel());
	}
	for (size_t i = 0; i < _tabButtons.size(); ++i) {
		_tabButtons[i]->setText(_tabs[i].label);
		_tabButtons[i]->setCount(countFor(int(i)));
	}

	const auto empty = _model->rowCount() == 0;
	_emptyOverlay->setVisible(empty);
}

int DownloadsContent::countFor(int tabIndex) const {
	if (tabIndex <= 0 || tabIndex >= int(_tabs.size())) {
		return countTotal();
	}
	int n = 0;
	for (const auto &row : _rows) {
		if (row.fileName.isEmpty()) continue;
		if (_tabs[tabIndex].match && _tabs[tabIndex].match(row.kind)) {
			++n;
		}
	}
	return n;
}

int DownloadsContent::countTotal() const {
	int n = 0;
	for (const auto &row : _rows) {
		if (!row.fileName.isEmpty()) ++n;
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

void DownloadsContent::cycleSortKey() {
	// Kept for API compat; new flow uses showSortMenu + setSortKey.
}

QString DownloadsContent::sortLabel(SortKey key) const {
	switch (key) {
	case SortKey::Date:   return tr::lng_downloads_sort_date(tr::now);
	case SortKey::Name:   return tr::lng_downloads_sort_name(tr::now);
	case SortKey::Size:   return tr::lng_downloads_sort_size(tr::now);
	case SortKey::Sender: return tr::lng_downloads_sort_sender(tr::now);
	}
	return QString();
}

void DownloadsContent::setSortKey(SortKey key) {
	_sortKey = key;
	if (_sortButton) {
		_sortButton->setText(u"Sort: "_q + sortLabel(key));
	}
	rebuildModel();
}

void DownloadsContent::showSortMenu() {
	if (!_sortButton) return;

	auto menu = std::make_unique<Ui::PopupMenu>(
		this,
		st::popupMenuExpandedSeparator);
	menu->deleteOnHide(false);

	// Pattern matches the prototype dropdown: the currently active option
	// shows a check icon on the left, inactive ones leave the slot empty
	// (nullptr icon) so labels stay aligned across rows. The check icon
	// itself comes from mediaPlayerMenuCheck so it matches Telegram's
	// standard menu glyph.
	const auto addOption = [&](
		const QString &label,
		SortKey key) {
		const auto *icon = (_sortKey == key)
			? &st::mediaPlayerMenuCheck
			: nullptr;
		menu->addAction(label, [this, key] { setSortKey(key); }, icon);
	};
	addOption(tr::lng_downloads_sort_date(tr::now),   SortKey::Date);
	addOption(tr::lng_downloads_sort_name(tr::now),   SortKey::Name);
	addOption(tr::lng_downloads_sort_size(tr::now),   SortKey::Size);
	addOption(tr::lng_downloads_sort_sender(tr::now), SortKey::Sender);

	// Anchor the popup's top-left to the bottom-left of the sort button so
	// the menu visually grows out of the button (matches the Figma prototype),
	// instead of appearing wherever the cursor happens to be.
	const auto anchor = QPoint(
		_sortButton->mapToGlobal(QPoint(0, _sortButton->height())).x(),
		_sortButton->mapToGlobal(QPoint(0, _sortButton->height())).y());
	menu->popup(anchor);

	// Hand ownership to a member so the popup survives this function
	// returning. Stored on `this` so the menu is destroyed when
	// DownloadsContent itself is destroyed; we replace any prior sort menu
	// on each invocation.
	_activeSortMenu = std::move(menu);
}

void DownloadsContent::pauseAll() {
	if (!_dc) return;
	for (const auto *t : _dc->tasks()) {
		if (t && t->state == Data::DownloadState::Downloading) {
			_dc->pause(t->id);
		}
	}
}

void DownloadsContent::cancelAll() {
	if (!_dc) return;
	for (const auto *t : _dc->tasks()) {
		if (t && t->state == Data::DownloadState::Downloading) {
			_dc->cancel(t->id);
		}
	}
}

void DownloadsContent::onRowAction(int row, DownloadsRowDelegate::ActionHit action) {
	if (!_model || !_dc || row < 0 || row >= int(_model->rows().size())) return;
	const auto id = _model->rows()[row].taskId;
	switch (action) {
	case DownloadsRowDelegate::SaveAction:
		_dc->openFile(id);
		break;
	case DownloadsRowDelegate::FolderAction:
		_dc->showInFolder(id);
		break;
	case DownloadsRowDelegate::RemoveAction:
		_dc->removeToTrash(id);
		break;
	case DownloadsRowDelegate::NoAction:
		break;
	}
}

void DownloadsContent::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	p.fillRect(rect(), DownloadsStyle::bg());
}

bool DownloadsContent::eventFilter(QObject *obj, QEvent *e) {
	if (_table && obj == _table->viewport()) {
		const auto type = e->type();
		if (type == QEvent::Resize && _emptyOverlay) {
			// Keep overlay centered on the viewport regardless of resize.
			_emptyOverlay->setGeometry(_table->viewport()->rect());
		} else if (_delegate && type == QEvent::MouseMove) {
			const auto me = static_cast<QMouseEvent*>(e);
			_delegate->setHoveredRow(_table->rowAt(me->pos().y()));
		} else if (_delegate && type == QEvent::Leave) {
			_delegate->setHoveredRow(-1);
		}
	}
	return QWidget::eventFilter(obj, e);
}

} // namespace Ui