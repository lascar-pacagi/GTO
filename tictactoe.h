#ifndef TICTACTOE_H
#define TICTACTOE_H
#include "game.h"
#include "misc.h"
#include "action_list.h"
#include <iostream>

struct TicTacToe {
    enum Action {
        NONE = 0,
        ROCK = 1,
        PAPER = 2,
        SCISSOR = 3,
    };
    using State = uint32_t;
    static constexpr int MAX_NB_ACTIONS = 3;
    static constexpr int STATE_SPACE_SIZE = 2;    
    State action_history = 0;
    void reset() {
        action_history = 0;
    }
    State get_state() const {
        return action_history;
    }
    void set_state(State state) {
        action_history = state;
    }
    State get_info_set(int player) const {
        return player;
    }
    void play(Action a) {
        action_history |= static_cast<uint32_t>(a) << current_player() * 2;
    }
    void undo(Action a) {
        action_history &= ~(0x3 << (action_history > 3 ? 2 : 0));
    }
    Action sample_action() const {
        return NONE;
    }
    int current_player() const {
        return action_history == 0 ? PLAYER1 : PLAYER2;
    }
    bool game_over() const {
        return action_history > 3;
    }    
    bool is_chance_node() const {
        return false;
    }
    static constexpr float PAYOFFS[] {
        0, 0, 0, 0, 0, 0, 1, -1, 0, -1, 0, 1, 0, 1, -1, 0,                    
    };
    float payoff(int player) const {
        return PAYOFFS[action_history] * (player == PLAYER1 ? 1 : -1);
    }
    void actions(ActionList<Action, MAX_NB_ACTIONS>& actions) {
        Action* action_list = actions.action_list;
        *action_list++ = ROCK;
        *action_list++ = PAPER;
        *action_list++ = SCISSOR;
        actions.last = action_list;
    }
    friend std::ostream& operator<<(std::ostream& os, const TicTacToe& ttt) {
        auto h = ttt.action_history;
        auto a1 = h & 0x3;
        auto a2 = h >> 2;
        if (h == 0) {
            os << "()";
        } else if (a2 == 0) {
            os << "(" << Action(a1) << ")";
        } else {
            os << "(" << Action(a1) << ' ' << Action(a2) << ")";
        }
        return os;
    }
    friend std::ostream& operator<<(std::ostream& os, const TicTacToe::Action& action) {
        static const char* repr[] = {
            "NONE",
            "ROCK",
            "PAPER",
            "SCISSOR",
        };
        os << repr[action];
        return os;
    }
};

#endif