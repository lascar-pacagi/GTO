#ifndef KUHN_H
#define KUHN_H

#include "game.h"
#include "action_list.h"
#include "misc.h"
#include <chrono>

struct Kuhn {
    enum Action {
        CHECK = 0,
        BET   = 1,
        CALL  = 2,
        FOLD  = 3,
        JACK  = 4,
        QUEEN = 5,
        KING  = 6,
        END   = 7,
    };
    static constexpr int MAX_NB_MOVES = 3;    
    uint32_t action_history = 0;
    int nb_actions = 0;
    mutable PRNG prng{std::chrono::system_clock::now().time_since_epoch().count()};
    void play(Action a) {
        action_history |= int64_t(a) << nb_actions * 3;
        ++nb_actions;
    }
    void undo(Action a) {
        --nb_actions;
        action_history &= ~(0b111ull << nb_actions * 3);
    }
    int current_player() const {
        return nb_actions < 2 ? CHANCE : nb_actions & 1;
    }
    Action get_action(int i) const {
        return Action((action_history & (0b111ull << i * 3)) >> i * 3);
    }
    bool game_over() const {
        return nb_actions == 5 || (nb_actions == 4 && get_action(nb_actions - 1) != BET);
    }    
    bool is_chance_node() const {
        return nb_actions < 2;
    }    
    static constexpr Action ACTIONS[] {
        JACK, QUEEN, KING, END,
        QUEEN, KING, END, JACK, KING, END, JACK, QUEEN, END,
        CHECK, BET, END,
        CHECK, BET, END, FOLD, CALL, END,
        FOLD, CALL, END,
    };
    static constexpr int DELTAS[] {
        0, 4, 13, 16, 22,
    };
    static constexpr int PAYOFFS[] {
        0, -2, -1, -1, 0, -1, -1, 0, 0, 0, 0, 0, -1, 1, 0, 2, -1, 1, 0, 2, 0, 0, 0, 2, 1, 0, -2, 1, 1, -2, -2
    };
    int payoff(int player) const {
        constexpr uint32_t magic = 3816247202;
        constexpr int n = 27;        
        return PAYOFFS[action_history * magic >> n] * (player == PLAYER1 ? 1 : -1);
    }
    Action sample_action() const {
        if (nb_actions == 0) {
            return Action(JACK + reduce(prng.rand<uint32_t>(), 3)); 
        } else {
            return ACTIONS[4 + (get_action(0) - JACK) * 3 + reduce(prng.rand<uint32_t>(), 2)];
        }
    }
    void actions(ActionList<Action, MAX_NB_MOVES>& actions) {
        Action* action_list = actions.action_list;
        int start = DELTAS[nb_actions];
        start += (nb_actions == 1) * ((get_action(nb_actions - 1) - JACK) * 3) + 
                (nb_actions == 3) * ((get_action(nb_actions - 1) - CHECK) * 3);
        const Action* moves = ACTIONS + start;
        for (Action a = *moves; a != END; a = *moves++) {
            *action_list++ = a;
        }
        actions.last = action_list;
    }
    friend std::ostream& operator<<(std::ostream& os, const Kuhn& kuhn) {
        auto h = kuhn.action_history;
        for (int i = 0; i < kuhn.nb_actions; i++) {
            os << Action(h & 0x7) << ' ';
            h >>= 3;
        }
        return os;
    }
    friend std::ostream& operator<<(std::ostream& os, const Kuhn::Action& action) {
        static const char* repr[] = {
            "CHECK",
            "BET",
            "CALL",
            "FOLD",
            "JACK",
            "QUEEN",
            "KING",
            "END",
        };
        os << repr[action];
        return os;
    }
};

#endif
