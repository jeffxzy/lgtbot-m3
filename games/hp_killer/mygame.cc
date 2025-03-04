// Copyright (c) 2018-present, Chang Liu <github.com/slontia>. All rights reserved.
//
// This source code is licensed under LGPLv2 (found in the LICENSE file).

#include <array>
#include <functional>
#include <memory>
#include <set>
#include <variant>
#include <random>
#include <algorithm>

#include "game_framework/game_main.h"
#include "game_framework/game_stage.h"
#include "game_framework/game_options.h"
#include "game_framework/game_achievements.h"
#include "utility/msg_checker.h"
#include "utility/html.h"
#include "occupation.h"

const std::string k_game_name = "HP杀"; // the game name which should be unique among all the games
const uint64_t k_max_player = 9; // 0 indicates no max-player limits
const uint64_t k_multiple = 0; // the default score multiple for the game, 0 for a testing game, 1 for a formal game, 2 or 3 for a long formal game
const std::string k_developer = "森高";
const std::string k_description = "通过对其他玩家造成伤害，杀掉隐藏在玩家中的杀手的游戏";

std::string GameOption::StatusInfo() const
{
    return "共 " + std::to_string(GET_VALUE(回合数)) + " 回合，每回合超时时间 " + std::to_string(GET_VALUE(时限)) + " 秒";
}

static const std::vector<Occupation>& GetOccupationList(const GameOption& option)
{
    return option.PlayerNum() == 5 ? GET_OPTION_VALUE(option, 五人身份) :
           option.PlayerNum() == 6 ? GET_OPTION_VALUE(option, 六人身份) :
           option.PlayerNum() == 7 ? GET_OPTION_VALUE(option, 七人身份) :
           option.PlayerNum() == 8 ? GET_OPTION_VALUE(option, 八人身份) :
           option.PlayerNum() == 9 ? GET_OPTION_VALUE(option, 九人身份) : (assert(false), GET_OPTION_VALUE(option, 五人身份));
}

static std::vector<Occupation>& GetOccupationList(GameOption& option)
{
    return const_cast<std::vector<Occupation>&>(GetOccupationList(const_cast<const GameOption&>(option)));
}

bool GameOption::ToValid(MsgSenderBase& reply)
{
    if (PlayerNum() < 5) {
        reply() << "该游戏至少 5 人参加，当前玩家数为 " << PlayerNum();
        return false;
    }
    const auto player_num_matched = [player_num = PlayerNum()](const std::vector<Occupation>& occupation_list)
        {
            return std::ranges::count_if(occupation_list, [](const Occupation occupation) { return occupation != Occupation::人偶; }) == player_num;
        };
    if (!GET_VALUE(身份列表).empty() && !player_num_matched(GET_VALUE(身份列表))) {
        reply() << "玩家人数和身份列表长度不匹配";
        return false;
    }
    auto& occupation_list = GetOccupationList(*this);
    if (!player_num_matched(occupation_list)) {
        reply() << "[警告] 身份列表配置项身份个数与参加人数不符，将按照默认配置进行游戏";
        occupation_list.clear();
    } else if (std::ranges::count(occupation_list, Occupation::杀手) != 1) {
        reply() << "[警告] 身份列表中杀手个数不为 1，将按照默认配置进行游戏";
        occupation_list.clear();
    } else if (std::ranges::count(occupation_list, Occupation::替身) > 1) {
        reply() << "[警告] 身份列表中替身个数大于 1，将按照默认配置进行游戏";
        occupation_list.clear();
    } else if (std::ranges::count(occupation_list, Occupation::内奸) > 1) {
        reply() << "[警告] 身份列表中内奸个数大于 1，将按照默认配置进行游戏";
        occupation_list.clear();
    }
    return true;
}

uint64_t GameOption::BestPlayerNum() const { return 8; }

// ========== PLAYER INFO ==========

struct Token
{
    char ToChar() const { return 'A' + id_; }

    friend std::istream& operator>>(std::istream& is, Token& token)
    {
        std::string s;
        if (!(is >> s)) {
            // already failed, do nothing
        } else if (s.size() != 1) {
            is.setstate(std::ios_base::failbit);
        } else if (std::islower(s[0])) {
            token.id_ = s[0] - 'a';
        } else if (std::isupper(s[0])) {
            token.id_ = s[0] - 'A';
        } else {
            is.setstate(std::ios_base::failbit);
        }
        return is;
    }

    friend std::ostream& operator<<(std::ostream& os, const Token& token)
    {
        return os << token.ToChar();
    }

    auto operator<=>(const Token&) const = default;

    uint32_t id_;
};

struct HurtAction
{
    std::string ToString() const { return std::string("攻击 ") + token_.ToChar() + " " + std::to_string(hp_); }

    Token token_;
    int32_t hp_;
};

struct CureAction
{
    std::string ToString() const { return std::string("治愈 ") + token_.ToChar() + " " + std::to_string(hp_); }

    Token token_;
    int32_t hp_;
};

struct BlockHurtAction
{
    std::string ToString() const { return std::string("挡刀") + (token_.has_value() ? std::string(" ") + token_->ToChar() : "杀手"); }

    std::optional<Token> token_;
};

struct DetectAction
{
    std::string ToString() const { return std::string("侦查 ") + token_.ToChar(); }

    Token token_;
};

struct PassAction
{
    std::string ToString() const { return "pass"; }
};

struct ExocrismAction
{
    std::string ToString() const { return std::string("除灵 ") + token_.ToChar(); }

    Token token_;
};

struct ShieldAntiAction
{
    std::string ToString() const { return std::string("盾反 ") + std::to_string(hp_); }

    int32_t hp_;
};

using ActionVariant = std::variant<HurtAction, CureAction, BlockHurtAction, DetectAction, PassAction, ExocrismAction, ShieldAntiAction>;

class RoleManager;

static constexpr const int32_t k_heavy_hurt_hp = 25;
static constexpr const int32_t k_heavy_cure_hp = 15;
static constexpr const int32_t k_normal_hurt_hp = 15;
static constexpr const int32_t k_normal_cure_hp = 10;
static constexpr const int32_t k_civilian_dead_threshold = 2;
static constexpr const int32_t k_civilian_team_dead_threshold = 3;

struct RoleStatus
{
    int32_t hp_;
    ActionVariant action_;
};

struct RoleOption
{
    int32_t hp_;
    int32_t cure_count_; // -1 means no limit
};

