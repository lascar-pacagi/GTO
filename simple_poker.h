#ifndef SIMPLE_POKER_H
#define SIMPLE_POKER_H
#include <chrono>
#include "game.h"
#include "misc.h"
#include "list.h"
#include <cmath>

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
    enum InfoSet : uint32_t {};
    static constexpr int MAX_NB_PLAYER_ACTIONS = 2;
    static constexpr int MAX_NB_CHANCE_ACTIONS = 3;  
    static constexpr int MAX_NB_ACTIONS = std::max(MAX_NB_PLAYER_ACTIONS, MAX_NB_CHANCE_ACTIONS);    
    uint32_t action_history = 0;
    int nb_plies = 0;
    mutable PRNG prng{static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
    void reset() {
        action_history = 0;
        nb_plies = 0;
    }
    static std::vector<std::pair<InfoSet, Action>> info_sets_and_actions(State state, int player) {
        int nb_plies = state >> 15;
        std::vector<std::pair<InfoSet, Action>> res;
        if (player == PLAYER1) {
            if (nb_plies >= 3) res.emplace_back(InfoSet((2 << 15) | (state & 0b111U)), Action((state >> 6) & 7));
            if (nb_plies >= 5) res.emplace_back(InfoSet((4 << 15) | (state & 0b111'111'000'111U)), Action((state >> 12) & 7));
        } else {
            if (nb_plies >= 4) res.emplace_back(InfoSet((3 << 15) | (state & 0b111'111'000U)), Action((state >> 9) & 7));
        };
        return res;
    }
    template<typename T = double>
    static T chance_reach_proba(State state) {
        static constexpr T probas[] = { T(1.0), T(1.0), T(1.0), T(1.0), T(0.5), T(0.25), T(0.25) };
        auto hand1 = state & 7;
        auto hand2 = (state >> 3) & 7;
        return probas[hand1] * probas[hand2];
    }
    State get_state() const {
        return nb_plies << 15 | action_history;
    }
    void set_state(State state) {
        action_history = state & 0x7FFF;
        nb_plies       = state >> 15;
    }
    InfoSet get_info_set(int player) const {
        constexpr uint32_t masks[] = { 0b111111111000111U, 0b111111111111000U, 0b111111111111111U };
        return InfoSet((nb_plies << 15) | (masks[player] & action_history)); 
    }
    static constexpr int8_t PAYOFFS[] {
        0, 0, 0, -1, 0, 3, -1, 1, -3, 0, -1, 1, 0, 0, 1, 0, 0, -1, -1, 3, -1, 0, 0, 0, -1, 1, -3, -1, 1, 1,
    };
    int payoff(int player) const {
        constexpr uint32_t magic = 1909500917u;
        constexpr int n = 27;   
        return PAYOFFS[action_history * magic >> n] * (player == PLAYER1 ? 1 : -1);
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
    template<int SIZE>
    void actions(List<Action, SIZE>& actions) {
        Action* action_list = actions.list;
        int start = DELTAS[nb_plies];
        start += (nb_plies == 3) * (get_action(2) == BET) * 3;       
        const Action* moves = ACTIONS + start;
        for (Action a = *moves; a != END; a = *++moves) {
            *action_list++ = a;
        }
        actions.last = action_list;
    }
    bool is_chance_player() const {
        return nb_plies < 2;
    }
    void probas(List<int, MAX_NB_CHANCE_ACTIONS>& probas) {
        int* proba_list = probas.list;
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
    friend std::ostream& operator<<(std::ostream& os, const SimplePoker::InfoSet& info_set) {
        uint32_t i = info_set;
        int nb_plies = i >> 15; 
        os << nb_plies << ' ';       
        if ((i & 7) != 0) {
            os << Action(i & 7);
            i >>= 3;
        } else {
            i >>= 3;
            os << Action(i & 7);
        }
        os << ' ';        
        for (int j = 0; j < nb_plies - 2; j++) {            
            i >>= 3;
            os << Action(i & 7);
        }
        return os;
    }
};

#endif
