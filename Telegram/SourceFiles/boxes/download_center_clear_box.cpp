/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/download_center_clear_box.h"
#include "data/data_download_center.h"
#include "main/main_session.h"
#include "window/window_session_controller.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/text/text_utilities.h"
#include "styles/style_layers.h"
#include "base/debug_log.h"
#include <vector>
namespace Settings {
void DownloadCenterClearBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		DownloadCenterClearKind kind) {
	LOG(("DLC: DownloadCenterClearBox ENTER kind=%1 box=%2 controller=%3")
			.arg(int(kind))
			.arg(reinterpret_cast<quintptr>(static_cast<Ui::GenericBox*>(box)))
			.arg(reinterpret_cast<quintptr>(static_cast<Window::SessionController*>(controller))));
	const auto title = (kind == DownloadCenterClearKind::Completed)
		? tr::lng_download_center_clear_completed_title()
		: (kind == DownloadCenterClearKind::Failed)
		? tr::lng_download_center_clear_failed_title()
		: tr::lng_download_center_clear_all_title();
	box->setTitle(title);
	box->addButton(tr::lng_box_ok(), [=] {
			LOG(("DLC: ClearBox OK handler ENTER kind=%1 box=%2")
				.arg(int(kind))
				.arg(reinterpret_cast<quintptr>(static_cast<Ui::GenericBox*>(box))));
			auto &center = controller->session().downloadCenter();
			const auto want = kind;
			LOG(("DLC: ClearBox ok kind=%1 total=%2").arg(int(want)).arg(center.totalCount()));
		std::vector<Data::DownloadTaskId> toRemove;
		toRemove.reserve(center.totalCount());
		for (const auto *task : center.tasks()) {
			const auto remove = (want == DownloadCenterClearKind::All)
				|| (want == DownloadCenterClearKind::Completed
					&& task->state == Data::DownloadState::Completed)
				|| (want == DownloadCenterClearKind::Failed
					&& task->state == Data::DownloadState::Failed);
			if (remove) {
				LOG(("DLC: ClearBox marking id=%1 state=%2").arg(task->id).arg(int(task->state)));
				toRemove.push_back(task->id);
			}
		}
		LOG(("DLC: ClearBox toRemove.size=%1, starting erase loop").arg(toRemove.size()));
		for (const auto id : toRemove) {
			LOG(("DLC: ClearBox calling remove(%1)").arg(id));
			center.remove(id);
		}
		LOG(("DLC: ClearBox done, calling box->closeBox()"));
		box->closeBox();
		LOG(("DLC: ClearBox OK handler EXIT"));
	});
	box->addButton(tr::lng_cancel(), [=] {
		LOG(("DLC: ClearBox Cancel clicked"));
		box->closeBox();
	});
	LOG(("DLC: DownloadCenterClearBox EXIT"));
}
} // namespace Settings
