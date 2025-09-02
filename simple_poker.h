#ifndef SIMPLE_POKER_H
#define SIMPLE_POKER_H
#include <chrono>
#include "game.h"
#include "misc.h"

struct SimplePoker {
    enum Action {
        CHECK = 0,
        BET   = 1,
        CALL  = 2,
        FOLD  = 3,
        HL    = 4,
        LL    = 5,
        HH    = 6,
        END   = 7,
    };
    using State = uint32_t;
    static constexpr int MAX_NB_ACTIONS = 3;    
    uint32_t action_history = 0;
    int nb_plies = 0;
    mutable PRNG prng{static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
    void reset() {
        action_history = 0;
        nb_plies = 0;
    }
    State get_state() const {
        return nb_plies << 15 | action_history;
    }
    void set_state(State state) {
        action_history = state & 0x7FFF;
        nb_plies       = state >> 15;
    }
    State get_info_set(int player) const {
        constexpr int32_t mask1 = 0b111111111000111;
        constexpr int32_t mask2 = 0b111111111111000;
        return (nb_plies << 15) | ((player == PLAYER1 ? mask1 : mask2) & action_history); 
    }
    void play(Action a) {
        action_history |= static_cast<uint32_t>(a) << nb_plies * 3;
        ++nb_plies;
    }
    void undo(Action a) {
        --nb_plies;
        action_history &= ~(0b111 << nb_plies * 3);
    }
    int current_player() const {
        return nb_plies < 2 ? CHANCE : nb_plies & 1;
    }
    Action get_action(int i) const {
        return Action((action_history & (0b111 << i * 3)) >> i * 3);
    }
    bool game_over() const {
        return nb_plies == 5 || (nb_plies == 4 && get_action(3) != BET);
    }
    bool is_chance_node() const {
        return nb_plies < 2;
    }
};

#endif
