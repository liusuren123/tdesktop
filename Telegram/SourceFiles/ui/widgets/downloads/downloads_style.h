/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtGui/QColor>

namespace DownloadsStyle {

inline constexpr QColor bg()             { return QColor(0x1F, 0x18, 0x18); }
inline constexpr QColor rowBg()          { return QColor(0, 0, 0, 0); }
inline constexpr QColor rowBorder()      { return QColor(255, 255, 255, 15); }
inline constexpr QColor accent()         { return QColor(0x2A, 0xAB, 0xEE); }
inline constexpr QColor textFg()         { return QColor(0xFF, 0xFF, 0xFF); }
inline constexpr QColor textDim()        { return QColor(0xFF, 0xFF, 0xFF, 140); }
inline constexpr QColor textMuted()      { return QColor(0xFF, 0xFF, 0xFF, 90); }
inline constexpr QColor textMutedAlt()   { return QColor(0xFF, 0xFF, 0xFF, 100); }
inline constexpr QColor doneFg()         { return QColor(0x27, 0xAE, 0x60); }
inline constexpr QColor doneBg()         { return QColor(0x27, 0xAE, 0x60, 38); }
inline constexpr QColor pausedFg()       { return QColor(0xF3, 0x9C, 0x12); }
inline constexpr QColor pausedBg()       { return QColor(0xF3, 0x9C, 0x12, 38); }
inline constexpr QColor activeFg()       { return QColor(0x2A, 0xAB, 0xEE); }
inline constexpr QColor activeBg()       { return QColor(0x2A, 0xAB, 0xEE, 38); }
inline constexpr QColor failedFg()       { return QColor(0xE7, 0x4C, 0x3C); }
inline constexpr QColor failedBg()       { return QColor(0xE7, 0x4C, 0x3C, 38); }
inline constexpr QColor navActive()      { return QColor(0x2A, 0xAB, 0xEE, 51); }
inline constexpr QColor thumbnailPlaceholder() { return QColor(255, 255, 255, 20); }
inline constexpr QColor playingOverlay() { return QColor(0, 0, 0, 89); }
inline constexpr QColor progressFill()   { return QColor(0x2A, 0xAB, 0xEE); }
inline constexpr QColor progressFillWarn() { return QColor(0xF3, 0x9C, 0x12); }
inline constexpr QColor pdfFg()          { return QColor(0xE7, 0x4C, 0x3C); }
inline constexpr QColor mp3Fg()          { return QColor(0x9B, 0x59, 0xB6); }
inline constexpr QColor oggFg()          { return QColor(0x27, 0xAE, 0x60); }
inline constexpr QColor docxFg()         { return QColor(0x29, 0x80, 0xB9); }
inline constexpr QColor zipFg()          { return QColor(0xF3, 0x9C, 0x12); }
inline constexpr QColor urlFg()          { return QColor(0x2A, 0xAB, 0xEE); }

} // namespace DownloadsStyle