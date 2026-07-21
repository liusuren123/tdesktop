/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_section.h"

#include "ui/widgets/downloads/downloads_content.h"
#include "window/window_session_controller.h"

#include <QtGui/QResizeEvent>

namespace Ui {

object_ptr<Window::SectionWidget> DownloadsMemento::createWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		Window::Column column,
		const QRect &geometry) {
	auto result = object_ptr<DownloadsWidget>(parent, controller);
	result->setGeometry(geometry);
	return result;
}

DownloadsWidget::DownloadsWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
: SectionWidget(parent, controller) {
	setAttribute(Qt::WA_OpaquePaintEvent);

	_content = Ui::CreateChild<DownloadsContent>(this);
	_content->setGeometry(rect());
}

bool DownloadsWidget::showInternal(
		not_null<Window::SectionMemento*> memento,
		const Window::SectionShow &params) {
	if (!sameTypeAs(memento)) {
		return false;
	}
	setFocus();
	return true;
}

bool DownloadsWidget::sameTypeAs(
		not_null<Window::SectionMemento*> memento) {
	return dynamic_cast<DownloadsMemento*>(memento.get()) != nullptr;
}

QRect DownloadsWidget::floatPlayerAvailableRect() {
	return _content ? _content->geometry() : rect();
}

bool DownloadsWidget::floatPlayerHandleWheelEvent(QEvent *e) {
	return false;
}

void DownloadsWidget::resizeEvent(QResizeEvent *e) {
	SectionWidget::resizeEvent(e);
	_content->setGeometry(rect());
}

std::shared_ptr<Window::SectionMemento> MakeDownloadsMemento() {
	return std::make_shared<DownloadsMemento>();
}

} // namespace Ui