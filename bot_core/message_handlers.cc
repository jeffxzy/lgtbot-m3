// Copyright (c) 2018-present, Chang Liu <github.com/slontia>. All rights reserved.
//
// This source code is licensed under LGPLv2 (found in the LICENSE file).

#include <algorithm>
#include <ranges>
#include <cmath>

#include "bot_core/message_handlers.h"

#include "utility/msg_checker.h"
#include "utility/html.h"
#include "utility/log.h"
#include "bot_core/bot_core.h"
#include "bot_core/db_manager.h"
#include "bot_core/match.h"
#include "bot_core/image.h"
#include "bot_core/options.h"

// para func can appear only once
#define RETURN_IF_FAILED(func)                                 \
    do {                                                       \
        if (const auto ret = (func); ret != EC_OK) return ret; \
    } while (0);

static MetaCommand make_command(const char* const description, const auto& cb, auto&&... checkers)
{
    return MetaCommand(description, cb, std::move(checkers)...);
};

namespace {

struct MetaCommandDesc
{
    bool is_common_;
    MetaCommand cmd_;
};

struct MetaCommandGroup
{
    std::string group_name_;
    std::vector<MetaCommand> desc_;
};

struct ShowCommandOption
{
    bool only_common_ = true;
    bool with_html_color_ = false;
    bool with_example_ = true;
};

}

extern const std::vector<MetaCommandGroup> meta_cmds;
extern const std::vector<MetaCommandGroup> admin_cmds;

static ErrCode help_internal(BotCtx& bot, MsgSenderBase& reply, const std::vector<MetaCommandGroup>& cmd_groups,
        const ShowCommandOption& option, const std::string& type_name)
{
    std::string outstr = "## 可使用的" + type_name + "指令";
    for (const MetaCommandGroup& cmd_group : cmd_groups) {
        int i = 0;
        outstr += "\n\n### " + cmd_group.group_name_;
        for (const MetaCommand& cmd : cmd_group.desc_) {
            outstr += "\n" + std::to_string(++i) + ". " + cmd.Info(option.with_example_, option.with_html_color_);
        }
    }
    if (option.with_html_color_) {
        reply() << Markdown(outstr);
    } else {
        reply() << outstr;
    }
    return EC_OK;
}

template <bool IS_ADMIN = false>
static ErrCode help(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply, const bool show_text) {
    
    return help_internal(
            bot, reply, IS_ADMIN ? admin_cmds : meta_cmds,
            ShowCommandOption{.only_common_ = show_text, .with_html_color_ = !show_text, .with_example_ = !show_text},
            IS_ADMIN ? "管理" : "元");
}

ErrCode HandleRequest(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, MsgReader& reader,
                      MsgSenderBase& reply, const std::vector<MetaCommandGroup>& cmd_groups)
{
    reader.Reset();
    for (const MetaCommandGroup& cmd_group : cmd_groups) {
        for (const MetaCommand& cmd : cmd_group.desc_) {
            const std::optional<ErrCode> errcode = cmd.CallIfValid(reader, bot, uid, gid, reply);
            if (errcode.has_value()) {
                return *errcode;
            }
        }
    }
    return EC_REQUEST_NOT_FOUND;
}

ErrCode HandleMetaRequest(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, const std::string& msg,
                          MsgSenderBase& reply)
{
    MsgReader reader(msg);
    const auto ret = HandleRequest(bot, uid, gid, reader, reply, meta_cmds);
    if (ret == EC_REQUEST_NOT_FOUND) {
        reply() << "[错误] 未预料的元指令，您可以通过「#帮助」查看所有支持的元指令";
    }
    return ret;
}

ErrCode HandleAdminRequest(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, const std::string& msg,
                           MsgSenderBase& reply)
{
    MsgReader reader(msg);
    const auto ret = HandleRequest(bot, uid, gid, reader, reply, admin_cmds);
    if (ret == EC_REQUEST_NOT_FOUND) {
        reply() << "[错误] 未预料的管理指令，您可以通过「%帮助」查看所有支持的管理指令";
    }
    return ret;
}

static ErrCode show_gamelist(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid,
                             MsgSenderBase& reply, const bool show_text)
{
    int i = 0;
    if (bot.game_handles().empty()) {
        reply() << "未载入任何游戏";
        return EC_OK;
    }
    if (show_text) {
        auto sender = reply();
        sender << "游戏列表：";
        for (const auto& [name, info] : bot.game_handles()) {
            sender << "\n" << (++i) << ". " << name;
            if (info->multiple_ == 0) {
                sender << "（试玩）";
            }
        }
    } else {
        html::Table table(0, 4);
        table.SetTableStyle(" align=\"center\" border=\"1px solid #ccc\" cellpadding=\"5\" cellspacing=\"1\" ");
        const auto game_handles_range = std::views::transform(bot.game_handles(), [](const auto& p) { return &p; });
        auto game_handles = std::vector(std::ranges::begin(game_handles_range), std::ranges::end(game_handles_range));
        std::ranges::sort(game_handles, [](const auto& _1, const auto& _2) { return _1->second->activity_ > _2->second->activity_; });
        for (const auto& p : game_handles) {
            const auto& name = p->first;
            const auto& info = p->second;
            table.AppendRow();
            table.AppendRow();
            table.MergeDown(table.Row() - 2, 0, 2);
            table.MergeRight(table.Row() - 1, 1, 3);
            table.Get(table.Row() - 2, 0).SetContent("<font size=\"5\"> **" + name + "**</font>\n\n热度：" + std::to_string(info->activity_));
            table.Get(table.Row() - 2, 1).SetContent("开发者：" + info->developer_);
            table.Get(table.Row() - 2, 2).SetContent(info->max_player_ == 0 ? "无玩家数限制" :
                    ("最多 " HTML_COLOR_FONT_HEADER(blue) "**" + std::to_string(info->max_player_) + "**" HTML_FONT_TAIL " 名玩家"));
            table.Get(table.Row() - 2, 3).SetContent(info->multiple_ == 0 ? "不计分" :
                    ("默认 " HTML_COLOR_FONT_HEADER(blue) "**" + std::to_string(info->multiple_) + "**" HTML_FONT_TAIL " 倍分数"));
            table.Get(table.Row() - 1, 1).SetContent("<font size=\"3\"> " + info->description_ + "</font>");
        }
        reply() << Markdown("## 游戏列表\n\n" + table.ToString(), 800);
    }
    return EC_OK;
}

