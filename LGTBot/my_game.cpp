#include "stdafx.h"
#include "my_game.h"

class MyGame;
class MyPlayer;

const std::string kGameName;
const int kMinPlayer = 1;
const int kMaxPlayer = 2;

typedef enum
{
  ROCK_SEL = 0,
  PAPER_SEL = 1,
  SCISSORS_SEL = 2,
  NONE_SEL = -1
} SELECTION;

/*======== Define your player here ===========*/
DEFINE_PLAYER
public:
  const int kWinRound = 3;
  int cur_win_ = 0;
  SELECTION sel_;

  int get_score()
  {
    return 0;
  }

  void round_init()
  {
    sel_ = NONE_SEL;
  }

  bool has_sel()
  {
    return sel_ != NONE_SEL;
  }

  bool sel(SELECTION sel)
  {
    if (has_sel()) return false;
    sel_ = sel;
    return true;
  }

  bool is_win()
  {
    return cur_win_ >= kWinRound;
  }
DEFINE_END

/* Define main stage here. */
DEFINE_STAGE(MAIN, CompStage)
public:
  const int winRound = 3;
  int cur_round = 0;

  void Start()
  {
    assert(!start_next_round());
  }

  void Over() {}

  bool Request(uint32_t pid, std::string msg, int32_t sub_type)
  {
    if (PassRequest(pid, msg, sub_type)) return start_next_round();
  }

  bool TimerCallback()
  {
    return start_next_round();
  }

  bool start_next_round()
  {
    bool has_winner = false;
    OperatePlayer([&has_winner](MyPlayer& p)
    {
      if (p.is_win())
      {
        assert(!has_winner);
        has_winner = true;
      }
    });
    
    if (!has_winner)
    {
      ++cur_round;
      SWITCH_SUBSTAGE(ROUND);
      return false; // continue
    }
    return true;  // finish
  }
DEFINE_END


DEFINE_SUBSTAGE(ROUND, TimerStage<5>, MAIN)
public:
  void Start()
  {
    OperatePlayer([](MyPlayer& p)
    {
      p.round_init();
    });
    Broadcast("第" + std::to_string(get_main().cur_round) + "回合开始！");
  }

  void Over()
  {
    auto p1 = GET_PLAYER(0);
    auto p2 = GET_PLAYER(1);
    switch (sel_comp(p1.sel_, p2.sel_))
    {
      case 1:
        ++p1.cur_win_;
        break;
      case -1:
        ++p2.cur_win_;
        break;
      case 0:
        break;
      default:
        assert(false);
    }
  }

  bool Request(uint32_t pid, std::string msg, int32_t sub_type)
  {
    auto p = GET_PLAYER(pid);
    if (sub_type != PRIVATE_MSG)
    {
      Broadcast(pid, "请私信选择您的答案，公开的答案无效");
      return false;
    }
    else if (p.has_sel())
    {
      Reply(pid, "您已经选择过了");
      return false;
    }
    else if (msg == "石头")
    {
      p.sel(ROCK_SEL);
      return all_selected();
    }
    else if (msg == "剪刀")
    {
      p.sel(SCISSORS_SEL);
      return all_selected();
    }
    else if (msg == "布")
    {
      p.sel(PAPER_SEL);
      return all_selected();
    }
    else
    {
      Reply(pid, "您瞧瞧您选的这是啥啊，会不会选啊，就仨选项听好了啊，剪刀、石头、布，再选一遍");
      return false;
    }
  }

  int sel_comp(const SELECTION& _1, const SELECTION& _2)
  {
    auto greater_than = [](const SELECTION& _1, const SELECTION& _2) -> bool
    {
      return _1 == (_2 + 1) % 3;
    };
    if (_1 == _2) return 0;
    else if (_2 == NONE_SEL || greater_than(_1, _2)) return 1;
    else if (_1 == NONE_SEL || greater_than(_2, _1)) return -1;
    assert(false);
    return 0;
  }

  bool one_selected = false;

  bool all_selected()
  {
    if (one_selected) return true;
    one_selected = true;
    return false;
  }
DEFINE_END

BIND_STAGE(MAIN, STAGE_KV(ROUND))