class RoleBase
{
  protected:
    RoleBase(const std::optional<PlayerID> pid, const Token token, const Occupation occupation, const Team team, const RoleOption& option, RoleManager& role_manager)
        : pid_(pid)
        , token_(token)
        , occupation_(occupation)
        , role_manager_(role_manager)
        , team_(team)
        , hp_(option.hp_)
        , can_act_(true)
        , disable_act_when_refresh_(false)
        , is_alive_(true)
        , is_allowed_heavy_hurt_cure_(false)
        , is_winner_(true)
        , remain_cure_(option.cure_count_)
        , cur_action_(PassAction{})
    {
    }

    RoleBase(const RoleBase&) = delete;
    RoleBase(RoleBase&&) = delete;

  public:
    virtual bool Act(const HurtAction& action, MsgSenderBase& reply);

    virtual bool Act(const CureAction& action, MsgSenderBase& reply);

    virtual bool Act(const BlockHurtAction& action, MsgSenderBase& reply)
    {
        reply() << "侦查失败：您无法执行该类型行动";
        return false;
    }

    virtual bool Act(const DetectAction& action, MsgSenderBase& reply)
    {
        reply() << "侦查失败：您无法执行该类型行动";
        return false;
    }

    virtual bool Act(const ExocrismAction& action, MsgSenderBase& reply)
    {
        reply() << "除灵失败：您无法执行该类型行动";
        return false;
    }

    virtual bool Act(const PassAction& action, MsgSenderBase& reply)
    {
        reply() << "您本回合决定不行动";
        cur_action_ = action;
        return true;
    }

    virtual bool Act(const ShieldAntiAction& action, MsgSenderBase& reply);

    // return true if dead in this round
    virtual bool Refresh()
    {
        if (!can_act_ && hp_ <= 0) {
            // both action and HP unchange os we need not push to |history_status_|
            return false;
        }
        if (disable_act_when_refresh_) {
            DisableAct();
            disable_act_when_refresh_ = false;
        }
        if (std::get_if<CureAction>(&cur_action_) && remain_cure_ > 0) {
            --remain_cure_;
        }
        history_status_.emplace_back(hp_, cur_action_);
        cur_action_ = PassAction{}; // change to default action
        if (hp_ <= 0 && is_alive_) {
            is_alive_ = false;
            return true;
        }
        return false;
    }

    void AddHp(const int32_t addition_hp) { hp_ += addition_hp; }
    void SetHp(const int32_t hp) { hp_ = hp; }
    void SetAllowHeavyHurtCure(const bool allow) { is_allowed_heavy_hurt_cure_ = allow; }
    void SetWinner(const bool is_winner) { is_winner_ = is_winner; }
    void DisableAct() { can_act_ = false; }
    void DisableActWhenRefresh() { disable_act_when_refresh_ = true; }

    std::optional<PlayerID> PlayerId() const { return pid_; }
    Token GetToken() const { return token_; }
    Occupation GetOccupation() const { return occupation_; }
    Team GetTeam() const { return team_; }
    int32_t GetHp() const { return hp_; }
    bool CanAct() const { return can_act_; }
    bool IsAlive() const { return is_alive_; }
    bool IsWinner() const { return is_winner_; }
    int32_t RemainCure() const { return remain_cure_; }
    const ActionVariant& CurAction() const { return cur_action_; }
    const RoleStatus* GetHistoryStatus(const uint32_t idx) const
    {
        return idx < history_status_.size() ? &history_status_[idx] : nullptr;
    }

  protected:
    const std::optional<PlayerID> pid_;
    const Token token_;
    const Occupation occupation_;
    RoleManager& role_manager_;
    Team team_;
    int32_t hp_;
    bool can_act_;
    bool disable_act_when_refresh_;
    bool is_alive_;
    bool is_allowed_heavy_hurt_cure_;
    bool is_winner_;
    int32_t remain_cure_;
    ActionVariant cur_action_;
    std::vector<RoleStatus> history_status_;
};

class RoleManager
{
  public:
    using RoleVec = std::vector<std::unique_ptr<RoleBase>>;

    RoleManager(RoleVec roles) : roles_(std::move(roles))
    {
    }

    RoleBase& GetRole(const Token token) { return *roles_[token.id_]; }
    const RoleBase& GetRole(const Token token) const { return *roles_[token.id_]; }

    RoleBase& GetRole(const PlayerID pid)
    {
        return const_cast<RoleBase&>(const_cast<const RoleManager*>(this)->GetRole(pid));
    }

    const RoleBase& GetRole(const PlayerID pid) const
    {
        for (auto& role : roles_) {
            if (role->PlayerId() == pid) {
                return *role;
            }
        }
        assert(false);
    }

    RoleBase* GetRole(const Occupation occupation)
    {
        return const_cast<RoleBase*>(const_cast<const RoleManager*>(this)->GetRole(occupation));
    }

    const RoleBase* GetRole(const Occupation occupation) const
    {
        for (auto& role : roles_) {
            if (role->GetOccupation() == occupation) {
                return role.get();
            }
        }
        return nullptr;
    }


    bool IsValid(const Token token) { return token.id_ < roles_.size(); }

    template <typename Fn>
    void Foreach(const Fn& fn) { std::ranges::for_each(roles_, [&fn](const auto& role_p) { fn(*role_p); }); }
    template <typename Fn>
    void Foreach(const Fn& fn) const { std::ranges::for_each(roles_, [&fn](const auto& role_p) { fn(*role_p); }); }

    uint32_t Size() const { return roles_.size(); }

  private:
    RoleVec roles_;
};

bool RoleBase::Act(const HurtAction& action, MsgSenderBase& reply)
{
    auto& target = role_manager_.GetRole(action.token_);
    if (!target.is_alive_) {
        reply() << "攻击失败：该角色已经死亡";
        return false;
    }
    if (is_allowed_heavy_hurt_cure_ && action.hp_ != k_normal_hurt_hp && action.hp_ != k_heavy_hurt_hp) {
        reply() << "攻击失败：您只能造成 " << k_normal_hurt_hp << " 或 " << k_heavy_hurt_hp << " 点伤害";
        return false;
    }
    if (!is_allowed_heavy_hurt_cure_ && action.hp_ != k_normal_hurt_hp) {
        reply() << "攻击失败：您只能造成 " << k_normal_hurt_hp << " 点伤害";
        return false;
    }
    reply() << "您本回合对角色 " << action.token_.ToChar() << " 造成了 " << action.hp_ << " 点伤害";
    cur_action_ = action;
    return true;
}