static ErrCode new_game(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, MsgSenderBase& reply,
                        const std::string& gamename, const bool is_single)
{
    const auto it = bot.game_handles().find(gamename);
    if (it == bot.game_handles().end()) {
        reply() << "[错误] 创建失败：未知的游戏名，请通过「#游戏列表」查看游戏名称";
        return EC_REQUEST_UNKNOWN_GAME;
    }
    if (gid.has_value()) {
        const auto running_match = bot.match_manager().GetMatch(*gid);
        ErrCode rc = EC_OK;
        if (running_match && (rc = running_match->Terminate(false /*is_force*/)) != EC_OK) {
            reply() << "[错误] 创建失败：该房间已经开始游戏";
            return rc;
        }
    }
    const auto& [ret, match] = bot.match_manager().NewMatch(*it->second, uid, gid, reply);
    if (ret != EC_OK) {
        return ret;
    }
    if (is_single) {
        RETURN_IF_FAILED(match->SetBenchTo(uid, EmptyMsgSender::Get(), std::nullopt));
        RETURN_IF_FAILED(match->GameStart(uid, gid.has_value(), reply));
    } else {
        auto sender = match->Boardcast();
        if (match->gid().has_value()) {
            sender << "现在玩家可以在群里通过「#加入」报名比赛，房主也可以通过「帮助」（不带#号）查看所有支持的游戏设置";
        } else {
            sender << "现在玩家可以通过私信我「#加入 " << match->MatchId() << "」报名比赛，您也可以通过「帮助」（不带#号）查看所有支持的游戏设置";
        }
        sender << match->BriefInfo();
    }
    return EC_OK;
}

template <typename Fn>
static auto handle_match_by_user(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, MsgSenderBase& reply,
        const Fn& fn, const char* const action_name = "")
{
    const auto match = bot.match_manager().GetMatch(uid);
    if (!match) {
        reply() << "[错误] " << action_name << "失败：您未加入游戏";
        return EC_MATCH_USER_NOT_IN_MATCH;
    }
    if (gid.has_value() && match->gid() != *gid) {
        reply() << "[错误] " << action_name << "失败：您是在其他房间创建的游戏，若您忘记该房间，可以尝试私信裁判";
        return EC_MATCH_NOT_THIS_GROUP;
    }
    return fn(match);
}

static ErrCode set_bench_to(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, MsgSenderBase& reply,
        const uint32_t bench_to_player_num)
{
    return handle_match_by_user(bot, uid, gid, reply,
            [&](const auto& match) { return match->SetBenchTo(uid, reply, bench_to_player_num); }, "配置");
}

static ErrCode set_multiple(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, MsgSenderBase& reply,
        const uint32_t multiple)
{
    return handle_match_by_user(bot, uid, gid, reply,
            [&](const auto& match) { return match->SetMultiple(uid, reply, multiple); }, "配置");
}

static ErrCode start_game(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, MsgSenderBase& reply)
{
    return handle_match_by_user(bot, uid, gid, reply,
            [&](const auto& match) { return match->GameStart(uid, gid.has_value(), reply); }, "开始");
}

static ErrCode leave(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, MsgSenderBase& reply,
        const bool force)
{
    return handle_match_by_user(bot, uid, gid, reply,
            [&](const auto& match) { return match->Leave(uid, reply, force); }, "退出");
}

static ErrCode user_interrupt_game(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, MsgSenderBase& reply,
        const bool cancel)
{
    return handle_match_by_user(bot, uid, gid, reply,
            [&](const auto& match) { return match->UserInterrupt(uid, reply, cancel); }, "中断");
}

static ErrCode join_private(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid,
                            MsgSenderBase& reply, const MatchID mid)
{
    if (gid.has_value()) {
        reply() << "[错误] 加入失败：请私信裁判加入私密游戏，或去掉比赛ID以加入当前房间游戏";
        return EC_MATCH_NEED_REQUEST_PRIVATE;
    }
    const auto match = bot.match_manager().GetMatch(mid);
    if (!match) {
        reply() << "[错误] 加入失败：游戏ID不存在";
        return EC_MATCH_NOT_EXIST;
    }
    if (!match->IsPrivate()) {
        reply() << "[错误] 加入失败：该游戏属于公开比赛，请前往房间加入游戏";
        return EC_MATCH_NEED_REQUEST_PUBLIC;
    }
    RETURN_IF_FAILED(match->Join(uid, reply));
    return EC_OK;
}

static ErrCode join_public(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, MsgSenderBase& reply)
{
    if (!gid.has_value()) {
        reply() << "[错误] 加入失败：若要加入私密游戏，请指明比赛ID";
        return EC_MATCH_NEED_ID;
    }
    const auto match = bot.match_manager().GetMatch(*gid);
    if (!match) {
        reply() << "[错误] 加入失败：该房间未进行游戏";
        return EC_MATCH_GROUP_NOT_IN_MATCH;
    }
    assert(!match->IsPrivate());
    RETURN_IF_FAILED(match->Join(uid, reply));
    return EC_OK;
}

