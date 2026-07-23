/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_row_delegate.h"

#include "lang/lang_keys.h"
#include "styles/style_downloads_icons.h"
#include "styles/style_widgets.h"
#include "ui/painter.h"
#include "ui/widgets/downloads/downloads_kind.h"
#include "ui/widgets/downloads/downloads_style.h"
#include "ui/widgets/downloads/downloads_table_model.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>

namespace Ui {
namespace {

using KindUtil::AbbreviationColorFor;
using KindUtil::AbbreviationFor;

constexpr int kActionButtonSize = 28;
constexpr int kActionButtonSpacing = 4;
constexpr int kActionIconSize = 11;

// Total rectangle spanning every column of the row containing `option`.
[[nodiscard]] QRect FullRowRect(const QStyleOptionViewItem &option) {
	auto *view = qobject_cast<QTableView*>(const_cast<QWidget*>(option.widget));
	if (!view) return option.rect;
	auto *header = view->horizontalHeader();
	if (!header) return option.rect;
	const auto x = int(header->sectionPosition(0));
	const auto w = int(header->length());
	return QRect(x, option.rect.y(), w, option.rect.height());
}

[[nodiscard]] QColor StatusFgFor(Sample::State state) {
	switch (state) {
	case Sample::State::Downloading: return DownloadsStyle::activeFg();
	case Sample::State::Paused:     return DownloadsStyle::pausedFg();
	case Sample::State::Failed:     return DownloadsStyle::failedFg();
	case Sample::State::Completed:
	default:                        return DownloadsStyle::doneFg();
	}
}

[[nodiscard]] QColor StatusBgFor(Sample::State state) {
	switch (state) {
	case Sample::State::Downloading: return DownloadsStyle::activeBg();
	case Sample::State::Paused:     return DownloadsStyle::pausedBg();
	case Sample::State::Failed:     return DownloadsStyle::failedBg();
	case Sample::State::Completed:
	default:                        return DownloadsStyle::doneBg();
	}
}

struct ActionSlot {
	QRect rect;
	const style::icon *icon;
};

[[nodiscard]] std::array<ActionSlot, 3> ActionSlots(const QRect &cell) {
	// Centre the buttons vertically in the full cell — including the
	// outer padding above and below the content row.
	const auto buttonY = cell.y() + (cell.height() - kActionButtonSize) / 2;
	const auto totalW = kActionButtonSize * 3 + kActionButtonSpacing * 2;
	auto x = cell.right() - totalW;

	const auto save = x;
	x += kActionButtonSize + kActionButtonSpacing;
	const auto folder = x;
	x += kActionButtonSize + kActionButtonSpacing;
	const auto remove = x;

	return {{
		{ QRect(save,   buttonY, kActionButtonSize, kActionButtonSize), &st::downloadsActionSave },
		{ QRect(folder, buttonY, kActionButtonSize, kActionButtonSize), &st::downloadsActionFolder },
		{ QRect(remove, buttonY, kActionButtonSize, kActionButtonSize), &st::downloadsActionTrash },
	}};
}

} // namespace

DownloadsRowDelegate::DownloadsRowDelegate(QObject *parent)
: QStyledItemDelegate(parent) {
}

QSize DownloadsRowDelegate::sizeHint(
		const QStyleOptionViewItem &option,
		const QModelIndex &index) const {
	return QSize(
		option.rect.width(),
		st::downloadsRowHeight + st::downloadsRowOuterPadding * 2);
}

void DownloadsRowDelegate::paint(
		QPainter *painter,
		const QStyleOptionViewItem &option,
		const QModelIndex &index) const {
	auto *tableModel = qobject_cast<DownloadsTableModel*>(
		const_cast<QAbstractItemModel*>(index.model()));
	if (!tableModel) return;

	const auto &row = tableModel->rowAt(index.row());
	const auto col = index.column();
	auto opt = option;
	initStyleOption(&opt, index);
	opt.text.clear();
	opt.icon = QIcon();

	const auto fullRow = FullRowRect(option);

	// Column 0 paints the row-spanning background + bottom separator.
	if (col == DownloadsTableModel::NameColumn) {
		auto hq = PainterHighQualityEnabler(*painter);
		painter->fillRect(fullRow, DownloadsStyle::bg());
		painter->setPen(QPen(DownloadsStyle::rowBorder(), st::downloadsRowBorderWidth));
		const auto borderY = fullRow.bottom();
		painter->drawLine(fullRow.left(), borderY, fullRow.right(), borderY);
	}

	switch (col) {
	case DownloadsTableModel::NameColumn:
		paintNameBlock(painter, opt.rect, row);
		break;
	case DownloadsTableModel::ProgressColumn:
		paintProgressColumn(painter, opt.rect, row);
		break;
	case DownloadsTableModel::SizeColumn:
		paintSizeColumn(painter, opt.rect, row);
		break;
	case DownloadsTableModel::DateColumn:
		paintDateColumn(painter, opt.rect, row);
		break;
	case DownloadsTableModel::ActionsColumn:
		paintActionsColumn(painter, opt.rect, row, false, {});
		break;
	}
}

void DownloadsRowDelegate::paintThumbnail(
		QPainter *painter,
		const QRect &r,
		const Sample::Row &row) const {
	auto hq = PainterHighQualityEnabler(*painter);
	const auto radius = st::downloadsThumbnailRadius;
	const auto path = [&](const QRectF &rect) {
		QPainterPath pp;
		pp.addRoundedRect(rect, radius, radius);
		return pp;
	};

	if (row.kind == Sample::Kind::Photo || row.kind == Sample::Kind::Video) {
		painter->fillPath(path(r), DownloadsStyle::thumbnailPlaceholder());
		if (row.isPlaying) {
			painter->fillPath(path(r), DownloadsStyle::playingOverlay());
			const auto cx = r.width() / 2.0;
			const auto cy = r.height() / 2.0;
			const auto side = r.width() * 0.28;
			QPainterPath triangle;
			triangle.moveTo(cx - side * 0.5, cy - side * 0.7);
			triangle.lineTo(cx + side * 0.7, cy);
			triangle.lineTo(cx - side * 0.5, cy + side * 0.7);
			triangle.closeSubpath();
			painter->fillPath(triangle, DownloadsStyle::textFg());
		}
		return;
	}

	painter->fillPath(path(r), DownloadsStyle::bg());
	const auto border = QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5);
	painter->setPen(QPen(DownloadsStyle::rowBorder(), st::downloadsRowBorderWidth));
	painter->drawPath(path(border));