bool RoleBase::Act(const CureAction& action, MsgSenderBase& reply)
{
    auto& target = role_manager_.GetRole(action.token_);
    if (!target.is_alive_) {
        reply() << "治愈失败：该角色已经死亡";
        return false;
    }
    if (remain_cure_ == 0) {
        reply() << "治愈失败：您已经没有治愈的机会了";
        return false;
    }
    if (is_allowed_heavy_hurt_cure_ && action.hp_ != k_normal_cure_hp && action.hp_ != k_heavy_cure_hp) {
        reply() << "治愈失败：您只能治愈 " << k_normal_cure_hp << " 或 " << k_heavy_cure_hp << " 点血量";
        return false;
    }
    if (!is_allowed_heavy_hurt_cure_ && action.hp_ != k_normal_cure_hp) {
        reply() << "治愈失败：您只能治愈 " << k_normal_cure_hp << " 点血量";
        return false;
    }
    auto sender = reply();
    sender << "您本回合对角色 " << action.token_.ToChar() << " 治愈了 " << action.hp_ << " 点血量，您";
    if (remain_cure_ > 0) {
        sender << "还可治愈 " << (remain_cure_ - 1) << " 次";
    } else {
        sender << "没有治愈次数的限制";
    }
    cur_action_ = action;
    return true;
}

bool RoleBase::Act(const ShieldAntiAction& action, MsgSenderBase& reply)
{
    if (team_ != Team::平民) {
        reply() << "盾反失败：只有平民阵营的玩家可以使用该技能";
        return false;
    }
    if (occupation_ != Occupation::守卫 && !history_status_.empty() && std::get_if<ShieldAntiAction>(&history_status_.back().action_)) {
        reply() << "盾反失败：您无法连续两回合进行盾反";
        return false;
    }
    if (const auto guard_role = role_manager_.GetRole(Occupation::守卫); guard_role == nullptr || !guard_role->is_alive_) {
        reply() << "盾反失败：只有守卫存活时才可使用盾反";
        return false;
    }
    reply() << "您选择盾反成功";
    cur_action_ = action;
    return true;
}

class KillerRole : public RoleBase
{
  public:
    KillerRole(const uint64_t pid, const Token token, const RoleOption& option, RoleManager& role_manager)
        : RoleBase(pid, token, Occupation::杀手, Team::杀手, option, role_manager)
    {
        is_allowed_heavy_hurt_cure_ = true;
    }
};

class BodyDoubleRole : public RoleBase
{
  public:
    BodyDoubleRole(const uint64_t pid, const Token token, const RoleOption& option, RoleManager& role_manager)
        : RoleBase(pid, token, Occupation::替身, Team::杀手, option, role_manager)
    {
    }

    virtual bool Act(const BlockHurtAction& action, MsgSenderBase& reply) override
    {
        reply() << "请做好觉悟，本回合对该角色造成的全部伤害将转移到您身上";
        cur_action_ = action;
        return true;
    }
};

class GhostRole : public RoleBase
{
  public:
    GhostRole(const uint64_t pid, const Token token, const RoleOption& option, RoleManager& role_manager)
        : RoleBase(pid, token, Occupation::恶灵, Team::杀手, option, role_manager)
    {
    }
};

class AssassinRole : public RoleBase
{
  public:
    AssassinRole(const uint64_t pid, const Token token, RoleOption option, RoleManager& role_manager)
        : RoleBase(pid, token, Occupation::刺客, Team::杀手, option, role_manager)
    {
    }

    virtual bool Act(const HurtAction& action, MsgSenderBase& reply) override
    {
        auto& target = role_manager_.GetRole(action.token_);
        if (!target.IsAlive()) {
            reply() << "攻击失败：该角色已经死亡";
            return false;
        }
        if (action.hp_ != 0 && action.hp_ != 5 && action.hp_ != 10 && action.hp_ != 15) {
            reply() << "攻击失败：您只能造成 0 / 5 / 10 / 15 点伤害";
            return false;
        }
        reply() << "您本回合对角色 " << action.token_.ToChar() << " 造成了 " << action.hp_ << " 点伤害";
        cur_action_ = action;
        return true;
    }
};

class CivilianRole : public RoleBase
{
  public:
    CivilianRole(const uint64_t pid, const Token token, const RoleOption& option, RoleManager& role_manager)
        : RoleBase(pid, token, Occupation::平民, Team::平民, option, role_manager)
    {
    }
};

class GoddessRole : public RoleBase
{
  public:
    GoddessRole(const uint64_t pid, const Token token, RoleOption option, RoleManager& role_manager)
        : RoleBase(pid, token, Occupation::圣女, Team::平民, (option.cure_count_ = -1, option), role_manager)
    {
    }

    virtual bool Act(const HurtAction& action, MsgSenderBase& reply) override
    {
        if (!history_status_.empty() && std::get_if<HurtAction>(&history_status_.back().action_)) {
            reply() << "攻击失败：您无法连续两回合进行攻击";
            return false;
        }
        return ::RoleBase::Act(action, reply);
    }
};

class DetectiveRole : public RoleBase
{
  public:
    DetectiveRole(const uint64_t pid, const Token token, const RoleOption& option, RoleManager& role_manager)
        : RoleBase(pid, token, Occupation::侦探, Team::平民, option, role_manager)
    {
    }

  public:
    virtual bool Act(const DetectAction& action, MsgSenderBase& reply) override
    {
        if (history_status_.empty()) {
            reply() << "侦查失败：首回合无法侦查";
            return false;
        }
        if (!history_status_.empty() && std::get_if<DetectAction>(&history_status_.back().action_)) {
            reply() << "侦查失败：您无法连续两回合进行侦查";
            return false;
        }
        reply() << "您选择侦查角色 " << action.token_.ToChar() << "，本回合结束后将私信您他的行动";
        cur_action_ = action;
        return true;
    }
};

class SorcererRole : public RoleBase
{
  public:
    SorcererRole(const uint64_t pid, const Token token, const RoleOption& option, RoleManager& role_manager)
        : RoleBase(pid, token, Occupation::灵媒, Team::平民, option, role_manager)
    {
    }

    virtual bool Act(const ExocrismAction& action, MsgSenderBase& reply) override
    {
        reply() << "您选择驱灵角色 " << action.token_.ToChar() << "，本回合结束后将私信您他是否为恶灵，以及是否驱灵成功";
        cur_action_ = action;
        return true;
    }
};

class GuardRole : public RoleBase
{
  public:
    GuardRole(const uint64_t pid, const Token token, const RoleOption& option, RoleManager& role_manager)
        : RoleBase(pid, token, Occupation::守卫, Team::平民, option, role_manager)
    {
    }

};

class TraitorRole : public RoleBase
{
  public:
    TraitorRole(const uint64_t pid, const Token token, const RoleOption& option, RoleManager& role_manager)
        : RoleBase(pid, token, Occupation::内奸, Team::特殊, option, role_manager)
    {
    }
};

