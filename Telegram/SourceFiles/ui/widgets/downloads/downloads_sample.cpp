/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/downloads/downloads_sample.h"

#include <algorithm>

namespace Sample {

const std::vector<Row> &InitialRows() {
	static const auto rows = [] {
		std::vector<Row> list;

		auto add = [&](
			QString fileName,
			QString chatName,
			QString context,
			Kind kind,
			State state,
			int percent,
			QString sizeText,
			QString dateText,
			bool isPlaying = false) {
			Row row;
			row.fileName = fileName;
			row.chatName = chatName;
			row.context = context;
			row.kind = kind;
			row.state = state;
			row.percent = percent;
			row.sizeText = sizeText;
			row.dateText = dateText;
			row.isPlaying = isPlaying;
			list.push_back(row);
		};

		add(u"Screenshot_2026-07-18.png"_q, u"Alice Chen"_q, u"Design Team"_q, Kind::Photo, State::Completed, 100, u"3.2 MB"_q, u"Today, 14:32"_q);
		add(u"product_demo_final_v3.mp4"_q, u"Marcus Webb"_q, u"Product Launch · 4:22"_q, Kind::Video, State::Completed, 100, u"124 MB"_q, u"Today, 11:05"_q);
		add(u"Q3_Financial_Report_2026.pdf"_q, u"Finance Dept"_q, u"Company General"_q, Kind::Document, State::Downloading, 67, u"8.4 MB"_q, u"Today, 09:47"_q, true);
		add(u"Ambient_Focus_Session_01.mp3"_q, u"Creative Hub Bot"_q, u"Music Sharing · 38:44"_q, Kind::Audio, State::Completed, 100, u"9.1 MB"_q, u"Yesterday, 22:15"_q);
		add(u"office_blueprint_v2.jpg"_q, u"Ryan Torres"_q, u"Office Renovation"_q, Kind::Photo, State::Completed, 100, u"1.7 MB"_q, u"Yesterday, 18:30"_q);
		add(u"Brand_Guidelines_2026_Complete.pdf"_q, u"Ava Nguyen"_q, u"Design Team"_q, Kind::Document, State::Paused, 32, u"45.2 MB"_q, u"Yesterday, 16:12"_q);
		add(u"onboarding_welcome_video.mov"_q, u"HR Department"_q, u"New Employees · 12:08"_q, Kind::Video, State::Completed, 100, u"208 MB"_q, u"Yesterday, 14:00"_q);
		add(u"team_photo_july_2026.jpg"_q, u"Elena Kozlov"_q, u"Company General"_q, Kind::Photo, State::Completed, 100, u"5.8 MB"_q, u"Jul 17, 10:22"_q);
		add(u"Architecture_Proposal_v4.docx"_q, u"Dev Backend"_q, u"Engineering"_q, Kind::Document, State::Completed, 100, u"2.1 MB"_q, u"Jul 17, 09:14"_q);
		add(u"Podcast_Episode_112_Tech_Trends.mp3"_q, u"TechCast Bot"_q, u"Saved Messages · 1:02:17"_q, Kind::Audio, State::Downloading, 84, u"54.7 MB"_q, u"Jul 16, 20:00"_q, true);
		add(u"mockup_hero_section_r2.png"_q, u"Ava Nguyen"_q, u"Design Team"_q, Kind::Photo, State::Completed, 100, u"4.4 MB"_q, u"Jul 16, 15:45"_q);
		add(u"Voice message.ogg"_q, u"Marcus Webb"_q, u"Product Launch · 0:22"_q, Kind::Voice, State::Completed, 100, u"0.3 MB"_q, u"Jul 16, 13:00"_q);
		add(u"figma.com/file/xkQ9...prototype.url"_q, u"Ava Nguyen"_q, u"Design Team"_q, Kind::Link, State::Completed, 100, u"—"_q, u"Jul 15, 11:30"_q);
		add(u"server_logs_2026-07-14.zip"_q, u"DevOps Bot"_q, u"Engineering"_q, Kind::Archive, State::Completed, 100, u"312 MB"_q, u"Jul 14, 08:00"_q);
		add(u"ux_walkthrough_iteration5.mp4"_q, u"Ava Nguyen"_q, u"Design Team · 6:54"_q, Kind::Video, State::Completed, 100, u"88 MB"_q, u"Jul 13, 17:22"_q);

		return list;
	}();
	return rows;
}

int ActiveCount(const std::vector<Row> &rows) {
	return int(std::ranges::count_if(rows, [](const Row &r) {
		return r.state == State::Downloading;
	}));
}

} // namespace Sample