static ErrCode show_private_matches(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
                                    MsgSenderBase& reply)
{
    uint64_t count = 0;
    auto sender = reply();
    const auto matches = bot.match_manager().Matches();
    for (const auto& match : matches) {
        if (match->IsPrivate() && match->state() == Match::State::NOT_STARTED) {
            ++count;
            sender << match->game_handle().name_ << " - [房主ID] " << match->host_uid() << " - [比赛ID] "
                   << match->MatchId() << "\n";
        }
    }
    if (count == 0) {
        sender << "当前无未开始的私密比赛";
    } else {
        sender << "共" << count << "场";
    }
    return EC_OK;
}

static ErrCode show_match_info(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
                                 MsgSenderBase& reply)
{
    std::shared_ptr<Match> match;
    if (gid.has_value() && !(match = bot.match_manager().GetMatch(*gid))) {
        reply() << "[错误] 查看失败：该房间未进行游戏";
        return EC_MATCH_GROUP_NOT_IN_MATCH;
    } else if (!gid.has_value() && !(match = bot.match_manager().GetMatch(uid))) {
        reply() << "[错误] 查看失败：该房间未进行游戏";
        return EC_MATCH_GROUP_NOT_IN_MATCH;
    }
    match->ShowInfo(reply);
    return EC_OK;
}

static ErrCode show_rule(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply,
                         const std::string& gamename, const bool show_text)
{
    const auto it = bot.game_handles().find(gamename);
    if (it == bot.game_handles().end()) {
        reply() << "[错误] 查看失败：未知的游戏名，请通过「#游戏列表」查看游戏名称";
        return EC_REQUEST_UNKNOWN_GAME;
    };
    if (!show_text) {
        reply() << Markdown(it->second->rule_);
        return EC_OK;
    }
    auto sender = reply();
    sender << "最多可参加人数：";
    if (it->second->max_player_ == 0) {
        sender << "无限制";
    } else {
        sender << it->second->max_player_;
    }
    sender << "人\n";
    sender << "详细规则：\n";
    sender << it->second->rule_;
    return EC_OK;
}

static ErrCode show_achievement(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply,
                         const std::string& gamename)
{
    const auto it = bot.game_handles().find(gamename);
    if (it == bot.game_handles().end()) {
        reply() << "[错误] 查看失败：未知的游戏名，请通过「#游戏列表」查看游戏名称";
        return EC_REQUEST_UNKNOWN_GAME;
    };
    if (it->second->achievements_.empty()) {
        reply() << "该游戏没有任何成就";
        return EC_OK;
    }
    html::Table table(1 + it->second->achievements_.size(), bot.db_manager() == nullptr ? 3 : 6);
    table.SetTableStyle(" align=\"center\" border=\"1px solid #ccc\" cellpadding=\"1\" cellspacing=\"1\" width=\"600\" ");
    table.Get(0, 0).SetContent("**序号**");
    table.Get(0, 1).SetContent("**名称**");
    table.Get(0, 2).SetContent("**描述**");
    if (bot.db_manager()) {
        table.Get(0, 3).SetContent("**首次达成时间**");
        table.Get(0, 4).SetContent("**达成次数**");
        table.Get(0, 5).SetContent("**达成人数**");
    }
    for (size_t i = 0; i < it->second->achievements_.size(); ++i) {
        const char* color_header = HTML_COLOR_FONT_HEADER(black);
        if (bot.db_manager()) {
            const auto statistic = bot.db_manager()->GetAchievementStatistic(uid, gamename, it->second->achievements_[i].name_);
            if (statistic.count_ > 0) {
                color_header = HTML_COLOR_FONT_HEADER(green);
            }
            table.Get(1 + i, 3).SetContent(color_header + (statistic.first_achieve_time_.empty() ? "-" : statistic.first_achieve_time_) + HTML_FONT_TAIL);
            table.Get(1 + i, 4).SetContent(color_header + std::to_string(statistic.count_) + HTML_FONT_TAIL);
            table.Get(1 + i, 5).SetContent(color_header + std::to_string(statistic.achieved_user_num_) + HTML_FONT_TAIL);
        }
        table.Get(1 + i, 0).SetContent(color_header + std::to_string(i + 1) + HTML_FONT_TAIL);
        table.Get(1 + i, 1).SetContent(color_header + it->second->achievements_[i].name_ + HTML_FONT_TAIL);
        table.Get(1 + i, 2).SetContent(color_header + it->second->achievements_[i].description_ + HTML_FONT_TAIL);
    }
    reply() << Markdown("## " + gamename + "：成就一览\n\n" + table.ToString());
    return EC_OK;
}

static ErrCode about(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply)
{
    reply() << "LGTBot 内测版本 Beta-v0.1.0"
               "\n"
               "\n作者：森高（QQ：654867229）"
               "\nGitHub：http://github.com/slontia/lgtbot"
               "\n"
               "\n若您使用中遇到任何 BUG 或其它问题，欢迎私信作者，或前往 GitHub 主页提 issue"
               "\n本项目仅供娱乐和技术交流，请勿用于商业用途，健康游戏，拒绝赌博";
    return EC_OK;
}

