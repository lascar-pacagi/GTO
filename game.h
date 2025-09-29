#ifndef GAME_H
#define GAME_H
#include "strategy.h"
#include <iostream>
#include <string>
#include "list.h"
#include <atomic>

constexpr int PLAYER1 = 0;
constexpr int PLAYER2 = 1;
constexpr int CHANCE  = 2;

constexpr int other_player(int player) {
    return 1 - player;
}

#endif