	const auto abbrev = AbbreviationFor(row.kind, row.fileName);
	if (abbrev.isEmpty()) return;
	painter->setPen(AbbreviationColorFor(row.kind));
	auto font = st::downloadsFileIconFont->f;
	painter->setFont(font);
	// Centre the abbreviation in the thumbnail. drawText(rect, AlignCenter)
	// positions the rendered glyph block at the visual centre of `r`,
	// independent of font metric quirks (ascent/descent/boundingRect can
	// differ across platforms and break baseline-based centering).
	painter->drawText(r, Qt::AlignCenter, abbrev);
}

void DownloadsRowDelegate::paintNameBlock(
		QPainter *painter,
		const QRect &cell,
		const Sample::Row &row) const {
	const auto outerPad = st::downloadsRowOuterPadding;
	const auto horizontalPad = st::downloadsRowHorizontalPadding;
	const auto thumbnailR = QRect(
		cell.x() + horizontalPad,
		cell.y() + outerPad,
		st::downloadsThumbnailSize,
		st::downloadsThumbnailSize);
	paintThumbnail(painter, thumbnailR, row);

	const auto gap = st::downloadsTabGap;
	const auto nameX = thumbnailR.right() + gap;
	const auto nameW = cell.width() - horizontalPad - (nameX - cell.x());
	const auto nameY = cell.y() + outerPad;

	auto nameFont = st::downloadsRowName.style.font;
	painter->setFont(nameFont);
	painter->setPen(st::downloadsRowName.textFg->c);
	const auto nameMetrics = QFontMetrics(nameFont);
	const auto nameRect = QRect(nameX, nameY, nameW, nameMetrics.height());
	painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignTop,
		nameMetrics.elidedText(row.fileName, Qt::ElideRight, nameW));

	const auto sourceFont = st::downloadsRowSource.style.font;
	painter->setFont(sourceFont);
	painter->setPen(st::downloadsRowSource.textFg->c);
	const auto sourceMetrics = QFontMetrics(sourceFont);
	const auto sourceText = u"from %1 · %2"_q.arg(row.chatName).arg(row.context);
	const auto sourceY = nameRect.bottom() + 2;
	const auto sourceRect = QRect(nameX, sourceY, nameW, sourceMetrics.height());
	painter->drawText(sourceRect, Qt::AlignLeft | Qt::AlignTop,
		sourceMetrics.elidedText(sourceText, Qt::ElideRight, nameW));
}

