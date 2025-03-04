// Copyright (c) 2018-present, Chang Liu <github.com/slontia>. All rights reserved.
//
// This source code is licensed under LGPLv2 (found in the LICENSE file).

#include "game_framework/unittest_base.h"

GAME_TEST(2, keep_rotate)
{
    ASSERT_PUB_MSG(OK, 0, "地图 genius");
    ASSERT_PUB_MSG(OK, 0, "回合数 10");
    ASSERT_TRUE(StartGame());
    const std::array<std::string, 2> chess_coor{"A5", "H2"};
    for (uint32_t i = 0; i < 9; ++i) {
        ASSERT_PRI_MSG(OK, 0, chess_coor[0] + " 逆");
        ASSERT_PRI_MSG(CONTINUE, 1, chess_coor[1] + " 逆");
    }
    ASSERT_PRI_MSG(OK, 0, chess_coor[0] + " 逆");
    ASSERT_PRI_MSG(CHECKOUT, 1, chess_coor[1] + " 逆");
    ASSERT_SCORE(10, 10);
}

GAME_TEST(2, forbid_public_message)
{
    ASSERT_PUB_MSG(OK, 0, "地图 genius");
    ASSERT_TRUE(StartGame());
    ASSERT_PUB_MSG(FAILED, 0, "A0 逆");
}

GAME_TEST(2, forbid_rotate_other_chess)
{
    ASSERT_PUB_MSG(OK, 0, "地图 genius");
    ASSERT_TRUE(StartGame());
    ASSERT_PRI_MSG(FAILED, 1, "A0 逆");
}

GAME_TEST(2, forbid_move_other_chess)
{
    ASSERT_PUB_MSG(OK, 0, "地图 genius");
    ASSERT_TRUE(StartGame());
    ASSERT_PRI_MSG(FAILED, 0, "H5 上");
}

GAME_TEST(2, forbid_rotate_shooter_towards_outside)
{
    ASSERT_PUB_MSG(OK, 0, "地图 genius");
    ASSERT_TRUE(StartGame());
    ASSERT_PRI_MSG(FAILED, 0, "A0 顺");
}

GAME_TEST(1, too_few_player)
{
    ASSERT_FALSE(StartGame());
}

GAME_TEST(2, crash_king)
{
    ASSERT_PUB_MSG(OK, 0, "地图 genius");
    ASSERT_TRUE(StartGame());
    ASSERT_PRI_MSG(OK, 0, "H6 左上");
    ASSERT_PRI_MSG(CHECKOUT, 1, "H4 右上");
    ASSERT_SCORE(1, 0);
}

GAME_TEST(2, keep_pass)
{
    ASSERT_PUB_MSG(OK, 0, "地图 genius");
    ASSERT_PUB_MSG(OK, 0, "回合数 10");
    ASSERT_TRUE(StartGame());
    for (uint32_t i = 0; i < 9; ++i) {
        ASSERT_PRI_MSG(OK, 0, "pass");
        ASSERT_PRI_MSG(CONTINUE, 1, "pass");
    }
    ASSERT_PRI_MSG(OK, 0, "pass");
    ASSERT_PRI_MSG(CHECKOUT, 1, "pass");
    ASSERT_SCORE(10, 10);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    return RUN_ALL_TESTS();
}