class PuppetRole : public RoleBase
{
  public:
    PuppetRole(const Token token, const RoleOption& option, RoleManager& role_manager)
        : RoleBase(std::nullopt, token, Occupation::人偶, Team::特殊, option, role_manager)
    {
    }
};

enum class RoundResult { KILLER_WIN, CIVILIAN_WIN, DRAW, CONTINUE };

// ========== GAME STAGES ==========

class MainStage : public MainGameStage<>
{
  public:
    using RoleMaker = std::unique_ptr<RoleBase>(*)(uint64_t, Token, const RoleOption&, RoleManager&);

    MainStage(const GameOption& option, MatchBase& match)
        : GameStage(option, match,
                MakeStageCommand("查看当前游戏进展情况", &MainStage::Status_, VoidChecker("赛况")),
                MakeStageCommand("攻击某名角色", &MainStage::Hurt_, VoidChecker("攻击"),
                    BasicChecker<Token>("角色代号", "A"), ArithChecker<int32_t>(0, 25, "血量")),
                MakeStageCommand("治愈某名角色", &MainStage::Cure_, VoidChecker("治愈"),
                    BasicChecker<Token>("角色代号", "A"),
                    BoolChecker(std::to_string(k_heavy_cure_hp), std::to_string(k_normal_cure_hp))),
                MakeStageCommand("检查某名角色上一回合行动", &MainStage::Detect_, VoidChecker("侦查"),
                    BasicChecker<Token>("角色代号", "A")),
                MakeStageCommand("替某名角色承担本回合伤害", &MainStage::BlockHurt_, VoidChecker("挡刀"),
                    OptionalChecker<BasicChecker<Token>>("角色代号（若为空，则为杀手代号）", "A")),
                MakeStageCommand("检查某名角色是否为恶灵", &MainStage::Exocrism_, VoidChecker("驱灵"),
                    BasicChecker<Token>("角色代号", "A")),
                MakeStageCommand("使用盾反", &MainStage::ShieldAnti_, VoidChecker("盾反"),
                    ArithChecker<int32_t>(-1000, 1000, "预测下一回合血量")),
                MakeStageCommand("跳过本回合行动", &MainStage::Pass_, VoidChecker("pass")))
        , role_manager_(GET_OPTION_VALUE(option, 身份列表).empty()
                ? GetRoleVec_(option, DefaultRoleOption_(option), role_manager_)
                : LoadRoleVec_(GET_OPTION_VALUE(option, 身份列表), DefaultRoleOption_(option), role_manager_))
        , k_image_width_((k_avatar_width_ + k_cellspacing_ + k_cellpadding_) * role_manager_.Size() + 150)
        , role_info_(RoleInfo_())
        , round_(1)
        , last_round_civilian_lost_(false)
        , last_round_killer_lost_(false)
        , last_round_traitor_lost_(false)
    {
    }

    virtual void OnStageBegin() override {
        Boardcast() << "游戏开始，将私信各位玩家角色代号及职业\n\n第 1 回合开始，请私信裁判行动";
        role_manager_.Foreach([&](const auto& role)
            {
                if (role.PlayerId().has_value()) {
                    Tell(*role.PlayerId()) << PrivateRoleInfo_(role);
                }
            });
        table_html_ = Html_(false);
        StartTimer(GET_OPTION_VALUE(option(), 时限));
        Boardcast() << Markdown("## 第 1 回合\n\n" + table_html_, k_image_width_);
    }

    virtual CheckoutErrCode OnTimeout() override
    {
        HookUnreadyPlayers();
        return CheckoutErrCode::Condition(OnRoundFinish_(), StageErrCode::CHECKOUT, StageErrCode::CONTINUE);
    }

    virtual AtomReqErrCode OnComputerAct(const PlayerID pid, MsgSenderBase& reply) override
    {
        if (IsReady(pid)) {
            return StageErrCode::OK;
        }
        if (rand() % 2) {
            Hurt_(pid, false, reply, Token{static_cast<uint32_t>(rand() % option().PlayerNum())}, 15); // randomly hurt one role
        } else {
            Cure_(pid, false, reply, Token{static_cast<uint32_t>(rand() % option().PlayerNum())}, false); // randomly hurt one role
        }
        return StageErrCode::READY;
    }

    virtual void OnAllPlayerReady() override
    {
        OnRoundFinish_();
    }

    virtual int64_t PlayerScore(const PlayerID pid) const override { return role_manager_.GetRole(pid).IsWinner(); }

  private:
    static RoleOption DefaultRoleOption_(const GameOption& option)
    {
        return RoleOption {
            .hp_ = GET_OPTION_VALUE(option, 血量),
            .cure_count_ = GET_OPTION_VALUE(option, 治愈次数),
        };
    }

    std::string PrivateRoleInfo_(const RoleBase& role)
    {
        std::string s = std::string("您的代号是 ") + role.GetToken().ToChar() + "，职业是「" + role.GetOccupation().ToString() + "」";
        if (role.GetOccupation() == Occupation::杀手) {
            s += "，平民阵营的代号包括";
            role_manager_.Foreach([&](const auto& role)
                {
                    if (role.GetTeam() == Team::平民) {
                        s += ' ';
                        s += role.GetToken().ToChar();
                    }
                });
        } else if (role.GetOccupation() == Occupation::替身 || role.GetOccupation() == Occupation::刺客) {
            if (const auto killer = role_manager_.GetRole(Occupation::杀手)) {
                s += "，杀手的代号是 ";
                s += killer->GetToken().ToChar();
            }
        } else if (role.GetOccupation() == Occupation::恶灵) {
            s += "，杀手和灵媒师的代号在";
            role_manager_.Foreach([&](const auto& role)
                {
                    if (role.GetOccupation() == Occupation::杀手 || role.GetOccupation() == Occupation::灵媒) {
                        s += ' ';
                        s += role.GetToken().ToChar();
                    }
                });
            s += " 之间";
        } else if (role.GetOccupation() == Occupation::内奸) {
            s += "，杀手和平民的代号在";
            role_manager_.Foreach([&](const auto& role)
                {
                    if (role.GetOccupation() == Occupation::杀手 || role.GetOccupation() == Occupation::平民) {
                        s += ' ';
                        s += role.GetToken().ToChar();
                    }
                });
            s += " 之间";
        }
        return s;
    }

