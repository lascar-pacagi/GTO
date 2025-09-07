#ifndef SIMPLE_POKER_H
#define SIMPLE_POKER_H
#include <chrono>
#include "game.h"
#include "misc.h"
#include "list.h"

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
    using InfoSet = uint32_t;
    using Proba = uint8_t;

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
    InfoSet get_info_set(int player) const {
        constexpr int32_t mask1 = 0b111111111000111;
        constexpr int32_t mask2 = 0b111111111111000;
        return (nb_plies << 15) | ((player == PLAYER1 ? mask1 : mask2) & action_history); 
    }
    static constexpr int PAYOFFS[] {
        0,
    };
    int payoff(int player) const {
        return 0;
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
    static constexpr Action ACTIONS[] {
        HH, LL, HL, END,
        CHECK, BET, END,
        FOLD, CALL, END,
    };
    static constexpr int DELTAS[] {
        0, 0, 4, 4, 7,
    };
    void actions(List<Action, MAX_NB_ACTIONS>& actions) {
        Action* action_list = actions.list;
        int start = DELTAS[nb_plies];
        start += (nb_plies == 3) * (get_action(2) == BET) * 3;       
        const Action* moves = ACTIONS + start;
        for (Action a = *moves; a != END; a = *++moves) {
            *action_list++ = a;
        }
        actions.last = action_list;
    }
    bool is_chance_node() const {
        return nb_plies < 2;
    }
    void probabilities(List<Proba, MAX_NB_ACTIONS>& probas) {
        uint8_t* proba_list = probas.list;
        if (nb_plies < 2) {    
            *proba_list++ = 25;
            *proba_list++ = 25;
            *proba_list++ = 50;        
        }
        probas.last = proba_list;
    }
    Action sample_action() const {
        static const Action cards[] = {
            HL, HL, LL, HH
        };
        return cards[prng.rand<uint32_t>() % 4];
    }
    friend std::ostream& operator<<(std::ostream& os, const SimplePoker& poker) {                    
        if (poker.nb_plies) {
            os << "[" << poker.get_action(0);
            for (int i = 1; i < poker.nb_plies; i++) {
                os << poker.get_action(i);
            }
            os << "]";
        } else {
            os << "[]";
        }
        return os;
    }
    friend std::ostream& operator<<(std::ostream& os, const SimplePoker::Action& action) {
        static const char* repr[] = {
            "k",
            "b",
            "c",
            "f",
            "HL",
            "LL",
            "HH"
        };
        os << repr[action];
        return os;
    }
};

#endif
