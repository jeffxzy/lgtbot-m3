// Copyright (c) 2018-present, Chang Liu <github.com/slontia>. All rights reserved.
//
// This source code is licensed under LGPLv2 (found in the LICENSE file).

#pragma once

#include <mutex>
#include <map>
#include <set>
#include <optional>

#include "bot_core/match_manager.h"
#include "bot_core/id.h"
#include "bot_core/db_manager.h"
#include "bot_core/options.h"

#include <dirent.h>

using GameHandleMap = std::map<std::string, std::unique_ptr<GameHandle>>;

class BotCtx
{
  public:
    BotCtx(const BotOption& option, std::unique_ptr<DBManagerBase> db_manager);
    BotCtx(const BotCtx&) = delete;
    BotCtx(BotCtx&&) = delete;
    MatchManager& match_manager() { return match_manager_; }

#ifdef TEST_BOT
    auto& game_handles() { return game_handles_; }
#else
    const auto& game_handles() const { return game_handles_; }
#endif

    bool HasAdmin(const UserID uid) const { return admins_.find(uid) != admins_.end(); }

    const std::string& game_path() const { return game_path_; }

    DBManagerBase* db_manager() const { return db_manager_.get(); }

    const UserID this_uid() const { return this_uid_; }

    auto& option() { return mutable_bot_options_; }
    const auto& options() const { return mutable_bot_options_; }

  private:
    void LoadGameModules_(const char* const games_path);
    void LoadAdmins_(const std::string_view& admins);
    void HandleConfig_(const std::filesystem::path::value_type* const conf_path);

    const UserID this_uid_;
    const std::string game_path_;
    std::mutex mutex_;
    GameHandleMap game_handles_;
    std::set<UserID> admins_;
    MatchManager match_manager_;
#ifdef WITH_SQLITE
    std::unique_ptr<DBManagerBase> db_manager_;
#endif
    MutableBotOption mutable_bot_options_;
};