void DownloadsRowDelegate::paintStatusBadge(
		QPainter *painter,
		const QRect &area,
		const Sample::Row &row,
		bool rightAligned) const {
	const auto text = [&]() -> QString {
		switch (row.state) {
		case Sample::State::Downloading: return QString::number(row.percent) + u"%"_q;
		case Sample::State::Paused:     return tr::lng_downloads_status_paused(tr::now);
		case Sample::State::Failed:     return tr::lng_downloads_status_failed(tr::now);
		case Sample::State::Completed:
		default:                        return tr::lng_downloads_status_done(tr::now);
		}
	}();

	const auto bg = StatusBgFor(row.state);
	const auto fg = StatusFgFor(row.state);

	painter->setFont(st::downloadsBadgeFont->f);
	const auto metrics = QFontMetrics(st::downloadsBadgeFont->f);
	const auto hPad = st::downloadsBadgePadding;
	const auto vPad = (area.height() - metrics.height()) / 2;
	const auto textWidth = metrics.horizontalAdvance(text);
	const auto badgeW = textWidth + hPad * 2;
	const auto badgeH = metrics.height();
	const auto badgeX = rightAligned
		? (area.right() - badgeW + 1)
		: area.x();
	const auto badgeY = area.y() + vPad;
	const auto pill = QRectF(badgeX, badgeY, badgeW, badgeH);

	auto hq = PainterHighQualityEnabler(*painter);
	QPainterPath path;
	path.addRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);
	painter->fillPath(path, bg);

	painter->setPen(fg);
	painter->drawText(pill, Qt::AlignCenter, text);
}

void DownloadsRowDelegate::paintProgressColumn(
		QPainter *painter,
		const QRect &cell,
		const Sample::Row &row) const {
	// Layout (top → bottom):
	//   [progress bar — right-aligned within content area]
	//   [status badge — right-aligned within content area]
	//
	// The cell is wider than the visible progress content: an extra
	// `downloadsProgressGap` is reserved on the right as visual breathing
	// room before the Size column starts. Both the bar and the badge stay
	// within the un-gapped area and end at the same x (right edge of the
	// 88px content block), so they look right-aligned with each other.
	const auto outerPad = st::downloadsRowOuterPadding;
	const auto trackH = st::downloadsProgressHeight;
	const auto gap = int(st::downloadsProgressGap);
	const auto contentW = cell.width() - gap;

	// Where to anchor the bar — vertically centred in the upper half of the row.
	const auto trackY = cell.y() + outerPad
		+ (st::downloadsRowHeight - trackH) / 2
		- st::downloadsRowStatusHeight;

	// Pick fill colour and fill width by state.
	QColor fill;
	int fillPercent = 0;
	switch (row.state) {
	case Sample::State::Completed:
		fill = DownloadsStyle::doneFg();
		fillPercent = 100;
		break;
	case Sample::State::Downloading:
		fill = DownloadsStyle::progressFill();
		fillPercent = row.percent;
		break;
	case Sample::State::Paused:
		fill = DownloadsStyle::progressFillWarn();
		fillPercent = row.percent;
		break;
	case Sample::State::Failed:
		fill = DownloadsStyle::failedFg();
		fillPercent = 100;
		break;
	}

	// Track background — full content width, subtle border colour.
	painter->fillRect(QRectF(cell.x(), trackY, contentW, trackH),
		DownloadsStyle::rowBorder());
	// Fill — coloured segment, drawn left-to-right (left edge at cell.x()).
	const auto fillW = (contentW * fillPercent) / 100;
	painter->fillRect(QRectF(cell.x(), trackY, fillW, trackH),
		fill);

	// Status badge sits below the bar, right-aligned within the content area.
	const auto badgeH = st::downloadsBadgeHeight;
	const auto badgeArea = QRect(
		cell.x(),
		trackY + trackH + 2,
		contentW,
		badgeH);
	paintStatusBadge(painter, badgeArea, row, true);
}

