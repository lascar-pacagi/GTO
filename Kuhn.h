#ifndef KUHN_H
#define KUHN_H

#include "game.h"
#include "action_list.h"

struct Kuhn {
    enum class Action {
        CHECK = 0,
        BET   = 1,
        CALL  = 2,
        FOLD  = 3,
        JACK  = 4,
        QUEEN = 5,
        KING  = 6,
    };
    static constexpr int MAX_NB_MOVES = 2;    
    uint64_t action_history = 0;
    int nb_actions = 0;
    void play(Action a) {
        actions_history |= int64_t(a) << nb_actions * 3;
        ++nb_actions;
    }
    void undo(Action a) {
        --nb_actions;
        actions_history &= ~(0b111ull << nb_actions * 3);
    }
    int current_player() const {
        return (nb_actions < 2) * 3 + (nb_actions >= 2) * (nb_actions & 1);
    }
    Action get_action(int i) const {
        return action_history & (0b111ull << i * 3);
    }
    bool game_over() const {
        return nb_actions == 5 || nb_actions == 4 && get_action(nb_actions - 1) != BET;
    }
    int payoff(int player) const {

    }
    bool is_chance_node() const {
        return nb_actions < 2;
    }
    Action sample_action() const {

    }
    void actions(ActionList<Action>& actions) const {

    }
};

#endif