    void SettlementAction_(MsgSenderBase::MsgSenderGuard& sender)
    {
        // 禁止有多个替身,因为会循环挡刀
        RoleBase* const hurt_blocker = role_manager_.GetRole(Occupation::替身);
        const BlockHurtAction* const block_hurt_action =
            hurt_blocker ? std::get_if<BlockHurtAction>(&hurt_blocker->CurAction()) : nullptr;
        const auto is_blocked_hurt = [&](const RoleBase& role)
            {
                return block_hurt_action &&
                    ((!block_hurt_action->token_.has_value() && role.GetOccupation() == Occupation::杀手) ||
                     (block_hurt_action->token_.has_value() && role.GetToken() == *block_hurt_action->token_));
            };

        const auto is_avoid_hurt = [&](const RoleBase& hurter_role, const RoleBase& hurted_role)
            {
                return hurter_role.GetOccupation() == Occupation::圣女 && hurted_role.GetTeam() == Team::平民;
            };

        role_manager_.Foreach([&](auto& role)
            {
                if (const auto action = std::get_if<HurtAction>(&role.CurAction())) {
                    auto& hurted_role = role_manager_.GetRole(action->token_);
                    if (is_avoid_hurt(role, hurted_role)) {
                        // do nothing
                    } else if (is_blocked_hurt(hurted_role)) {
                        hurt_blocker->AddHp(-action->hp_);
                    } else {
                        hurted_role.AddHp(-action->hp_);
                    }
                } else if (const auto action = std::get_if<CureAction>(&role.CurAction())) {
                    role_manager_.GetRole(action->token_).AddHp(action->hp_);
                } else if (const auto action = std::get_if<DetectAction>(&role.CurAction())) {
                    auto& detected_role = role_manager_.GetRole(action->token_);
                    assert(role.PlayerId().has_value());
                    auto sender = Tell(*role.PlayerId());
                    sender << "上一回合角色 " << action->token_.ToChar() << " 的行动是「";
                    if (const auto detect_action = std::get_if<HurtAction>(&detected_role.CurAction())) {
                        sender << "攻击 " << detect_action->token_.ToChar();
                    } else if (const auto detect_action = std::get_if<CureAction>(&detected_role.CurAction())) {
                        sender << "治愈 " << detect_action->token_.ToChar();
                    } else {
                        sender << "其它";
                    }
                    sender << "」";
                    if (!detected_role.IsAlive() && (std::get_if<HurtAction>(&detected_role.CurAction()) ||
                                std::get_if<CureAction>(&detected_role.CurAction()))) {
                        DisableAct_(detected_role, true);
                        sender << "，而且你完成了除灵，他已经失去行动能力了！";
                    }
                } else if (const auto action = std::get_if<ExocrismAction>(&role.CurAction())) {
                    auto& exocrism_role = role_manager_.GetRole(action->token_);
                    assert(role.PlayerId().has_value());
                    auto sender = Tell(*role.PlayerId());
                    if (exocrism_role.GetOccupation() != Occupation::恶灵) {
                        sender << "很遗憾，" << action->token_.ToChar() << " 不是恶灵";
                    } else {
                        sender << action->token_.ToChar() << " 确实是恶灵！";
                        const auto hurt_action = std::get_if<HurtAction>(&exocrism_role.CurAction());
                        if (!exocrism_role.CanAct()) {
                            sender << "但是他早就已经失去行动能力了";
                        } else if (!exocrism_role.IsAlive() || (hurt_action && hurt_action->token_ == role.GetToken())) {
                            sender << "驱灵成功，他已经失去行动能力了！";
                            DisableAct_(exocrism_role, true);
                        } else {
                            sender << "但是并没有驱灵成功，他仍可以继续行动";
                        }
                    }
                }
            });
        bool has_shield_anti_succ = false;
        role_manager_.Foreach([&](auto& role)
            {
                if (const auto action = std::get_if<ShieldAntiAction>(&role.CurAction())) {
                    if (is_blocked_hurt(role) || role.GetHp() != action->hp_) {
                        return;
                    }
                    has_shield_anti_succ = true;
                    role_manager_.Foreach([&](auto& hurter_role)
                        {
                            if (const auto hurt_action = std::get_if<HurtAction>(&hurter_role.CurAction());
                                    hurt_action != nullptr && hurt_action->token_ == role.GetToken() && !is_avoid_hurt(hurter_role, role)) {
                                role.AddHp(hurt_action->hp_);
                                hurter_role.AddHp(-hurt_action->hp_);
                            }
                        });
                }
            });
        if (has_shield_anti_succ) {
            role_manager_.Foreach([&](auto& role)
                {
                    if (role.GetOccupation() == Occupation::守卫) {
                        assert(role.PlayerId().has_value());
                        Tell(*role.PlayerId()) << "请注意，上一回合有人盾反成功";
                    }
                });
        }

    }

    void RefreshRoles_(MsgSenderBase::MsgSenderGuard& sender)
    {
        role_manager_.Foreach([&](auto& role)
            {
                if (role.Refresh()) {
                    sender << "\n角色 " << role.GetToken().ToChar() << " 死亡，";
                    if (role.PlayerId().has_value()) {
                        sender << "他的「中之人」是" << At(*role.PlayerId());
                    } else {
                        sender << "他是 NPC，没有「中之人」";
                    }
                    if (role.GetOccupation() != Occupation::恶灵) {
                        DisableAct_(role);
                    }
                    if (role.GetOccupation() == Occupation::杀手) {
                        role_manager_.Foreach([&](auto& other_role)
                            {
                                if (other_role.GetOccupation() == Occupation::内奸) {
                                    other_role.SetAllowHeavyHurtCure(true);
                                    assert(other_role.PlayerId().has_value());
                                    Tell(*other_role.PlayerId()) << "杀手已经死亡，您获得了造成 " << k_heavy_hurt_hp << " 点伤害和治愈 " << k_heavy_cure_hp << " 点 HP 的权利";
                                }
                            });
                    }
                }
            });
        sender << "\n\n";
    }

