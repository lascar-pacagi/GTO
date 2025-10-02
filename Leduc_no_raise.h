#ifndef LEDUC_NO_RAISE_H
#define LEDUC_NO_RAISE_H

#include "game.h"
#include "list.h"
#include "misc.h"
#include <chrono>
#include <iostream>

struct LeducNoRaise {
    enum Action {
        CHECK = 0,
        BET   = 1,
        CALL  = 2,
        FOLD  = 3,
        JACK  = 4,
        QUEEN = 5,
        KING  = 6,
        END = 7,
    };

    using State = uint64_t;
    enum InfoSet : uint64_t {};
    static constexpr int MAX_NB_PLAYER_ACTIONS = 2;
    static constexpr int MAX_NB_CHANCE_ACTIONS = 3;  
    static constexpr int MAX_NB_ACTIONS = std::max(MAX_NB_PLAYER_ACTIONS, MAX_NB_CHANCE_ACTIONS);    
    uint64_t action_history = 0;
    uint64_t nb_plies = 0;
    mutable PRNG prng{static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
    
    private:
    static bool is_hand(Action a) {
        return JACK <= a && a <= KING;
    }
    static Action get_action(State state, int i) {
        return Action((state & (0b111ULL << i * 3)) >> i * 3);
    }

    public:
    void reset() {
        action_history = 0;
        nb_plies = 0;
    }
    static std::vector<std::pair<InfoSet, Action>> info_sets_and_actions(State state, int player) {
        int nb_plies = state >> 32;
        std::vector<std::pair<InfoSet, Action>> res;        
        if (player == PLAYER1) {
            res.emplace_back(InfoSet((2ULL << 32) | (state & 0b111ULL)), get_action(state, 2));
            if (nb_plies == 6) {
                res.emplace_back(InfoSet((5ULL << 32) | (state & 0b111'111'111'000'111ULL)), get_action(state, 5));
            } else if (nb_plies == 7) {
                res.emplace_back(InfoSet((4ULL << 32) | (state & 0b111'111'000'111ULL)), get_action(state, 4));
                res.emplace_back(InfoSet((6ULL << 32) | (state & 0b111'111'111'111'000'111ULL)), get_action(state, 6));
            }
        } else {
            if (nb_plies >= 4) {
                res.emplace_back(InfoSet((3ULL << 32) | (state & 0b111'111'000ULL)), get_action(state, 3));
                if (nb_plies == 7) {
                    res.emplace_back(InfoSet((6ULL << 32) | (state & 0b111'111'111'111'111'000ULL)), get_action(state, 6));
                } else if (nb_plies == 8) {
                    res.emplace_back(InfoSet((7ULL << 32) | (state & 0b111'111'111'111'111'111'000ULL)), get_action(state, 7));
                }
            }
        };
        return res;
    }
    template<typename T = double>
    static T chance_reach_proba(State state) {
        auto hand1 = state & 7;
        auto hand2 = (state >> 3) & 7;
        T proba = static_cast<T>(1.0 / 3.0) * (hand1 == hand2 ? static_cast<T>(1.0 / 5.0) : static_cast<T>(2.0 / 5.0));
        auto flop = get_action(state, 4); 
        if (!is_hand(flop)) {
            flop = get_action(state, 5);
        }
        if (is_hand(flop)) {
            if (hand1 == hand2) {
                proba *= static_cast<T>(0.5);
            } else {
                proba *= static_cast<T>(0.25) + (hand1 != flop && hand2 != flop) * static_cast<T>(0.25);
            }
        }
        return proba;
    }
    State get_state() const {
        return nb_plies << 32 | action_history;
    }
    void set_state(State state) {
        action_history = state & 0xFFFFFFFFULL;
        nb_plies       = state >> 32;
    }
    InfoSet get_info_set(int player) const {
        constexpr uint64_t masks[] = { 0xFFFFFFC7ULL, 0xFFFFFFF8ULL, 0xFFFFFFFFULL };        
        return InfoSet((nb_plies << 32) | (masks[player] & action_history)); 
    }
    void play(Action a) {
        action_history |= static_cast<uint64_t>(a) << nb_plies * 3;
        ++nb_plies;
    }
    void undo(Action a) {
        --nb_plies;
        action_history &= ~(0b111ULL << nb_plies * 3);
    }
    int current_player() const {
        static const int player[2][10] = {
            { CHANCE, CHANCE, PLAYER1, PLAYER2, CHANCE, PLAYER1, PLAYER2, PLAYER1, PLAYER2, PLAYER1 },
            { CHANCE, CHANCE, PLAYER1, PLAYER2, PLAYER1, CHANCE, PLAYER1, PLAYER2, PLAYER1, PLAYER2 }
        };
        return player[get_action(3) == BET][nb_plies];
    }
    Action get_action(int i) const {
        return Action((action_history & (0b111ULL << i * 3)) >> i * 3);
    }
    bool game_over() const {
        if (nb_plies <= 2) return false;
        Action last_action = get_action(nb_plies - 1);
        if (last_action == FOLD) return true;
        Action before_last_action = get_action(nb_plies - 2);
        bool is_chance = before_last_action == JACK || before_last_action == QUEEN || before_last_action == KING;
        return nb_plies >= 7 && !is_chance && (last_action == CHECK || last_action == CALL);
    }    
    bool is_chance_player() const {
        return current_player() == CHANCE;
    }
    static constexpr Action ACTIONS[] {
        JACK, QUEEN, KING, END,
        CHECK, BET, END, FOLD, CALL, END,
        QUEEN, KING, END,
        JACK, KING, END,
        JACK, QUEEN, END,
        // 19
        JACK, QUEEN, QUEEN, KING, KING,
        JACK, JACK, QUEEN, KING, KING,
        JACK, JACK, QUEEN, QUEEN, KING,
        // 34
        QUEEN, QUEEN, KING, KING, // JJ 0        
        JACK, QUEEN, KING, KING,  // JQ 1
        JACK, QUEEN, QUEEN, KING, // JK 2
        JACK, JACK, KING, KING,   // QQ 3
        END, END, END, END,
        JACK, JACK, QUEEN, KING,  // QK 5
        END, END, END, END,
        END, END, END, END,
        JACK, JACK, QUEEN, QUEEN, // KK 8
    };
    static constexpr int DELTAS[] {
        0, 0, 4, 4, 4, 4, 4, 4, 4, 10, 0, 0, 0, 0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0, 0, 16  
    };
    static constexpr int8_t PAYOFFS[] {
        0, 0, 3, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, -3, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, -3, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 3, 0, 0, 0, 0, -7, 0, 0, -3, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, -3, 0, 0, 0, 0, 0, -1, 0, 3, 1, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0, 0, 0, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, -5, 0, 0, 0, 1, 0, -3, -3, 0, 0, 0, 3, 0, 0, -1, 0, 0, 0, -3, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 7, -7, -5, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 3, 0, 7, 5, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 0, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 7, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 7, -1, 0, -3, -1, 0, 0, 1, 0, 0, 0, -7, 0, 0, 3, -1, 0, -1, 1, 0, 0, -7, 0, 0, 0, 0, 0, 0, -1, 0, 3, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, 0, 0, -3, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, -3, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 5, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, -3, 0, 0, 3, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, -7, 0, -3, 0, 0, 0, 0, 0, 0, 0, -7, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 3, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, -3, 0, 0, 3, 0, 0, 0, 0, -3, 0, 0, -3, 0, 0, 3, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 3, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 7, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 3, 0, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, -3, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, -3, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, -3, 3, 0, 1, 0, 0, 0, 0, 0, -1, 0, 3, 1, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0, 7, 3, 0, 0, 1, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, -5, 0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, -3, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, -3, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, -5, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -3, 3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 3, 0, 0, -1, 0, 0, -1, -7, 0, 1, 0, 0, 0, 0, 0, 0, 3, 0, 0, -1, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, -3, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 3, -3, 0, 0, 0, 0, 0, -1, -3, 0, 0, 3, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -3, 0, 0, 3, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 5, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 3, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 1, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 7, 7, 0, 0, 0, 0, 0, 0, 5, 0, -7, 0, 0, 0, 0, 0, 0, 7, -5, 0, 0, 0, 0, 0, 0, 0, 7, 0, -7, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, -1, 0, 0, 0, 1, 0, 0, 0, -3, 3, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 1, 0, 0, 0, 0, 0, 0, 7, 3, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, -7, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 7, 0, 0, -3, -1, 0, 0, 1, 0, 0, 0, -7, 0, -3, 3, -1, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0, 0, -7, -1, 0, 3, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, -1, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 5, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, -3, 0, 0, -5, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, -3, 0, 0, 3, 3, 0, 0, 0, 0, 0, -1, 0, 0, 0, 7, 0, -3, 0, 0, 3, 0, 0, 0, 0, -7, 0, -1, -3, 0, 0, 3, 0, 0, 0, 0, 0, 0, -1, 0, 0, 7, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, -5, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 1, 0, 0, 0, 0, 0, 0
    };
    int payoff(int player) const {
        constexpr uint64_t magic = 12313911824519274705ULL;
        constexpr int n = 53;
        return PAYOFFS[action_history * magic >> n] * (player == PLAYER1 ? 1 : -1);
    }
    Action sample_action() const {
        if (nb_plies == 0) {
            return Action(JACK + prng.rand<uint32_t>() % 3); 
        } else if (nb_plies == 1) {
            int card = get_action(0);
            return ACTIONS[19 + (card - JACK) * 5 + prng.rand<uint32_t>() % 5];
        } else {
            int card1 = get_action(0) - 3;
            int card2 = get_action(1) - 3;
            int idx = 34 + (card1 * card2 - 1) * 4 + prng.rand<uint32_t>() % 4;
            return ACTIONS[idx];
        }
    }
    template<int SIZE>
    void actions(List<Action, SIZE>& actions) {        
        Action* action_list = actions.list;
        int start = DELTAS[nb_plies];
        if (nb_plies > 2) {
            int current_player = this->current_player();
            if (current_player == CHANCE) {
                start = DELTAS[((action_history & 7ULL) - 1) * ((action_history >> 3 & 7ULL) - 1)];
            } else {
                Action last_action = get_action(nb_plies - 1);
                bool is_chance = last_action == JACK || last_action == QUEEN || last_action == KING;
                start += !is_chance * (last_action * 3); 
            }
        }
        const Action* moves = ACTIONS + start;
        // std::cout << '#' << nb_plies << '\n';
        // std::cout << start << '\n';
        // std::cout << current_player() << '\n';
        for (Action a = *moves; a != END; a = *++moves) {
        //    std::cout << a << ' ';
            *action_list++ = a;
        }
        //std::cout << '\n';
        actions.last = action_list;
    }
    void probas(List<int, MAX_NB_CHANCE_ACTIONS>& probas) {
        int* proba_list = probas.list;
        if (nb_plies < 2) {
            Action card = static_cast<Action>(action_history & 7ULL);
            *proba_list++ = 20 + (nb_plies == 1) * (card != JACK) * 20;
            *proba_list++ = 20 + (nb_plies == 1) * (card != QUEEN) * 20;
            *proba_list++ = 20 + (nb_plies == 1) * (card != KING) * 20;
        } else {
            Action card1 = static_cast<Action>(action_history & 7ULL);
            Action card2 = static_cast<Action>(action_history >> 3 & 7ULL);
            if (card1 == card2) {
                *proba_list++ = 50;
                *proba_list++ = 50;
            } else {
                *proba_list++ = 20 + (card1 != JACK && card2 != JACK) * 20;
                *proba_list++ = 20 + (card1 != QUEEN && card2 != QUEEN) * 20;
                *proba_list++ = 20 + (card1 != KING && card2 != KING) * 20;
            }
        }
        probas.last = proba_list;
    }
    friend std::ostream& operator<<(std::ostream& os, const LeducNoRaise& leduc) {
        auto h = leduc.action_history;
        for (uint64_t i = 0; i < leduc.nb_plies; i++) {
            os << Action(h & 7) << ' ';
            h >>= 3;
        }
        return os;
    }
    friend std::ostream& operator<<(std::ostream& os, const LeducNoRaise::Action& action) {
        static const char* repr[] = {
            "k",
            "b",
            "c",
            "f",
            "J",
            "Q",
            "K",
            "END",
        };
        os << repr[action];
        return os;
    }
    friend std::ostream& operator<<(std::ostream& os, const LeducNoRaise::InfoSet& info_set) {
        uint64_t i = info_set;
        int nb_plies = i >> 32;        
        //os << '#' << nb_plies;
        if (nb_plies == 0) return os;
        if ((i & 7) != 0) {
            os << ' ' << Action(i & 7ULL);
            i >>= 3;
        } else {
            i >>= 3;
            os << ' ' << Action(i & 7ULL);
        }
        os << ' ';        
        for (int j = 0; j < nb_plies - 2; j++) {            
            i >>= 3;
            os << Action(i & 7ULL);
        }
        return os;
    }
};

#endif
