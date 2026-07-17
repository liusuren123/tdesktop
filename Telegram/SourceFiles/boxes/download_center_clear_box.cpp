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
#include <vector>
namespace Settings {
void DownloadCenterClearBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		DownloadCenterClearKind kind) {
	const auto title = (kind == DownloadCenterClearKind::Completed)
		? tr::lng_download_center_clear_completed_title()
		: (kind == DownloadCenterClearKind::Failed)
		? tr::lng_download_center_clear_failed_title()
		: tr::lng_download_center_clear_all_title();
	box->setTitle(title);
	box->addButton(tr::lng_box_ok(), [=, &controller = controller] {
			auto &center = controller->session().downloadCenter();
			const auto want = kind;
			std::vector<Data::DownloadTaskId> toRemove;
			toRemove.reserve(center.totalCount());
			for (const auto *task : center.tasks()) {
				const auto remove = (want == DownloadCenterClearKind::All)
					|| (want == DownloadCenterClearKind::Completed
						&& task->state == Data::DownloadState::Completed)
					|| (want == DownloadCenterClearKind::Failed
						&& task->state == Data::DownloadState::Failed);
				if (remove) {
					toRemove.push_back(task->id);
				}
			}
			for (const auto id : toRemove) {
				center.remove(id);
			}
			box->closeBox();
		});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}
} // namespace Settings
