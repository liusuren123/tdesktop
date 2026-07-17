/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/sections/settings_download_center.h"
#include "settings/settings_common_session.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "boxes/download_center_clear_box.h"
#include "data/data_download_center.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/ui_utility.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/painter.h"
#include "ui/vertical_list.h"
#include "ui/ui_utility.h"
#include "ui/layers/generic_box.h"
#include "lang/lang_keys.h"
#include "window/window_session_controller.h"
#include "styles/style_settings.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "base/debug_log.h"
#include <QtCore/QFileInfo>
#include <QtGui/QAction>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QMenu>
namespace Settings {
namespace {
using namespace Builder;
class DownloadCenter;
QString FormatBytes(int64 bytes) {
	if (bytes <= 0) {
		return u"0 B"_q;
	}
	const char *units[] = { "B", "KB", "MB", "GB", "TB" };
	auto value = double(bytes);
	auto unit = 0;
	while (value >= 1024.0 && unit < 4) {
		value /= 1024.0;
		++unit;
	}
	return QString::number(value, 'f', value >= 100 ? 0 : 1)
		+ ' '
		+ QString::fromLatin1(units[unit]);
}
QString StateLabel(Data::DownloadState state) {
	switch (state) {
	case Data::DownloadState::Queued: return QStringLiteral("Queued");
	case Data::DownloadState::Downloading: return QStringLiteral("Downloading");
	case Data::DownloadState::Paused: return QStringLiteral("Paused");
	case Data::DownloadState::Completed: return QStringLiteral("Completed");
	case Data::DownloadState::Failed: return QStringLiteral("Failed");
	case Data::DownloadState::Cancelled: return QStringLiteral("Cancelled");
	}
	return QStringLiteral("Unknown");
}
class TaskRow : public Ui::RippleButton {
public:
	TaskRow(
			QWidget *parent,
			not_null<DownloadCenter*> controller,
			Data::DownloadTaskId taskId);

	[[nodiscard]] Data::DownloadTaskId taskId() const {
		return _taskId;
	}

	void refreshFromTask(const Data::DownloadTask &task);

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	void rebuildMenu();