static ErrCode show_profile(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
                            MsgSenderBase& reply, const TimeRange time_range)
{
    if (!bot.db_manager()) {
        reply() << "[错误] 查看失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    const auto profile = bot.db_manager()->GetUserProfile(uid,
            k_time_range_begin_datetimes[time_range.ToUInt()], k_time_range_end_datetimes[time_range.ToUInt()]);  // TODO: pass sender

    const auto colored_text = [](const auto score, std::string text)
        {
            std::string s;
            if (score < 0) {
                s = HTML_COLOR_FONT_HEADER(red);
            } else if (score > 0) {
                s = HTML_COLOR_FONT_HEADER(green);
            }
            s += std::move(text);
            if (score != 0) {
                s += HTML_FONT_TAIL;
            }
            return s;
        };

    std::string html = std::string("## ") + GetUserAvatar(uid.GetCStr(), 40) + HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE +
        GetUserName(uid.GetCStr(), gid.has_value() ? gid->GetCStr() : nullptr) + "\n";

    html += "\n- **注册时间**：" + (profile.birth_time_.empty() ? "无" : profile.birth_time_) + "\n";

    // title: season score info
    html += "\n<h3 align=\"center\">" HTML_COLOR_FONT_HEADER(blue);
    html += time_range.ToString();
    html += HTML_FONT_TAIL "赛季</h3>\n";

    // score info

    html += "\n- **游戏局数**：" + std::to_string(profile.match_count_);
    html += "\n- **零和总分**：" + colored_text(profile.total_zero_sum_score_, std::to_string(profile.total_zero_sum_score_));
    html += "\n- **头名总分**：" + colored_text(profile.total_top_score_, std::to_string(profile.total_top_score_));

    // game level score info

    html += "\n- **各游戏等级总分**：\n\n";
    if (profile.game_level_infos_.empty()) {
        html += "<p align=\"center\">您本赛季还没有参与过游戏</p>\n\n";
    } else {
        static constexpr const size_t k_level_score_table_num = 2;
        html::Table level_score_table_outside(1, k_level_score_table_num);
        level_score_table_outside.SetTableStyle(" align=\"center\" cellpadding=\"0\" cellspacing=\"0\" width=\"800\" ");
        level_score_table_outside.SetRowStyle(" valign=\"top\" ");
        std::array<html::Table, k_level_score_table_num> level_score_table;
        const size_t game_level_info_num_each_table = (profile.game_level_infos_.size() + k_level_score_table_num - 1) / k_level_score_table_num;
        for (size_t i = 0; i < k_level_score_table_num; ++i) {
            auto& table = level_score_table[i];
            table.SetTableStyle(" align=\"center\" border=\"1px solid #ccc\" cellpadding=\"1\" cellspacing=\"1\" ");
            table.AppendRow();
            table.AppendColumn();
            table.Get(0, 0).SetContent("**序号**");
            table.AppendColumn();
            table.Get(0, 1).SetContent("**游戏名称**");
            table.AppendColumn();
            table.Get(0, 2).SetContent("**局数**");
            table.AppendColumn();
            table.Get(0, 3).SetContent("**等级总分**");
            table.AppendColumn();
            table.Get(0, 4).SetContent("**加权等级总分**");
            table.AppendColumn();
            table.Get(0, 5).SetContent("**评级**");
        }
        for (size_t i = 0; i < profile.game_level_infos_.size(); ++i) {
            const auto& info = profile.game_level_infos_[i];
            const int32_t total_level_score_ = static_cast<int32_t>(info.total_level_score_);
            auto& table = level_score_table[i / game_level_info_num_each_table];
            table.AppendRow();
            table.GetLastRow(0).SetContent(colored_text(total_level_score_ / 100,  std::to_string(i + 1)));
            table.GetLastRow(1).SetContent(colored_text(total_level_score_ / 100, info.game_name_));
            table.GetLastRow(2).SetContent(colored_text(total_level_score_ / 100, std::to_string(info.count_)));
            table.GetLastRow(3).SetContent(colored_text(total_level_score_ / 100, std::to_string(info.total_level_score_)));
            table.GetLastRow(4).SetContent(colored_text(total_level_score_ / 100, std::to_string(std::sqrt(info.count_) * info.total_level_score_)));
            table.GetLastRow(5).SetContent(colored_text(total_level_score_ / 100,
                        total_level_score_ <= -300 ? "E" :
                        total_level_score_ <= -100 ? "D" :
                        total_level_score_ < 100   ? "C" :
                        total_level_score_ < 300   ? "B" :
                        total_level_score_ < 500   ? "A" : "S"));
        }
        for (size_t i = 0; i < k_level_score_table_num; ++i) {
            level_score_table_outside.Get(0, i).SetContent(level_score_table[i].ToString());
        }
        html += "\n\n" + level_score_table_outside.ToString() + "\n\n";
    }

    // title: recent info

    html += "\n<h3 align=\"center\">近期战绩</h3>\n";

    // show recent matches

    html += "\n- **近十场游戏记录**：\n\n";
    if (profile.recent_matches_.empty()) {
        html += "<p align=\"center\">您还没有参与过游戏</p>\n\n";
    } else {
        html::Table recent_matches_table(1, 9);
        recent_matches_table.SetTableStyle(" align=\"center\" border=\"1px solid #ccc\" cellpadding=\"1\" cellspacing=\"1\" width=\"800\" ");
        recent_matches_table.Get(0, 0).SetContent("**序号**");
        recent_matches_table.Get(0, 1).SetContent("**游戏名称**");
        recent_matches_table.Get(0, 2).SetContent("**结束时间**");
        recent_matches_table.Get(0, 3).SetContent("**等价排名**");
        recent_matches_table.Get(0, 4).SetContent("**倍率**");
        recent_matches_table.Get(0, 5).SetContent("**游戏得分**");
        recent_matches_table.Get(0, 6).SetContent("**零和得分**");
        recent_matches_table.Get(0, 7).SetContent("**头名得分**");
        recent_matches_table.Get(0, 8).SetContent("**等级得分**");

        for (uint32_t i = 0; i < profile.recent_matches_.size(); ++i) {
            recent_matches_table.AppendRow();
            const auto match_profile = profile.recent_matches_[i];
            recent_matches_table.Get(i + 1, 0).SetContent(colored_text(match_profile.top_score_, std::to_string(i + 1)));
            recent_matches_table.Get(i + 1, 1).SetContent(colored_text(match_profile.top_score_, match_profile.game_name_));
            recent_matches_table.Get(i + 1, 2).SetContent(colored_text(match_profile.top_score_, match_profile.finish_time_));
            recent_matches_table.Get(i + 1, 3).SetContent(colored_text(match_profile.top_score_, [&match_profile]()
                        {
                            std::stringstream ss;
                            ss.precision(2);
                            ss << (match_profile.user_count_ - float(match_profile.rank_score_) / 2 + 0.5) << " / " << match_profile.user_count_;
                            return ss.str();
                        }()));
            recent_matches_table.Get(i + 1, 4).SetContent(colored_text(match_profile.top_score_, std::to_string(match_profile.multiple_) + " 倍"));
            recent_matches_table.Get(i + 1, 5).SetContent(colored_text(match_profile.top_score_, std::to_string(match_profile.game_score_)));
            recent_matches_table.Get(i + 1, 6).SetContent(colored_text(match_profile.top_score_, std::to_string(match_profile.zero_sum_score_)));
            recent_matches_table.Get(i + 1, 7).SetContent(colored_text(match_profile.top_score_, std::to_string(match_profile.top_score_)));
            recent_matches_table.Get(i + 1, 8).SetContent(colored_text(match_profile.top_score_, std::to_string(match_profile.level_score_)));
        }
        html += recent_matches_table.ToString() + "\n\n";
    }

    // show recent honors

    html += "\n- **近十次荣誉记录**：\n\n";
    if (profile.recent_honors_.empty()) {
        html += "<p align=\"center\">您还没有获得过荣誉</p>\n\n";
    } else {
        html::Table recent_honors_table(1, 3);
        recent_honors_table.SetTableStyle(" align=\"center\" border=\"1px solid #ccc\" cellpadding=\"1\" cellspacing=\"1\" width=\"800\" ");
        recent_honors_table.Get(0, 0).SetContent("**ID**");
        recent_honors_table.Get(0, 1).SetContent("**荣誉**");
        recent_honors_table.Get(0, 2).SetContent("**获得时间**");
        for (const auto& info : profile.recent_honors_) {
            recent_honors_table.AppendRow();
            recent_honors_table.GetLastRow(0).SetContent(std::to_string(info.id_));
            recent_honors_table.GetLastRow(1).SetContent(info.description_);
            recent_honors_table.GetLastRow(2).SetContent(info.time_);
        }
        html += recent_honors_table.ToString() + "\n\n";
    }

    // show recent achievements

    html += "\n- **近十次成就记录**：\n\n";
    if (profile.recent_achievements_.empty()) {
        html += "<p align=\"center\">您还没有获得过成就</p>\n\n";
    } else {
        html::Table recent_honors_table(1, 5);
        recent_honors_table.SetTableStyle(" align=\"center\" border=\"1px solid #ccc\" cellpadding=\"1\" cellspacing=\"1\" width=\"800\" ");
        recent_honors_table.Get(0, 0).SetContent("**序号**");
        recent_honors_table.Get(0, 1).SetContent("**游戏名称**");
        recent_honors_table.Get(0, 2).SetContent("**成就名称**");
        recent_honors_table.Get(0, 3).SetContent("**成就描述**");
        recent_honors_table.Get(0, 4).SetContent("**获得时间**");
        for (size_t i = 0; i < profile.recent_achievements_.size(); ++i) {
            const auto& info = profile.recent_achievements_[i];
            recent_honors_table.AppendRow();
            recent_honors_table.GetLastRow(0).SetContent(std::to_string(i + 1));
            recent_honors_table.GetLastRow(1).SetContent(info.game_name_);
            recent_honors_table.GetLastRow(2).SetContent(info.achievement_name_);
            if (const auto it = bot.game_handles().find(info.game_name_); it == bot.game_handles().end()) {
                recent_honors_table.GetLastRow(3).SetContent("???");
            } else {
                for (const auto& [name, description] : it->second->achievements_) {
                    if (name == info.achievement_name_) {
                        recent_honors_table.GetLastRow(3).SetContent(description);
                        break;
                    }
                }
            }
            recent_honors_table.GetLastRow(4).SetContent(info.time_);
        }
        html += recent_honors_table.ToString() + "\n\n";
    }

    // reply image

    reply() << Markdown(html, 850);

    return EC_OK;
}

