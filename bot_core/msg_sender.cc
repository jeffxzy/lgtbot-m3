// Copyright (c) 2018-present, Chang Liu <github.com/slontia>. All rights reserved.
//
// This source code is licensed under LGPLv2 (found in the LICENSE file).

#include <filesystem>

#include "msg_sender.h"
#include "bot_core/match.h"

bool DownloadUserAvatar(const char* const uid, const std::filesystem::path::value_type* const dest_filename);

std::string GetUserAvatar(const char* const uid, const int32_t size)
{
    const auto path = (std::filesystem::current_path() / ".image" / "avatar" / uid) += ".png";
    std::filesystem::create_directories(path.parent_path());
    if (!DownloadUserAvatar(uid, path.c_str())) {
        return "";
    }
    return "<img src=\"file://" + path.string() + "\" style=\"width:" + std::to_string(size) + "px; height:" +
        std::to_string(size) + "px; border-radius:50%; vertical-align: middle;\"/>";
}

void MsgSender::SavePlayer(const PlayerID& pid, const bool is_at)
{
    if (!match_) {
        SaveText_("[" + std::to_string(pid) + "号玩家]");
        return;
    }
    SaveText_("[" + std::to_string(pid) + "号：");
    const auto& id = match_->ConvertPid(pid);
    if (const auto pval = std::get_if<ComputerID>(&id)) {
        SaveText_("机器人" + std::to_string(*pval) + "号");
    } else {
        SaveUser(std::get<UserID>(id), is_at);
    }
    SaveText_("]");
}