	not_null<DownloadCenter*> _controller;
	Data::DownloadTaskId _taskId = 0;
	object_ptr<QToolButton> _menuButton = { nullptr };
	object_ptr<QMenu> _menu = { nullptr };
	QString _fileName;
	QString _infoLine;
	float64 _progress = 0.;
	int64 _readySize = 0;
	int64 _totalSize = 0;
	Data::DownloadState _state = Data::DownloadState::Queued;
};
class DownloadCenter : public Section<DownloadCenter> {
public:
	DownloadCenter(
		QWidget *parent,
		not_null<Window::SessionController*> controller);
	friend class TaskRow;
	[[nodiscard]] rpl::producer<QString> title() override;
	void showFinished() override;
private:
	void setupContent();
	void rebuildTaskList();
	void onTaskAdded(Data::DownloadTaskId id);
	void onTaskUpdated(Data::DownloadTaskId id);
	void onTaskRemoved(Data::DownloadTaskId id);
	void onTasksReloaded();
	void handlePause(Data::DownloadTaskId id);
	void handleResume(Data::DownloadTaskId id);
	void handleCancel(Data::DownloadTaskId id);
	void handleRetry(Data::DownloadTaskId id);
	void handleRemove(Data::DownloadTaskId id);
	void handleOpen(Data::DownloadTaskId id);
	void handleShowInFolder(Data::DownloadTaskId id);
	QPointer<Ui::VerticalLayout> _listContainer;
		base::flat_map<Data::DownloadTaskId, QPointer<TaskRow>> _rows;
		QPointer<Ui::FlatLabel> _emptyLabel;
};
TaskRow::TaskRow(
	QWidget *parent,
	not_null<DownloadCenter*> controller,
	Data::DownloadTaskId taskId)
: RippleButton(parent, st::defaultRippleAnimation)
, _controller(controller)
, _taskId(taskId)
, _menuButton(this)
, _menu(this) {
	LOG(("DLC: TaskRow ctor id=%1 parent=%2").arg(_taskId).arg((quintptr)parent));
	resize(width(), st::settingsDownloadCenterRowHeight);
	setAcceptBoth(true);
	_menuButton->setText(u"\u22EE"_q);
	_menuButton->setCursor(Qt::PointingHandCursor);
	_menuButton->setStyleSheet(QString(
		"QToolButton{border:none;background:transparent;font-size:18px;color:%1;padding:0;}"
		"QToolButton:hover{color:%2;}"
		"QToolButton::menu-indicator{image:none;}"
	).arg(st::settingsDownloadCenterRowInfoFg->c.name())
		.arg(st::settingsDownloadCenterRowFg->c.name()));
	_menuButton->setFixedSize(28, st::settingsDownloadCenterRowHeight);
	_menuButton->setPopupMode(QToolButton::InstantPopup);
	_menuButton->raise();
	_menuButton->setMenu(_menu);
	rebuildMenu();
}
void TaskRow::resizeEvent(QResizeEvent *e) {
	RippleButton::resizeEvent(e);
	if (_menuButton) {
		const auto btnSize = _menuButton->size();
		_menuButton->move(width() - btnSize.width(),
			(height() - btnSize.height()) / 2);
		_menuButton->raise();
	}
}
void TaskRow::rebuildMenu() {
	if (!_menu) return;
	_menu->clear();
	auto &downloadCenter = _controller->controller()->session().downloadCenter();
	const auto *task = downloadCenter.task(_taskId);
	if (!task) return;
	const auto id = _taskId;
	const auto state = task->state;
	if (state == Data::DownloadState::Completed) {
		_menu->addAction(tr::lng_download_center_action_open(tr::now), [=] {
			_controller->handleOpen(id);
		});
		_menu->addAction(tr::lng_download_center_action_show_in_folder(tr::now), [=] {
			_controller->handleShowInFolder(id);
		});
	}
	if (state == Data::DownloadState::Downloading
		|| state == Data::DownloadState::Queued) {
		_menu->addAction(tr::lng_download_center_action_pause(tr::now), [=] {
			_controller->handlePause(id);
		});
	}
	if (state == Data::DownloadState::Paused
			|| state == Data::DownloadState::Queued
			|| state == Data::DownloadState::Failed) {
			_menu->addAction(tr::lng_download_center_action_resume(tr::now), [=] {
				_controller->handleResume(id);
			});
		}
	if (state == Data::DownloadState::Failed) {
		_menu->addAction(tr::lng_download_center_action_retry(tr::now), [=] {
			_controller->handleRetry(id);
		});
	}
	if (state == Data::DownloadState::Downloading
		|| state == Data::DownloadState::Paused
		|| state == Data::DownloadState::Queued) {
		_menu->addAction(tr::lng_download_center_action_cancel(tr::now), [=] {
			_controller->handleCancel(id);
		});
	}
	_menu->addAction(tr::lng_download_center_action_remove(tr::now), [=] {
		_controller->handleRemove(id);
	});
}
void TaskRow::refreshFromTask(const Data::DownloadTask &task) {
	const auto previousState = _state;
	_fileName = task.fileName.isEmpty()
		? QString::number(task.id)
		: task.fileName;
	_readySize = task.readySize;
	_totalSize = task.totalSize;
	_state = task.state;
	_progress = (_totalSize > 0)
		? std::clamp(float64(_readySize) / float64(_totalSize), 0., 1.)
		: (task.state == Data::DownloadState::Completed ? 1. : 0.);
	QString sizeLine = FormatBytes(_totalSize);
	if (_state == Data::DownloadState::Downloading
		|| _state == Data::DownloadState::Paused) {
		sizeLine = u"%1 / %2"_q.arg(FormatBytes(_readySize)).arg(sizeLine);
	} else if (_state == Data::DownloadState::Completed) {
		sizeLine = FormatBytes(_readySize);
	}
	_infoLine = StateLabel(_state) + u" — " + sizeLine;
	if (previousState != _state) {
		rebuildMenu();
	}
	update();
	}
void TaskRow::paintEvent(QPaintEvent *e) {
	RippleButton::paintEvent(e);
	auto p = QPainter(this);
	p.setRenderHint(QPainter::Antialiasing);
	const auto padding = st::settingsDownloadCenterRowPadding;
	const auto inner = rect().marginsRemoved(padding);
	const auto leftColWidth = 64;
	const auto textLeft = inner.left() + leftColWidth;
	const auto menuBtnWidth = _menuButton ? _menuButton->width() : 0;
	const auto textRight = inner.right() - menuBtnWidth;
	p.setPen(st::settingsDownloadCenterRowFg);
		p.setFont(st::semiboldTextStyle.font);
		const auto nameMetrics = p.fontMetrics();
		const auto nameHeight = nameMetrics.height();
		p.drawText(
			QRect(textLeft, inner.top(), textRight - textLeft, nameHeight),
			Qt::AlignLeft | Qt::TextSingleLine,
			_fileName);
		p.setPen(st::settingsDownloadCenterRowInfoFg);
		p.setFont(st::normalFont);
	const auto infoY = inner.top() + nameHeight + 2;
	p.drawText(
		QRect(textLeft, infoY, textRight - textLeft, nameMetrics.height()),
		Qt::AlignLeft | Qt::TextSingleLine,
		_infoLine);
	if (_totalSize > 0
		&& (_state == Data::DownloadState::Downloading
			|| _state == Data::DownloadState::Paused)) {
		const auto barY = infoY + nameMetrics.height() + 6;
		const auto barRect = QRect(textLeft, barY, textRight - textLeft, 4);
		p.setPen(Qt::NoPen);
		p.setBrush(st::settingsDownloadCenterRowProgressBg);
		p.drawRoundedRect(barRect, 2, 2);
		p.setBrush(st::settingsDownloadCenterRowProgressFg);
		const auto filledWidth = int(barRect.width() * _progress);
		if (filledWidth > 0) {
			const auto filled = QRectF(
				barRect.left(),
				barRect.top(),
				filledWidth,
				barRect.height());
			p.drawRoundedRect(filled, 2, 2);
		}
	}
}
DownloadCenter::DownloadCenter(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}
rpl::producer<QString> DownloadCenter::title() {
	return tr::lng_download_center_title();
}
void DownloadCenter::showFinished() {
	rebuildTaskList();
}
void DownloadCenter::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	const auto &session = controller()->session();
	auto &downloadCenter = session.downloadCenter();
	Ui::AddSubsectionTitle(
		content,
		tr::lng_download_center_section_active());
	_listContainer = content->add(
			object_ptr<Ui::VerticalLayout>(content));
		const auto emptyLabel = content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					tr::lng_download_center_empty(),
					st::settingsDownloadCenterEmpty));
			_emptyLabel = emptyLabel;
	Ui::AddSkip(content);
	Ui::AddDivider(content);
	Ui::AddSkip(content);
	Ui::AddSubsectionTitle(
		content,
		tr::lng_download_center_section_actions());
	const auto actions = content->add(
		object_ptr<Ui::VerticalLayout>(content));
	const auto resumeAll = actions->add(
			object_ptr<Ui::SettingsButton>(
				actions,
				tr::lng_download_center_resume_all()));
		resumeAll->addClickHandler([=, &downloadCenter] {
			for (const auto *task : downloadCenter.tasks()) {
				if (task->state == Data::DownloadState::Paused) {
					downloadCenter.resume(task->id);
				}
			}
		});

		const auto pauseAll = actions->add(
			object_ptr<Ui::SettingsButton>(
				actions,
				tr::lng_download_center_pause_all()));
		pauseAll->addClickHandler([=, &downloadCenter] {
			for (const auto *task : downloadCenter.tasks()) {
				if (task->state == Data::DownloadState::Downloading
					|| task->state == Data::DownloadState::Queued) {
					downloadCenter.pause(task->id);
				}
			}
		});
	const auto clearCompleted = actions->add(
		object_ptr<Ui::SettingsButton>(
			actions,
			tr::lng_download_center_clear_completed()));
	clearCompleted->addClickHandler([=] {
			controller()->show(
				Box(Settings::DownloadCenterClearBox,
					controller(),
					DownloadCenterClearKind::Completed));
		});
		const auto clearFailed = actions->add(
			object_ptr<Ui::SettingsButton>(
				actions,
				tr::lng_download_center_clear_failed()));
		clearFailed->addClickHandler([=] {
			controller()->show(
				Box(Settings::DownloadCenterClearBox,
					controller(),
					DownloadCenterClearKind::Failed));
		});
		const auto clearAll = actions->add(
			object_ptr<Ui::SettingsButton>(
				actions,
				tr::lng_download_center_clear_all()));
		clearAll->addClickHandler([=] {
					controller()->show(
						Box(Settings::DownloadCenterClearBox,
							controller(),
							DownloadCenterClearKind::All));
				});
			Ui::AddSkip(actions);
			Ui::AddDivider(actions);
			Ui::AddSkip(actions);
			const auto autoResume = actions->add(
				object_ptr<Ui::SettingsButton>(
					actions,
					tr::lng_download_center_auto_resume(),
					st::settingsButtonNoIcon
				))->toggleOn(
					Core::App().settings().downloadAutoResumeValue()
				);
			autoResume->toggledChanges(
			) | rpl::on_next([](bool enabled) {
				Core::App().settings().setDownloadAutoResume(enabled);
			}, lifetime());
			Ui::ResizeFitChild(this, content);
	downloadCenter.taskAdded(
	) | rpl::on_next([=](Data::DownloadTaskId id) {
		onTaskAdded(id);
	}, lifetime());
	downloadCenter.taskUpdated(
	) | rpl::on_next([=](Data::DownloadTaskId id) {
		onTaskUpdated(id);
	}, lifetime());
	downloadCenter.taskRemoved(
	) | rpl::on_next([=](Data::DownloadTaskId id) {
		onTaskRemoved(id);
	}, lifetime());
	downloadCenter.tasksReloaded(
	) | rpl::on_next([=] {
		onTasksReloaded();
	}, lifetime());
}
void DownloadCenter::rebuildTaskList() {
	if (!_listContainer) {
		return;
	}
	_listContainer->clear();
	_rows.clear();
	const auto &downloadCenter = controller()->session().downloadCenter();
	auto hasAny = false;
	for (const auto *task : downloadCenter.tasks()) {
		hasAny = true;
		const auto row = _listContainer->add(
			object_ptr<TaskRow>(_listContainer, this, task->id));
		row->refreshFromTask(*task);
		_rows.emplace(task->id, row);
	}
	if (_emptyLabel) {
			_emptyLabel->setVisible(!hasAny);
		}
}
void DownloadCenter::onTaskAdded(Data::DownloadTaskId id) {
	rebuildTaskList();
}
void DownloadCenter::onTaskUpdated(Data::DownloadTaskId id) {
	const auto &downloadCenter = controller()->session().downloadCenter();
	const auto *task = downloadCenter.task(id);
	if (!task) {
		rebuildTaskList();
		return;
	}
	if (!_rows.contains(id)) {
		rebuildTaskList();
		return;
	}
	if (const auto row = _rows[id]) {
		row->refreshFromTask(*task);
	}
}
void DownloadCenter::onTaskRemoved(Data::DownloadTaskId id) {
	rebuildTaskList();
}
void DownloadCenter::onTasksReloaded() {
	rebuildTaskList();
}
void DownloadCenter::handlePause(Data::DownloadTaskId id) {
	LOG(("DLC: handlePause id=%1").arg(id));
	controller()->session().downloadCenter().pause(id);
}
void DownloadCenter::handleResume(Data::DownloadTaskId id) {
	LOG(("DLC: handleResume id=%1").arg(id));
	controller()->session().downloadCenter().resume(id);
}
void DownloadCenter::handleCancel(Data::DownloadTaskId id) {
	LOG(("DLC: handleCancel id=%1").arg(id));
	controller()->session().downloadCenter().cancel(id);
}
void DownloadCenter::handleRetry(Data::DownloadTaskId id) {
	LOG(("DLC: handleRetry id=%1").arg(id));
	controller()->session().downloadCenter().retry(id);
}
void DownloadCenter::handleRemove(Data::DownloadTaskId id) {
	LOG(("DLC: handleRemove id=%1").arg(id));
	controller()->session().downloadCenter().remove(id);
}
void DownloadCenter::handleOpen(Data::DownloadTaskId id) {
	controller()->session().downloadCenter().openFile(id);
}
void DownloadCenter::handleShowInFolder(Data::DownloadTaskId id) {
	controller()->session().downloadCenter().showInFolder(id);
}
} // namespace
Type DownloadCenterId() {
	return DownloadCenter::Id();
}
} // namespace Settings
