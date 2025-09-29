#ifndef LEDUC_H
#define LEDUC_H

#include "game.h"
#include "list.h"
#include "misc.h"
#include <chrono>
#include <iostream>

struct Leduc {
    enum Action {
        CHECK = 0,
        BET   = 1,
        CALL  = 2,
        FOLD  = 3,
        JACK  = 4,
        QUEEN = 5,
        KING  = 6,
        RAISE = 7,        
        END = 8,
    };
    using State = uint64_t;
    enum InfoSet : uint64_t {};
    static constexpr int MAX_NB_PLAYER_ACTIONS = 3;
    static constexpr int MAX_NB_CHANCE_ACTIONS = 3;  
    static constexpr int MAX_NB_ACTIONS = std::max(MAX_NB_PLAYER_ACTIONS, MAX_NB_CHANCE_ACTIONS);    
    uint64_t action_history = 0;
    uint64_t nb_plies = 0;
    mutable PRNG prng{static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
    void reset() {
        action_history = 0;
        nb_plies = 0;
    }
    State get_state() const {
        return nb_plies << 48 | action_history;
    }
    void set_state(State state) {
        action_history = state & 0xFFFFFFFFFFFFULL;
        nb_plies       = state >> 48;
    }
    InfoSet get_info_set(int player) const {
        constexpr uint64_t masks[] = { 0xFFFFFFFFFF0FULL, 0xFFFFFFFFFFF0ULL, 0xFFFFFFFFFFFFULL };        
        return InfoSet((nb_plies << 48) | (masks[player] & action_history));
    }
    void play(Action a) {
        action_history |= static_cast<uint64_t>(a) << nb_plies * 4;
        ++nb_plies;
    }
    void undo(Action a) {
        --nb_plies;
        action_history &= ~(0xFULL << nb_plies * 4);
    }
    int current_player() const {
        static const int player[3][11] = {
            { CHANCE, CHANCE, PLAYER1, PLAYER2, CHANCE, PLAYER1, PLAYER2, PLAYER1, PLAYER2, PLAYER1, PLAYER2 },
            { CHANCE, CHANCE, PLAYER1, PLAYER2, PLAYER1, CHANCE, PLAYER1, PLAYER2, PLAYER1, PLAYER2, PLAYER1 },
            { CHANCE, CHANCE, PLAYER1, PLAYER2, PLAYER1, PLAYER2, CHANCE, PLAYER1, PLAYER2, PLAYER1, PLAYER2 }
        };
        return player[get_action(4) == RAISE ? 2 : (get_action(3) == BET || get_action(3) == RAISE ? 1 : 0)][nb_plies];
    }
    Action get_action(int i) const {
        return Action((action_history & (0xFULL << i * 4)) >> i * 4);
    }
    bool game_over() const {
        if (nb_plies <= 2) return false;
        Action last_action = get_action(nb_plies - 1);
        if (last_action == FOLD) return true;
        Action before_last_action = get_action(nb_plies - 2);        
        bool is_chance = last_action == JACK || before_last_action == JACK 
                        || last_action == QUEEN || before_last_action == QUEEN 
                        || last_action == KING || before_last_action == KING;
        return nb_plies >= 7 && !is_chance && (last_action == CHECK || last_action == CALL);
    }
    bool is_chance_player() const {
        return current_player() == CHANCE;
    }
    static constexpr Action ACTIONS[] {
        JACK, QUEEN, KING, END,
        CHECK, BET, END, FOLD, CALL, RAISE, END, FOLD, CALL, END,
        // 14
        QUEEN, KING, END,
        JACK, KING, END,
        JACK, QUEEN, END,
        // 23
        JACK, QUEEN, QUEEN, KING, KING,
        JACK, JACK, QUEEN, KING, KING,
        JACK, JACK, QUEEN, QUEEN, KING,
        // 38
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
        0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 14, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 20
    };
    int payoff(int player) const {        
        int pot = 1;
        int ply = 0;
        Action card_p1 = get_action(ply++);
        Action card_p2 = get_action(ply++);
        Action r1_a1_p1 = get_action(ply++);
        Action r1_a1_p2 = get_action(ply++);
        if (r1_a1_p1 == BET) {
            if (r1_a1_p2 == FOLD) return (player == PLAYER1 ? pot : -pot);
            pot += 2;
            if (r1_a1_p2 == RAISE) {
                Action r1_a2_p1 = get_action(ply++);
                if (r1_a2_p1 == FOLD) return (player == PLAYER2 ? pot : -pot);
                pot += 2;
            }
        } else {
            if (r1_a1_p2 == BET) {
                Action r1_a2_p1 = get_action(ply++);
                if (r1_a2_p1 == FOLD) return (player == PLAYER2 ? pot : -pot);
                pot += 2;
                if (r1_a2_p1 == RAISE) {
                    Action r1_a2_p2 = get_action(ply++);
                    if (r1_a2_p2 == FOLD) return (player == PLAYER1 ? pot : -pot);
                    pot += 2;
                }
            } 
        }
        Action community_card = get_action(ply++);
        Action r2_a1_p1 = get_action(ply++);
        Action r2_a1_p2 = get_action(ply++);
        if (r2_a1_p1 == BET) {
            if (r2_a1_p2 == FOLD) return (player == PLAYER1 ? pot : -pot);
            pot += 4;
            if (r2_a1_p2 == RAISE) {
                Action r2_a2_p1 = get_action(ply++);
                if (r2_a2_p1 == FOLD) return (player == PLAYER2 ? pot : -pot);
                pot += 4;
            }
        } else {
            if (r2_a1_p2 == BET) {
                Action r2_a2_p1 = get_action(ply++);
                if (r2_a2_p1 == FOLD) return (player == PLAYER2 ? pot : -pot);
                pot += 4;
                if (r2_a2_p1 == RAISE) {
                    Action r2_a2_p2 = get_action(ply++);
                    if (r2_a2_p2 == FOLD) return (player == PLAYER1 ? pot : -pot);
                    pot += 4;
                }
            } 
        }
        int p1_rank = (card_p1 == community_card) ? (100 + card_p1) : card_p1;
        int p2_rank = (card_p2 == community_card) ? (100 + card_p2) : card_p2;
        int winner_payoff = (p1_rank > p2_rank) ? pot : ((p1_rank < p2_rank) ? -pot : 0);
        return (player == PLAYER1) ? winner_payoff : -winner_payoff;
    }
    Action sample_action() const {
        if (nb_plies == 0) {
            return Action(JACK + prng.rand<uint32_t>() % 3); 
        } else if (nb_plies == 1) {
            int card = get_action(0);
            return ACTIONS[23 + (card - JACK) * 5 + prng.rand<uint32_t>() % 5];
        } else {
            int card1 = get_action(0) - 3;
            int card2 = get_action(1) - 3;
            int idx = 38 + (card1 * card2 - 1) * 4 + prng.rand<uint32_t>() % 4;
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
                start = DELTAS[2 + ((action_history & 0xFULL) - 1) * ((action_history >> 4 & 0xFULL) - 1)];
                //std::cout << 2 + ((action_history & 0xFULL) - 1) * ((action_history >> 4 & 0xFULL) - 1) << " start: " << start << '\n';
            } else {
                static constexpr int D[] = { 0, 3, 0, 0, 0, 0, 0, 7 };
                Action last_action = get_action(nb_plies - 1);
                start += D[last_action];
            }
        }
        const Action* moves = ACTIONS + start;
        // std::cout << '#' << nb_plies << '\n';
        // std::cout << start << '\n';
        // if (current_player() == CHANCE) std::cout << "CHANCE" << '\n';
        for (Action a = *moves; a != END; a = *++moves) {
            //std::cout << a << ' ';
            *action_list++ = a;
        }
        //std::cout << '\n';
        actions.last = action_list;
    }
    void probas(List<int, MAX_NB_CHANCE_ACTIONS>& probas) {
        int* proba_list = probas.list;
        if (nb_plies < 2) {
            Action card = static_cast<Action>(action_history & 0xFULL);
            *proba_list++ = 20 + (nb_plies == 1) * (card != JACK) * 20;
            *proba_list++ = 20 + (nb_plies == 1) * (card != QUEEN) * 20;
            *proba_list++ = 20 + (nb_plies == 1) * (card != KING) * 20;
        } else {
            Action card1 = static_cast<Action>(action_history & 0xFULL);
            Action card2 = static_cast<Action>(action_history >> 4 & 0xFULL);
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
    friend std::ostream& operator<<(std::ostream& os, const Leduc& leduc) {
        auto h = leduc.action_history;
        for (uint64_t i = 0; i < leduc.nb_plies; i++) {
            os << Action(h & 0xFULL) << ' ';
            h >>= 4;
        }
        return os;
    }
    friend std::ostream& operator<<(std::ostream& os, const Leduc::Action& action) {
        static const char* repr[] = {
            "k",
            "b",
            "c",
            "f",
            "J",
            "Q",
            "K",
            "r",
            "END",
        };
        os << repr[action];
        return os;
    }
    friend std::ostream& operator<<(std::ostream& os, const Leduc::InfoSet& info_set) {
        uint64_t i = info_set;
        //std::cout << i << ' ';
        int nb_plies = i >> 48;        
        //os << '#' << nb_plies;
        if (nb_plies == 0) return os;
        if ((i & 0xFULL) != 0) {
            os << ' ' << Action(i & 0xFULL);
            i >>= 4;
        } else {
            i >>= 4;
            os << ' ' << Action(i & 0xFULL);
        }
        os << ' ';        
        for (int j = 0; j < nb_plies - 2; j++) {            
            i >>= 4;
            os << Action(i & 0xFULL);
        }
        return os;
    }
};

#endif