static ErrCode clear_profile(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply)
{
    static constexpr const uint32_t k_required_match_num = 3;
    if (!bot.db_manager()) {
        reply() << "[错误] 重来失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    if (!bot.db_manager()->Suicide(uid, k_required_match_num)) {
        reply() << "[错误] 重来失败：清除战绩，需最近三局比赛均取得正零和分的收益";
        return EC_USER_SUICIDE_FAILED;
    }
    reply() << GetUserName(uid.GetCStr(), gid.has_value() ? gid->GetCStr() : nullptr) << "，凋零！";
    return EC_OK;
}

template <typename V>
static std::string print_score(const V& vec, const std::optional<GroupID> gid, const std::string_view& unit = "分")
{
    std::string s;
    for (uint64_t i = 0; i < vec.size(); ++i) {
        s += "\n" + std::to_string(i + 1) + "位：" +
                GetUserName(vec[i].first.GetCStr(), gid.has_value() ? gid->GetCStr() : nullptr) +
                "【" + std::to_string(vec[i].second) + " " + unit.data() + "】";
    }
    return s;
};

template <typename V>
static std::string print_score_in_table(const std::string_view& score_name, const V& vec,
        const std::optional<GroupID> gid, const std::string_view& unit = "分")
{
    html::Table table(2 + vec.size(), 3);
    table.SetTableStyle(" align=\"center\" border=\"1px solid #ccc\" cellpadding=\"1\" cellspacing=\"1\" width=\"400\"");
    table.MergeRight(0, 0, 3);
    table.Get(0, 0).SetContent(std::string("**" HTML_COLOR_FONT_HEADER(blue)) + score_name.data() +
            HTML_FONT_TAIL "排行**");
    table.Get(1, 0).SetContent("**排名**");
    table.Get(1, 1).SetContent("**用户**");
    table.Get(1, 2).SetContent(std::string("**") + score_name.data() + "**");
    for (uint64_t i = 0; i < vec.size(); ++i) {
        const auto uid_cstr = vec[i].first.GetCStr();
        table.Get(2 + i, 0).SetContent(std::to_string(i + 1) + " 位");
        table.Get(2 + i, 1).SetContent(
                "<p align=\"left\">" HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE + GetUserAvatar(uid_cstr, 30) +
                HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE + GetUserName(uid_cstr, gid.has_value() ? gid->GetCStr() : nullptr) +
                "</p>");
        table.Get(2 + i, 2).SetContent(std::to_string(vec[i].second) + " " + unit.data());
    }
    return table.ToString();
};

