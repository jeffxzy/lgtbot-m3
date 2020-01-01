#pragma once

#include <mutex>
#include <iostream>
#include <string>
#include <functional>
#include "../new-rock-paper-scissors/dllmain.h"
#include "dllmain.h"

#define DLL_EXPORT __declspec(dllexport)
#define DLL_IMPORT __declspec(dllimport)

#define RETURN_IF_FAILED(str) \
do\
{\
  if (const std::string& err = (str); !err.empty())\
    return err;\
} while (0);

struct GameHandle
{
  GameHandle(const std::string& name, const uint64_t min_player, const uint64_t max_player, 
    const std::function<GameBase*(const uint64_t)>& new_game, const std::function<int(GameBase* const)>& release_game,
    const HINSTANCE& module)
    : name_(name), min_player_(min_player), max_player_(max_player),
    new_game_(new_game), release_game_(release_game), module_(module) {}
  GameHandle(GameHandle&&) = default;
  ~GameHandle() { FreeLibrary(module_); }
  
  const std::string name_;
  const uint64_t min_player_;
  const uint64_t max_player_;
  const std::function<GameBase*(const uint64_t)> new_game_;
  const std::function<int(GameBase* const)> release_game_;
  const HINSTANCE module_;
};

static const int32_t LGT_AC = -1;
static const UserID INVALID_USER_ID = 0;
static const GroupID INVALID_GROUP_ID = 0;

static std::mutex g_mutex;

static std::map<std::string, GameHandle> g_game_handles;

static AT_CALLBACK g_at_cb = nullptr;
static PRIVATE_MSG_CALLBACK g_send_pri_msg_cb = nullptr;
static PUBLIC_MSG_CALLBACK g_send_pub_msg_cb = nullptr;
static UserID g_this_uid = INVALID_USER_ID;

static std::string At(const UserID uid) { return g_at_cb(uid); }
static void SendPrivateMsg(const UserID uid, const std::string& msg) { return g_send_pri_msg_cb(uid, msg.c_str()); }
static void SendPublicMsg(const GroupID gid, const std::string& msg) { return g_send_pub_msg_cb(gid, msg.c_str()); }