void DownloadsRowDelegate::paintSizeColumn(
		QPainter *painter,
		const QRect &cell,
		const Sample::Row &row) const {
	const auto outerPad = st::downloadsRowOuterPadding;
	auto font = st::downloadsRowSize.style.font;
	painter->setFont(font);
	painter->setPen(st::downloadsRowSize.textFg->c);
	const auto metrics = QFontMetrics(font);
	const auto r = QRect(
		cell.x(),
		cell.y() + outerPad,
		cell.width(),
		st::downloadsRowHeight);
	painter->drawText(r,
		Qt::AlignHCenter | Qt::AlignVCenter,
		metrics.elidedText(row.sizeText, Qt::ElideRight, r.width()));
}

void DownloadsRowDelegate::paintDateColumn(
		QPainter *painter,
		const QRect &cell,
		const Sample::Row &row) const {
	const auto outerPad = st::downloadsRowOuterPadding;
	auto font = st::downloadsRowDate.style.font;
	painter->setFont(font);
	painter->setPen(st::downloadsRowDate.textFg->c);
	const auto metrics = QFontMetrics(font);
	const auto r = QRect(
		cell.x(),
		cell.y() + outerPad,
		cell.width(),
		st::downloadsRowHeight);
	painter->drawText(r,
		Qt::AlignLeft | Qt::AlignVCenter,
		metrics.elidedText(row.dateText, Qt::ElideRight, r.width()));
}

void DownloadsRowDelegate::paintActionsColumn(
		QPainter *painter,
		const QRect &cell,
		const Sample::Row &row,
		bool hovered,
		const QPoint &hoverPos) const {
	for (const auto &slot : ActionSlots(cell)) {
		const auto isHover = hovered && slot.rect.contains(hoverPos);
		if (isHover) {
			auto hq = PainterHighQualityEnabler(*painter);
			QPainterPath path;
			path.addRoundedRect(slot.rect, 6, 6);
			painter->fillPath(path, QColor(255, 255, 255, 15));
		}
		const auto iconY = slot.rect.y() + (kActionButtonSize - kActionIconSize) / 2;
		const auto iconX = slot.rect.x() + (kActionButtonSize - kActionIconSize) / 2;
		slot.icon->paint(
			*painter,
			iconX,
			iconY,
			kActionIconSize,
			DownloadsStyle::textMutedAlt());
	}
}

DownloadsRowDelegate::ActionHit DownloadsRowDelegate::hitAction(
		const QPoint &pos,
		const QRect &cellRect) {
	const auto slots = ActionSlots(cellRect);
	for (size_t i = 0; i < slots.size(); ++i) {
		if (slots[i].rect.contains(pos)) {
			switch (i) {
			case 0: return SaveAction;
			case 1: return FolderAction;
			case 2: return RemoveAction;
			}
		}
	}
	return NoAction;
}

bool DownloadsRowDelegate::editorEvent(
		QEvent *event,
		QAbstractItemModel *model,
		const QStyleOptionViewItem &option,
		const QModelIndex &index) {
	if (event->type() == QEvent::MouseButtonPress) {
		auto *me = static_cast<QMouseEvent*>(event);
		if (me->button() == Qt::RightButton) {
			Q_EMIT rowContextMenu(index.row(), me->globalPos());
			return true;
		}
		if (me->button() == Qt::LeftButton
			&& index.column() == DownloadsTableModel::ActionsColumn) {
			const auto local = me->pos() - option.rect.topLeft();
			const auto action = hitAction(local, option.rect);
			switch (action) {
			case SaveAction:   Q_EMIT saveClicked(index.row());   return true;
			case FolderAction: Q_EMIT folderClicked(index.row()); return true;
			case RemoveAction: Q_EMIT removeClicked(index.row()); return true;
			case NoAction:
			default: break;
			}
		}
		// A left click that didn't land on an action button selects the row
		// (opens the detail panel).
		if (me->button() == Qt::LeftButton) {
			Q_EMIT rowClicked(index.row());
			return true;
		}
	}
	return false;
}

} // namespace Ui