    bool CheckTeamsLost_(MsgSenderBase::MsgSenderGuard& sender)
    {
        bool killer_dead = true;
        bool traitor_dead = true;
        uint32_t civilian_dead_count = 0;
        uint32_t civilian_team_dead_count = 0;
        role_manager_.Foreach([&](const auto& role)
            {
                if (role.IsAlive()) {
                    if (role.GetOccupation() == Occupation::内奸) {
                        traitor_dead = false;
                    }
                    if (role.GetOccupation() == Occupation::杀手) {
                        killer_dead = false;
                    }
                    return;
                } else if (role.GetTeam() == Team::平民) {
                    ++civilian_team_dead_count;
                    if (role.GetOccupation() == Occupation::平民) {
                        ++civilian_dead_count;
                    }
                }
            });
        bool civilian_lost = civilian_dead_count >= k_civilian_dead_threshold ||
            civilian_team_dead_count >= k_civilian_team_dead_threshold;
        bool killer_lost = killer_dead;
        bool traitor_lost = traitor_dead;

        switch (!civilian_lost + !killer_lost + !traitor_lost) {
        case 0: // multiple teams lost at the same time
            sender << "游戏结束，多个阵营的失败条件同时满足，此时根据优先级，判定";
            if (traitor_lost && !last_round_traitor_lost_) { // traitor lost at this round
                traitor_lost = false;
                sender << "内奸";
            } else if (killer_lost && !last_round_killer_lost_) { // killer lost at this round
                killer_lost = false;
                sender << "杀手阵营";
            } else {
                assert(false);
            }
            sender << "胜利";
            break;

        case 1: // only one team wins
            sender << "游戏结束，";
            if (!traitor_lost) {
                sender << "内奸";
            } else if (!killer_lost) {
                sender << "杀手阵营";
            } else {
                sender << "平民阵营";
            }
            sender << "胜利";
            break;

        default:
            if ((++round_) > GET_OPTION_VALUE(option(), 回合数)) {
                --round_;
                sender << "游戏达到最大回合限制，游戏平局";
            } else {
                role_manager_.Foreach([&](auto& role)
                    {
                        if ((!last_round_civilian_lost_ && civilian_lost && role.GetTeam() == Team::平民) ||
                                (!last_round_killer_lost_ && killer_lost && role.GetTeam() == Team::杀手)) {
                            if (role.PlayerId().has_value()) {
                                Tell(*role.PlayerId()) << "很遗憾，您所在的阵营失败了";
                            }
                            if (role.CanAct()) {
                                DisableAct_(role);
                            }
                        }
                    });
                last_round_civilian_lost_ = civilian_lost;
                last_round_killer_lost_ = killer_lost;
                last_round_traitor_lost_ = traitor_lost;
                sender << "游戏继续，第 " << round_ << " 回合开始，请私信裁判行动";
                return false;
            }
        }

        role_manager_.Foreach([&](auto& role)
            {
                if (role.GetTeam() == Team::平民) {
                    role.SetWinner(!civilian_lost);
                } else if (role.GetTeam() == Team::杀手) {
                    role.SetWinner(!killer_lost);
                } else if (role.GetOccupation() == Occupation::内奸) {
                    role.SetWinner(!traitor_lost);
                } else if (role.GetOccupation() != Occupation::人偶) {
                    assert(false);
                }
            });

        return true;
    }

    bool Settlement_()
    {
        auto sender = Boardcast();
        sender << "第 " << round_ << " 回合结束，下面公布各角色血量";
        SettlementAction_(sender);
        RefreshRoles_(sender);
        return CheckTeamsLost_(sender);
    }

    void DisableAct_(RoleBase& role, const bool delay_to_refresh = false)
    {
        if (delay_to_refresh) {
            // Some logic may depend on the value of |role.can_act_| so we cannot modify it immediately.
            // For example, the ghost will be disabled acting when being exorcismed. If we modify the
            // |role.can_act_| immediately, the action will not be emplaced into |role.history_status_|.
            role.DisableActWhenRefresh();
        } else {
            role.DisableAct();
        }
        if (role.PlayerId()) {
            Tell(*role.PlayerId()) << "您失去了行动能力";
            Eliminate(*role.PlayerId());
        }
    }

    template <typename RoleType>
    static std::unique_ptr<RoleBase> MakeRole_(const uint64_t pid, const Token token, const RoleOption& option, RoleManager& role_manager)
    {
        return std::make_unique<RoleType>(pid, token, option, role_manager);
    }

    static RoleManager::RoleVec LoadRoleVec_(const std::vector<Occupation>& occupation_list, const RoleOption& option, RoleManager& role_manager)
    {
        RoleManager::RoleVec v;
        for (uint32_t i = 0, pid = 0; i < occupation_list.size(); ++i) {
            if (occupation_list[i] == Occupation::人偶) {
                v.emplace_back(std::make_unique<PuppetRole>(Token(i), option, role_manager));
            } else {
                v.emplace_back(k_role_makers_[occupation_list[i].ToUInt()](pid++, Token(i), option, role_manager));
            }
        }
        return v;
    }

    static RoleManager::RoleVec GetRoleVec_(const GameOption& option, const RoleOption& role_option, RoleManager& role_manager)
    {
        const auto make_roles = [&](const auto& occupation_list)
            {
                std::vector<PlayerID> pids;
                for (uint32_t i = 0; i < option.PlayerNum(); ++i) {
                    pids.emplace_back(i);
                }
                std::vector<Token> tokens;
                for (uint32_t i = 0; i < occupation_list.size(); ++i) {
                    tokens.emplace_back(i);
                }
                std::random_device rd;
                std::mt19937 g(rd());
                std::ranges::shuffle(pids, g);
                std::ranges::shuffle(tokens, g);
                RoleManager::RoleVec v;
                for (size_t i = 0, pid_i = 0; i < occupation_list.size(); ++i) {
                    if (occupation_list[i] == Occupation::人偶) {
                        v.emplace_back(std::make_unique<PuppetRole>(tokens[i], role_option, role_manager));
                    } else {
                        v.emplace_back(k_role_makers_[static_cast<uint32_t>(occupation_list[i])](pids[pid_i++], tokens[i], role_option, role_manager));
                    }
                }
                std::ranges::sort(v, [](const auto& _1, const auto& _2) { return _1->GetToken() < _2->GetToken(); });
                return v;
            };
        if (const auto& occupation_list = GetOccupationList(option); !occupation_list.empty()) {
            return make_roles(occupation_list);
        }
        switch (option.PlayerNum()) {
        case 5: return
                make_roles(std::array<Occupation, 5>{Occupation::杀手, Occupation::替身, Occupation::侦探, Occupation::圣女, Occupation::平民});
        case 6: return
                make_roles(std::array<Occupation, 6>{Occupation::杀手, Occupation::替身, Occupation::侦探, Occupation::圣女, Occupation::平民, Occupation::平民});
        case 7: return
                make_roles(std::array<Occupation, 7>{Occupation::杀手, Occupation::替身, Occupation::侦探, Occupation::圣女, Occupation::平民, Occupation::平民, Occupation::内奸});
        case 8: return rand() % 2 ?
                make_roles(std::array<Occupation, 9>{Occupation::杀手, Occupation::替身, Occupation::刺客, Occupation::侦探, Occupation::圣女, Occupation::守卫, Occupation::平民, Occupation::平民, Occupation::人偶}) :
                make_roles(std::array<Occupation, 8>{Occupation::杀手, Occupation::替身, Occupation::恶灵, Occupation::侦探, Occupation::圣女, Occupation::灵媒, Occupation::平民, Occupation::平民});
        case 9: return rand() % 2 ?
                make_roles(std::array<Occupation, 9>{Occupation::杀手, Occupation::替身, Occupation::刺客, Occupation::侦探, Occupation::圣女, Occupation::守卫, Occupation::平民, Occupation::平民, Occupation::内奸}) :
                make_roles(std::array<Occupation, 9>{Occupation::杀手, Occupation::替身, Occupation::恶灵, Occupation::侦探, Occupation::圣女, Occupation::灵媒, Occupation::平民, Occupation::平民, Occupation::内奸});
        default:
            assert(false);
            return {};
        }
    }

