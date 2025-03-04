
target_link_libraries(mahjong_17_steps Mahjong MahjongAlgorithm)
add_dependencies(mahjong_17_steps Mahjong MahjongAlgorithm)
if (WITH_TEST)
    target_link_libraries(test_game_mahjong_17_steps Mahjong MahjongAlgorithm)
    add_dependencies(test_game_mahjong_17_steps Mahjong MahjongAlgorithm)
    target_link_libraries(run_game_mahjong_17_steps Mahjong MahjongAlgorithm)
    add_dependencies(run_game_mahjong_17_steps Mahjong MahjongAlgorithm)
endif()