static ErrCode show_rank(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply)
{
    if (!bot.db_manager()) {
        reply() << "[错误] 查看失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    std::string s;
    for (const auto time_range : TimeRange::Members()) {
        const auto info = bot.db_manager()->GetRank(
                k_time_range_begin_datetimes[time_range.ToUInt()], k_time_range_end_datetimes[time_range.ToUInt()]);
        s += "\n<h2 align=\"center\">" HTML_COLOR_FONT_HEADER(blue);
        s += time_range.ToString();
        s += HTML_FONT_TAIL "赛季排行</h2>\n";
        html::Table table(1, 3);
        table.SetTableStyle(" align=\"center\" cellpadding=\"0\" cellspacing=\"0\" width=\"1250\" ");
        table.Get(0, 0).SetContent(print_score_in_table("零和总分", info.zero_sum_score_rank_, gid));
        table.Get(0, 1).SetContent(print_score_in_table("头名总分", info.top_score_rank_, gid));
        table.Get(0, 2).SetContent(print_score_in_table("游戏局数", info.match_count_rank_, gid, "场"));
        s += "\n\n" + table.ToString() + "\n\n";
    }
    reply() << Markdown(s, 1300);
    return EC_OK;
}

static ErrCode show_rank_time_range(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply,
        const TimeRange time_range)
{
    if (!bot.db_manager()) {
        reply() << "[错误] 查看失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    const auto info = bot.db_manager()->GetRank(
            k_time_range_begin_datetimes[time_range.ToUInt()], k_time_range_end_datetimes[time_range.ToUInt()]);
    reply() << "## 零和得分排行（" << time_range << "赛季）：\n" << print_score(info.zero_sum_score_rank_, gid);
    reply() << "## 头名得分排行（" << time_range << "赛季）：\n" << print_score(info.top_score_rank_, gid);
    reply() << "## 游戏局数排行（" << time_range << "赛季）：\n" << print_score(info.match_count_rank_, gid, "场");
    return EC_OK;
}

static ErrCode show_game_rank(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply,
        const std::string& game_name)
{
    if (!bot.db_manager()) {
        reply() << "[错误] 查看失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    if (bot.game_handles().find(game_name) == bot.game_handles().end()) {
        reply() << "[错误] 查看失败：未知的游戏名，请通过「#游戏列表」查看游戏名称";
        return EC_REQUEST_UNKNOWN_GAME;
    }
    std::string s;
    for (const auto time_range : TimeRange::Members()) {
        const auto info = bot.db_manager()->GetLevelScoreRank(game_name,
                k_time_range_begin_datetimes[time_range.ToUInt()], k_time_range_end_datetimes[time_range.ToUInt()]);
        s += "\n<h2 align=\"center\">" HTML_COLOR_FONT_HEADER(blue);
        s += time_range.ToString();
        s += HTML_FONT_TAIL "赛季";
        s += HTML_COLOR_FONT_HEADER(blue);
        s += game_name;
        s += HTML_FONT_TAIL "排行</h2>\n";
        html::Table table(1, 3);
        table.SetTableStyle(" align=\"center\" cellpadding=\"0\" cellspacing=\"0\" width=\"1250\" ");
        table.Get(0, 0).SetContent(print_score_in_table("等级总分", info.level_score_rank_, gid));
        table.Get(0, 1).SetContent(print_score_in_table("加权等级总分", info.weight_level_score_rank_, gid));
        table.Get(0, 2).SetContent(print_score_in_table("游戏局数", info.match_count_rank_, gid, "场"));
        s += "\n\n" + table.ToString() + "\n\n";
    }
    reply() << Markdown(s, 1300);
    return EC_OK;
}

static ErrCode show_game_rank_range_time(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply,
        const std::string& game_name, const TimeRange time_range)
{
    if (!bot.db_manager()) {
        reply() << "[错误] 查看失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    if (bot.game_handles().find(game_name) == bot.game_handles().end()) {
        reply() << "[错误] 查看失败：未知的游戏名，请通过「#游戏列表」查看游戏名称";
        return EC_REQUEST_UNKNOWN_GAME;
    }
    const auto info = bot.db_manager()->GetLevelScoreRank(game_name, k_time_range_begin_datetimes[time_range.ToUInt()],
            k_time_range_end_datetimes[time_range.ToUInt()]);
    reply() << "## 等级得分排行（" << time_range << "赛季）：\n" << print_score(info.level_score_rank_, gid);
    reply() << "## 加权等级得分排行（" << time_range << "赛季）：\n" << print_score(info.weight_level_score_rank_, gid);
    reply() << "## 游戏局数排行（" << time_range << "赛季）：\n" << print_score(info.match_count_rank_, gid, "场");
    return EC_OK;
}

static ErrCode show_honors(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply)
{
    if (!bot.db_manager()) {
        reply() << "[错误] 查看失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    html::Table table(1, 4);
    table.SetTableStyle(" align=\"center\" border=\"1px solid #ccc\" cellpadding=\"1\" cellspacing=\"1\" ");
    table.Get(0, 0).SetContent("**ID**");
    table.Get(0, 1).SetContent("**用户**");
    table.Get(0, 2).SetContent("**荣誉**");
    table.Get(0, 3).SetContent("**获得时间**");
    for (const auto& info : bot.db_manager()->GetHonors()) {
        table.AppendRow();
        table.GetLastRow(0).SetContent(std::to_string(info.id_));
        table.GetLastRow(1).SetContent(GetUserAvatar(info.uid_.GetCStr(), 25) + HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE +
                GetUserName(info.uid_.GetCStr(), gid.has_value() ? gid->GetCStr() : nullptr));
        table.GetLastRow(2).SetContent(info.description_);
        table.GetLastRow(3).SetContent(info.time_);
    }
    reply() << Markdown("## 荣誉列表\n\n" + table.ToString(), 800);
    return EC_OK;
}

const std::vector<MetaCommandGroup> meta_cmds = {
    {
        "信息查看", { // GAME INFO: can be executed at any time
            make_command("查看帮助", help<false>, VoidChecker("#帮助"),
                        OptionalDefaultChecker<BoolChecker>(false, "文字", "图片")),
            make_command("查看游戏列表", show_gamelist, VoidChecker("#游戏列表"),
                        OptionalDefaultChecker<BoolChecker>(false, "文字", "图片")),
            make_command("查看游戏规则（游戏名称可以通过「#游戏列表」查看）", show_rule, VoidChecker("#规则"),
                        AnyArg("游戏名称", "猜拳游戏"), OptionalDefaultChecker<BoolChecker>(false, "文字", "图片")),
            make_command("查看游戏成就（游戏名称可以通过「#游戏列表」查看）", show_achievement, VoidChecker("#成就"),
                        AnyArg("游戏名称", "猜拳游戏")),
            make_command("查看已加入，或该房间正在进行的比赛信息", show_match_info, VoidChecker("#游戏信息")),
            make_command("查看当前所有未开始的私密比赛", show_private_matches, VoidChecker("#私密游戏列表")),
            make_command("关于机器人", about, VoidChecker("#关于")),
        }
    },
    {
        "战绩情况", { // SCORE INFO: can be executed at any time
            make_command("查看个人战绩", show_profile, VoidChecker("#战绩"),
                    OptionalDefaultChecker<EnumChecker<TimeRange>>(TimeRange::总)),
            make_command("清除个人战绩", clear_profile, VoidChecker("#人生重来算了")),
            make_command("查看排行榜", show_rank, VoidChecker("#排行大图")),
            make_command("查看某个赛季粒度排行榜", show_rank_time_range, VoidChecker("#排行"),
                    OptionalDefaultChecker<EnumChecker<TimeRange>>(TimeRange::年)),
            make_command("查看单个游戏等级积分排行榜", show_game_rank, VoidChecker("#排行大图"),
                    AnyArg("游戏名称", "猜拳游戏")),
            make_command("查看单个游戏某个赛季粒度等级积分排行榜", show_game_rank_range_time, VoidChecker("#排行"),
                    AnyArg("游戏名称", "猜拳游戏"), OptionalDefaultChecker<EnumChecker<TimeRange>>(TimeRange::年)),
            make_command("查看所有荣誉", show_honors, VoidChecker("#荣誉列表")),
        }
    },
    {
        "新建游戏", { // NEW GAME: can only be executed by host
            make_command("在当前房间建立公开游戏，或私信 bot 以建立私密游戏（游戏名称可以通过「#游戏列表」查看）",
                        new_game, VoidChecker("#新游戏"), AnyArg("游戏名称", "猜拳游戏"),
                        OptionalDefaultChecker<BoolChecker>(false, "单机", "多人")),
            make_command("房主设置参与游戏的AI数量，使得玩家不低于一定数量（属于配置变更，会使得全部玩家退出游戏）",
                        set_bench_to, VoidChecker("#替补至"), ArithChecker<uint32_t>(2, 32, "数量")),
            make_command("房主调整分数倍率，0 代表试玩（属于配置变更，会使得全部玩家退出游戏）",
                        set_multiple, VoidChecker("#倍率"), ArithChecker<uint32_t>(0, 3, "倍率")),
            make_command("房主开始游戏", start_game, VoidChecker("#开始")),
        }
    },
    {
        "参与游戏", { // JOIN/LEAVE GAME: can only be executed by player
            make_command("加入当前房间的公开游戏", join_public, VoidChecker("#加入")),
            make_command("私信bot以加入私密游戏（可通过「#私密游戏列表」查看比赛编号）", join_private, VoidChecker("#加入"),
                        BasicChecker<MatchID>("私密比赛编号", "1")),
            make_command("退出游戏（若附带了「强制」参数，则可以在游戏进行中退出游戏，需注意退出后无法继续参与原游戏）",
                        leave, VoidChecker("#退出"),  OptionalDefaultChecker<BoolChecker>(false, "强制", "常规")),
            make_command("发起中断比赛", user_interrupt_game, VoidChecker("#中断"), OptionalDefaultChecker<BoolChecker>(false, "取消", "确定")),
        }
    }
};

static ErrCode interrupt_game(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
        MsgSenderBase& reply, const std::optional<MatchID> mid)
{
    std::shared_ptr<Match> match;
    if (mid.has_value() && !(match = bot.match_manager().GetMatch(*mid))) {
        reply() << "[错误] 中断失败：游戏ID不存在";
        return EC_MATCH_NOT_EXIST;
    } else if (!mid.has_value() && gid.has_value() && !(match = bot.match_manager().GetMatch(*gid))) {
        reply() << "[错误] 中断失败：该房间未进行游戏";
        return EC_MATCH_GROUP_NOT_IN_MATCH;
    } else if (!mid.has_value() && !gid.has_value()) {
        reply() << "[错误] 中断失败：需要在房间中使用该指令，或指定比赛ID";
        return EC_MATCH_NEED_REQUEST_PUBLIC;
    }
    match->Terminate(true /*is_force*/);
    reply() << "中断成功";
    return EC_OK;
}

static ErrCode set_game_default_multiple(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
        MsgSenderBase& reply, const std::string& gamename, const uint32_t multiple)
{
    const auto it = bot.game_handles().find(gamename);
    if (it == bot.game_handles().end()) {
        reply() << "[错误] 查看失败：未知的游戏名，请通过「#游戏列表」查看游戏名称";
        return EC_REQUEST_UNKNOWN_GAME;
    };
    it->second->multiple_ = multiple;
    reply() << "设置成功，游戏默认倍率为 " << multiple;
    return EC_OK;
}

static ErrCode show_others_profile(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
        MsgSenderBase& reply, const std::string& others_uid, const TimeRange time_range)
{
    return show_profile(bot, others_uid, gid, reply, time_range);
}

static ErrCode clear_others_profile(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
        MsgSenderBase& reply, const std::string& others_uid, const std::string& reason)
{
    if (!bot.db_manager()) {
        reply() << "[错误] 清除失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    if (!bot.db_manager()->Suicide(UserID{others_uid}, 0)) {
        reply() << "[错误] 清除失败：未知原因";
        return EC_USER_SUICIDE_FAILED;
    }
    MsgSender{UserID{others_uid}}() << "非常抱歉，您的战绩已被强制清空，理由为「" << reason << "」\n如有疑问，请联系管理员";
    reply() << "战绩删除成功，且已通知该玩家！";
    return EC_OK;
}

static ErrCode set_option(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
        MsgSenderBase& reply, const std::vector<std::string>& option_args)
{
    if (option_args.empty()) {
        reply() << "[错误] 配置参数为空";
        return EC_INVALID_ARGUMENT;
    }
    MsgReader reader(option_args);
    if (!bot.option().SetOption(reader)) {
        reply() << "[错误] 设置配置项失败，请检查配置项是否存在";
        return EC_INVALID_ARGUMENT;
    }
    reply() << "设置成功";
    return EC_OK;
}

static ErrCode read_all_options(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
        MsgSenderBase& reply, const bool text_mode)
{
    std::string outstr = "### 全局配置选项";
    const auto option_size = bot.option().Count();
    for (uint64_t i = 0; i < option_size; ++i) {
        outstr += "\n" + std::to_string(i) + ". " + (text_mode ? bot.option().Info(i) : bot.option().ColoredInfo(i));
    }
    if (text_mode) {
        reply() << outstr;
    } else {
        reply() << Markdown(outstr);
    }
    return EC_OK;
}

static ErrCode add_honor(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply,
        const std::string& honor_uid, const std::string honor_desc)
{
    if (!bot.db_manager()) {
        reply() << "[错误] 添加失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    if (!bot.db_manager()->AddHonor(honor_uid, honor_desc)) {
        reply() << "[错误] 添加失败：未知原因";
        return EC_HONOR_ADD_FAILED;
    }
    reply() << "添加荣誉成功，恭喜" << At(UserID{honor_uid}) << "荣获「" << honor_desc << "」";
    return EC_OK;
}

static ErrCode delete_honor(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgSenderBase& reply,
        const int32_t id)
{
    if (!bot.db_manager()) {
        reply() << "[错误] 删除失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    if (!bot.db_manager()->DeleteHonor(id)) {
        reply() << "[错误] 删除失败：未知原因";
        return EC_HONOR_ADD_FAILED;
    }
    reply() << "删除荣誉成功";
    return EC_OK;
}

const std::vector<MetaCommandGroup> admin_cmds = {
    {
        "信息查看", {
            make_command("查看帮助", help<true>, VoidChecker("%帮助"),
                        OptionalDefaultChecker<BoolChecker>(false, "文字", "图片")),
        }
    },
    {
        "管理操作", {
            make_command("强制中断比赛", interrupt_game, VoidChecker("%中断"),
                        OptionalChecker<BasicChecker<MatchID>>("私密比赛编号")),
            make_command("设置游戏默认属性", set_game_default_multiple, VoidChecker("%默认倍率"),
                        AnyArg("游戏名称", "猜拳游戏"), ArithChecker<uint32_t>(0, 3, "倍率")),
            make_command("查看他人战绩", show_others_profile, VoidChecker("%战绩"), AnyArg("用户 ID", "123456789"),
                        OptionalDefaultChecker<EnumChecker<TimeRange>>(TimeRange::总)),
            make_command("清除他人战绩，并通知其具体理由", clear_others_profile, VoidChecker("%清除战绩"),
                        AnyArg("用户 ID", "123456789"), AnyArg("理由", "恶意刷分")),
            make_command("查看所有支持的配置项", read_all_options, VoidChecker("%配置列表"),
                        OptionalDefaultChecker<BoolChecker>(false, "文字", "图片")),
            make_command("设置配置项（可通过「%配置列表」查看所有支持的配置）", set_option, VoidChecker("%配置"),
                        RepeatableChecker<AnyArg>("配置参数", "配置参数")),
        }
    },
    {
        "荣誉操作", {
            make_command("新增荣誉", add_honor, VoidChecker("%荣誉"), VoidChecker("新增"),
                        AnyArg("用户 ID", "123456789"), AnyArg("荣誉描述", "2022 年度某游戏年赛冠军")),
            make_command("新增荣誉", delete_honor, VoidChecker("%荣誉"), VoidChecker("删除"),
                        ArithChecker<int32_t>(0, INT32_MAX, "编号")),
        }
    },
};