    std::string Image_(const char* const name, const int32_t width) const
    {
        return std::string("<img src=\"file://") + option().ResourceDir() + "/" + name + ".png\" style=\"width:" +
            std::to_string(width) + "px; vertical-align: middle;\">";
    }

    std::string RoleInfo_() const
    {
        std::string s = HTML_SIZE_FONT_HEADER(4) "<b>本场游戏包含职业：";
        std::vector<Occupation> occupations;
        role_manager_.Foreach([&](const auto& role)
            {
                occupations.emplace_back(role.GetOccupation());
            });
        std::ranges::sort(occupations);
        for (const auto& occupation : occupations) {
            if (occupation == Occupation::杀手 || occupation == Occupation::替身 || occupation == Occupation::恶灵 || occupation == Occupation::刺客) {
                s += HTML_COLOR_FONT_HEADER(red);
            } else if (occupation == Occupation::内奸 || occupation == Occupation::人偶) {
                s += HTML_COLOR_FONT_HEADER(blue);
            } else {
                s += HTML_COLOR_FONT_HEADER(black);
            }
            s += occupation.ToString();
            if (occupation == Occupation::人偶) {
                s += "（NPC）";
            }
            s += HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE;
        }
        s += "</b>" HTML_FONT_TAIL;
        return s;
    }

    std::string Html_(const bool with_action) const
    {
        const char* const k_dark_blue = "#7092BE";
        const char* const k_middle_grey = "#E0E0E0";
        const char* const k_light_grey = "#F5F5F5";

        html::Table table(0, role_manager_.Size() + 1);
        table.SetTableStyle(" align=\"center\" cellspacing=\"" + std::to_string(k_cellspacing_) + "\" cellpadding=\"" +
                std::to_string(k_cellpadding_) + "\"");

        const auto new_line = [&](const std::string_view title, const char* const color, const auto fn)
            {
                table.AppendRow();
                table.GetLastRow(0).SetContent("**" + std::string(title) + "**");
                table.GetLastRow(0).SetColor(color);
                for (uint32_t token_id = 0; token_id < role_manager_.Size(); ++token_id) {
                    const auto& role = role_manager_.GetRole(Token{token_id});
                    auto& box = table.Get(table.Row() - 1, token_id + 1);
                    box.SetColor(role.IsAlive() ? k_dark_blue : k_middle_grey);
                    fn(box, role);
                }
            };
        new_line("玩家", k_dark_blue, [&](html::Box& box, const RoleBase& role)
                {
                    const auto image = (role.IsAlive() && !with_action) ? Image_("unknown_player", k_avatar_width_)       :
                                       role.PlayerId().has_value()      ? PlayerAvatar(*role.PlayerId(), k_avatar_width_) :
                                       "<p style=\"width:" + std::to_string(k_avatar_width_) + "px;\"></p>";
                    box.SetContent(image);
                });
        new_line("角色代号", k_dark_blue, [&](html::Box& box, const RoleBase& role)
                {
                    box.SetContent(std::string("<font size=\"6\"> **") + role.GetToken().ToChar() + "** ");
                });
        new_line("职业", k_dark_blue, [&](html::Box& box, const RoleBase& role)
                {
                    box.SetContent(std::string("<font size=\"5\"> **") +
                            (with_action ? role.GetOccupation().ToString() : "??") +
                            "** " HTML_FONT_TAIL);
                });
        new_line("初始状态", k_light_grey, [&](html::Box& box, const RoleBase& role)
                {
                    box.SetContent("<p align=\"left\"><font size=\"4\">" +
                            Image_("blank", k_icon_size_) + std::to_string(GET_OPTION_VALUE(option(), 血量)) + "</font></p>");
                    box.SetColor(k_light_grey);
                });
        for (uint32_t r = 0; r < (with_action ? round_ : round_ - 1); ++r) {
            table.AppendRow();
            table.GetLastRow(0).SetContent("**第 " + std::to_string(r + 1) + " 回合**");
            table.GetLastRow(0).SetColor(r % 2 ? k_light_grey : k_middle_grey);
            if (with_action) {
                for (uint32_t token_id = 0; token_id < role_manager_.Size(); ++token_id) {
                    const auto status = role_manager_.GetRole(Token{token_id}).GetHistoryStatus(r);
                    table.Get(table.Row() - 1, token_id + 1).SetColor(r % 2 ? k_light_grey : k_middle_grey);
                    if (!status) {
                        continue;
                    }
                    table.Get(table.Row() - 1, token_id + 1).SetContent("**" +
                            std::visit([](const auto& action) { return action.ToString(); }, status->action_) + "**");
                }
                table.AppendRow();
                table.MergeDown(table.Row() - 2, 0, 2);
            }
            for (uint32_t token_id = 0; token_id < role_manager_.Size(); ++token_id) {
                const auto& role = role_manager_.GetRole(Token{token_id});
                const auto status = role.GetHistoryStatus(r);
                table.Get(table.Row() - 1, token_id + 1).SetColor(r % 2 ? k_light_grey : k_middle_grey);
                if (!status) {
                    continue;
                }
                const auto last_hp = r == 0 ? GET_OPTION_VALUE(option(), 血量) : role.GetHistoryStatus(r - 1)->hp_;
                if (!with_action && last_hp <= 0) { // hind the hp from dead role to protect Ghost's identity
                    continue;
                }
                const auto image = Image_(
                    last_hp > 0 && status->hp_ <= 0 ? "dead" :
                    last_hp < status->hp_ ? "up"   :
                    last_hp > status->hp_ ? "down" : "blank", k_icon_size_);
                table.Get(table.Row() - 1, token_id + 1).SetContent("<p align=\"left\"><font size=\"4\">" +
                        image + std::to_string(status->hp_) + "</font></p>");
            }
        }

        return role_info_ + "\n\n" + table.ToString();
    }

