/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/object_ptr.h"
#include "window/section_memento.h"
#include "window/section_widget.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Ui {

class DownloadsContent;

class DownloadsMemento final : public Window::SectionMemento {
public:
	[[nodiscard]] object_ptr<Window::SectionWidget> createWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		Window::Column column,
		const QRect &geometry) override;

	[[nodiscard]] bool instant() const override {
		return true;
	}
};

class DownloadsWidget final : public Window::SectionWidget {
public:
	DownloadsWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	bool showInternal(
		not_null<Window::SectionMemento*> memento,
		const Window::SectionShow &params) override;
	bool sameTypeAs(not_null<Window::SectionMemento*> memento) override;

	QRect floatPlayerAvailableRect() override;
	bool floatPlayerHandleWheelEvent(QEvent *e) override;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	DownloadsContent *_content = nullptr;
};

[[nodiscard]] std::shared_ptr<Window::SectionMemento> MakeDownloadsMemento();

} // namespace Ui