    AtomReqErrCode Status_(const PlayerID pid, const bool is_public, MsgSenderBase& reply)
    {
        if (!is_public) {
            const auto& role = role_manager_.GetRole(pid);
            reply() << PrivateRoleInfo_(role) << "，剩余 "
                << (std::get_if<CureAction>(&role.CurAction()) ? role.RemainCure() - 1 : role.RemainCure()) << " 次治愈机会";
        }
        reply() << Markdown("## 第 " + std::to_string(round_) + " 回合\n\n" + table_html_, k_image_width_);
        return StageErrCode::OK;
    }

    bool OnRoundFinish_()
    {
        if (!Settlement_()) {
            table_html_ = Html_(false);
            Boardcast() << Markdown("## 第 " + std::to_string(round_) + " 回合\n\n" + table_html_, k_image_width_);
            ClearReady();
            StartTimer(GET_OPTION_VALUE(option(), 时限));
            return false;
        }
        Boardcast() << Markdown("## 终局\n\n" + Html_(true), k_image_width_);
        return true;
    }

    AtomReqErrCode GenericAct_(const PlayerID pid, const bool is_public, MsgSenderBase& reply, const ActionVariant& action)
    {
        if (is_public) {
            reply() << "行动失败：请您私信裁判行动";
            return StageErrCode::FAILED;
        }
        if (IsReady(pid)) {
            reply() << "行动失败：您已经完成本回合行动了";
            return StageErrCode::FAILED;
        }
        auto& role = role_manager_.GetRole(pid);
        if (!role.CanAct()) {
            reply() << "行动失败：您已经失去了行动能力";
            return StageErrCode::FAILED; // should not appened for user player
        }
        if (!std::visit([&role, &reply](auto& action) { return role.Act(action, reply); }, action)) {
            return StageErrCode::FAILED;
        }
        return StageErrCode::READY;
    }

    AtomReqErrCode Hurt_(const PlayerID pid, const bool is_public, MsgSenderBase& reply, const Token token, const int32_t hp)
    {
        if (!role_manager_.IsValid(token)) {
            reply() << "攻击失败：场上没有该角色";
            return StageErrCode::FAILED;
        }
        return GenericAct_(pid, is_public, reply, HurtAction{.token_ = token, .hp_ = hp});
    }

    AtomReqErrCode Cure_(const PlayerID pid, const bool is_public, MsgSenderBase& reply, const Token token, const bool is_heavy)
    {
        if (!role_manager_.IsValid(token)) {
            reply() << "治愈失败：场上没有该角色";
            return StageErrCode::FAILED;
        }
        return GenericAct_(pid, is_public, reply, CureAction{.token_ = token, .hp_ = is_heavy ? k_heavy_cure_hp : k_normal_cure_hp});
    }

    AtomReqErrCode Detect_(const PlayerID pid, const bool is_public, MsgSenderBase& reply, const Token token)
    {
        if (!role_manager_.IsValid(token)) {
            reply() << "侦查失败：场上没有该角色";
            return StageErrCode::FAILED;
        }
        return GenericAct_(pid, is_public, reply, DetectAction{.token_ = token});
    }

    AtomReqErrCode BlockHurt_(const PlayerID pid, const bool is_public, MsgSenderBase& reply, const std::optional<Token>& token)
    {
        if (token.has_value() && !role_manager_.IsValid(*token)) {
            reply() << "挡刀失败：场上没有该角色";
            return StageErrCode::FAILED;
        }
        return GenericAct_(pid, is_public, reply, BlockHurtAction{token});
    }

    AtomReqErrCode Exocrism_(const PlayerID pid, const bool is_public, MsgSenderBase& reply, const Token token)
    {
        if (!role_manager_.IsValid(token)) {
            reply() << "驱灵失败：场上没有该角色";
            return StageErrCode::FAILED;
        }
        return GenericAct_(pid, is_public, reply, ExocrismAction{.token_ = token});
    }

    AtomReqErrCode ShieldAnti_(const PlayerID pid, const bool is_public, MsgSenderBase& reply, const int32_t hp)
    {
        return GenericAct_(pid, is_public, reply, ShieldAntiAction{.hp_ = hp});
    }

    AtomReqErrCode Pass_(const PlayerID pid, const bool is_public, MsgSenderBase& reply)
    {
        return GenericAct_(pid, is_public, reply, PassAction{});
    }

    static RoleMaker k_role_makers_[Occupation::Count()];
    static constexpr const uint32_t k_avatar_width_ = 80;
    static constexpr const uint32_t k_cellspacing_ = 3;
    static constexpr const uint32_t k_cellpadding_ = 1;
    static constexpr const uint32_t k_icon_size_ = 40;
    RoleManager role_manager_;
    const uint32_t k_image_width_;
    const std::string role_info_;
    int round_;
    std::string table_html_;

    bool last_round_civilian_lost_;
    bool last_round_killer_lost_;
    bool last_round_traitor_lost_;
};

MainStage::RoleMaker MainStage::k_role_makers_[Occupation::Count()] = {
    // killer team
    [static_cast<uint32_t>(Occupation(Occupation::杀手))] = &MainStage::MakeRole_<KillerRole>,
    [static_cast<uint32_t>(Occupation(Occupation::替身))] = &MainStage::MakeRole_<BodyDoubleRole>,
    [static_cast<uint32_t>(Occupation(Occupation::恶灵))] = &MainStage::MakeRole_<GhostRole>,
    [static_cast<uint32_t>(Occupation(Occupation::刺客))] = &MainStage::MakeRole_<AssassinRole>,
    // civilian team
    [static_cast<uint32_t>(Occupation(Occupation::平民))] = &MainStage::MakeRole_<CivilianRole>,
    [static_cast<uint32_t>(Occupation(Occupation::圣女))] = &MainStage::MakeRole_<GoddessRole>,
    [static_cast<uint32_t>(Occupation(Occupation::侦探))] = &MainStage::MakeRole_<DetectiveRole>,
    [static_cast<uint32_t>(Occupation(Occupation::灵媒))] = &MainStage::MakeRole_<SorcererRole>,
    [static_cast<uint32_t>(Occupation(Occupation::守卫))] = &MainStage::MakeRole_<GuardRole>,
    // special team
    [static_cast<uint32_t>(Occupation(Occupation::内奸))] = &MainStage::MakeRole_<TraitorRole>,
};

MainStageBase* MakeMainStage(MsgSenderBase& reply, GameOption& options, MatchBase& match)
{
    if (!options.ToValid(reply)) {
        return nullptr;
    }
    return new MainStage(options, match);
}
