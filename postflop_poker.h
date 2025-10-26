#ifndef POSTFLOP_POKER_H
#define POSTFLOP_POKER_H

#include "game.h"
#include "list.h"
#include "misc.h"
#include <chrono>
#include <iostream>
#include <array>
#include <vector>
#include <set>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <cstring>
#include <numeric>

/*
 * Texas Hold'em Postflop Solver
 *
 * Features:
 * - Fixed flop (user-specified)
 * - Initial chance node deals hand combinations from ranges
 * - Turn and river are dealt randomly (chance nodes)
 * - Supports flexible betting actions
 * - Compatible with both CFR and MCCFR
 *
 * State encoding:
 * - Bits 0-5: Player 1 hole card 1 (0-51)
 * - Bits 6-11: Player 1 hole card 2 (0-51)
 * - Bits 12-17: Player 2 hole card 1 (0-51)
 * - Bits 18-23: Player 2 hole card 2 (0-51)
 * - Bits 24-29: Turn card (0-51, 63=not dealt)
 * - Bits 30-35: River card (0-51, 63=not dealt)
 * - Bits 36-39: Current street (0=deal, 1=flop, 2=turn_deal, 3=turn_bet, 4=river_deal, 5=river_bet)
 * - Bits 40+: Action history (4 bits per action)
 */

// Card evaluation helper
struct PokerCard {
    static constexpr uint32_t primes[13] = {
        2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41
    };

    static uint32_t make_card(int rank, int suit) {
        uint32_t rank_bit = 1 << (rank + 16);
        uint32_t suit_bit = 1 << (suit + 12);
        uint32_t rank_value = rank << 8;
        uint32_t prime = primes[rank];
        return rank_bit | suit_bit | rank_value | prime;
    }

    static int get_rank(uint32_t card) { return (card >> 8) & 0xF; }
    static uint32_t get_prime(uint32_t card) { return card & 0xFF; }
    static bool is_flush(uint32_t c1, uint32_t c2, uint32_t c3, uint32_t c4, uint32_t c5) {
        return ((c1 & c2 & c3 & c4 & c5) & 0xF000) != 0;
    }
};

// Simple hand evaluator for postflop poker
class PostflopHandEvaluator {
private:
    std::array<uint32_t, 52> deck;
    std::array<uint16_t, 8192> flush_table;
    std::array<uint16_t, 49205> unique_table;

    void initialize_deck() {
        for (int rank = 0; rank < 13; ++rank) {
            for (int suit = 0; suit < 4; ++suit) {
                deck[rank + suit * 13] = PokerCard::make_card(rank, suit);
            }
        }
    }

    inline uint16_t evaluate5_scalar(uint32_t c1, uint32_t c2, uint32_t c3,
                                     uint32_t c4, uint32_t c5) const {
        if (PokerCard::is_flush(c1, c2, c3, c4, c5)) {
            uint32_t rank_bits = ((c1 | c2 | c3 | c4 | c5) >> 16) & 0x1FFF;
            return flush_table[rank_bits];
        }

        uint64_t product = 1ULL * PokerCard::get_prime(c1) * PokerCard::get_prime(c2) *
                          PokerCard::get_prime(c3) * PokerCard::get_prime(c4) * PokerCard::get_prime(c5);
        return unique_table[product % 49205];
    }

public:
    PostflopHandEvaluator() {
        initialize_deck();
        flush_table.fill(7462);
        unique_table.fill(7462);
        load_tables("poker_tables.bin");
    }

    void load_tables(const char* filename) {
        std::ifstream in(filename, std::ios::binary);
        if (!in) {
            std::cerr << "Warning: poker_tables.bin not found. Hand evaluation will not work correctly.\n";
            return;
        }
        in.read(reinterpret_cast<char*>(flush_table.data()),
               flush_table.size() * sizeof(uint16_t));
        in.read(reinterpret_cast<char*>(unique_table.data()),
               unique_table.size() * sizeof(uint16_t));
    }

    uint16_t evaluate7(const uint8_t* cards) const {
        uint32_t c[7];
        for (int i = 0; i < 7; ++i) {
            c[i] = deck[cards[i]];
        }

        uint16_t best = 7462;
        best = std::min(best, evaluate5_scalar(c[0], c[1], c[2], c[3], c[4]));
        best = std::min(best, evaluate5_scalar(c[0], c[1], c[2], c[3], c[5]));
        best = std::min(best, evaluate5_scalar(c[0], c[1], c[2], c[3], c[6]));
        best = std::min(best, evaluate5_scalar(c[0], c[1], c[2], c[4], c[5]));
        best = std::min(best, evaluate5_scalar(c[0], c[1], c[2], c[4], c[6]));
        best = std::min(best, evaluate5_scalar(c[0], c[1], c[2], c[5], c[6]));
        best = std::min(best, evaluate5_scalar(c[0], c[1], c[3], c[4], c[5]));
        best = std::min(best, evaluate5_scalar(c[0], c[1], c[3], c[4], c[6]));
        best = std::min(best, evaluate5_scalar(c[0], c[1], c[3], c[5], c[6]));
        best = std::min(best, evaluate5_scalar(c[0], c[1], c[4], c[5], c[6]));
        best = std::min(best, evaluate5_scalar(c[0], c[2], c[3], c[4], c[5]));
        best = std::min(best, evaluate5_scalar(c[0], c[2], c[3], c[4], c[6]));
        best = std::min(best, evaluate5_scalar(c[0], c[2], c[3], c[5], c[6]));
        best = std::min(best, evaluate5_scalar(c[0], c[2], c[4], c[5], c[6]));
        best = std::min(best, evaluate5_scalar(c[0], c[3], c[4], c[5], c[6]));
        best = std::min(best, evaluate5_scalar(c[1], c[2], c[3], c[4], c[5]));
        best = std::min(best, evaluate5_scalar(c[1], c[2], c[3], c[4], c[6]));
        best = std::min(best, evaluate5_scalar(c[1], c[2], c[3], c[5], c[6]));
        best = std::min(best, evaluate5_scalar(c[1], c[2], c[4], c[5], c[6]));
        best = std::min(best, evaluate5_scalar(c[1], c[3], c[4], c[5], c[6]));
        best = std::min(best, evaluate5_scalar(c[2], c[3], c[4], c[5], c[6]));

        return best;
    }

    uint16_t evaluate5(const uint8_t* cards) const {
        uint32_t c[5];
        for (int i = 0; i < 5; ++i) {
            c[i] = deck[cards[i]];
        }
        return evaluate5_scalar(c[0], c[1], c[2], c[3], c[4]);
    }
};

struct PostflopPoker {
    // Card representation: 0-51 (rank 0-12, suit 0-3)
    // rank: 0=2, 1=3, ..., 12=A
    // suit: 0=clubs, 1=diamonds, 2=hearts, 3=spades
    static constexpr int INVALID_CARD = 63;

    // Static hand evaluator instance (shared across all games)
    static PostflopHandEvaluator& get_evaluator() {
        static PostflopHandEvaluator evaluator;
        return evaluator;
    }

    enum Action : uint8_t {
        // Betting actions
        FOLD = 0,
        CHECK = 1,
        CALL = 2,
        BET_HALF_POT = 3,
        BET_POT = 4,
        RAISE_POT = 5,
        ALL_IN = 6,

        // Hand dealing actions (for initial range sampling)
        DEAL_HANDS_START = 7,  // Actions 7+ are for dealing specific hand combos

        END = 255
    };

    enum Street : uint8_t {
        INITIAL_DEAL = 0,
        FLOP_BETTING = 1,
        TURN_DEAL = 2,
        TURN_BETTING = 3,
        RIVER_DEAL = 4,
        RIVER_BETTING = 5,
        SHOWDOWN = 6
    };

    using State = uint64_t;
    using InfoSet = uint64_t;

    static constexpr int MAX_NB_PLAYER_ACTIONS = 6;
    static constexpr int MAX_NB_CHANCE_ACTIONS = 45;  // Max cards that can be dealt
    static constexpr int MAX_NB_ACTIONS = std::max(MAX_NB_PLAYER_ACTIONS, MAX_NB_CHANCE_ACTIONS);

    // Configuration
    std::array<uint8_t, 3> flop_cards;  // Fixed flop
    std::vector<std::pair<std::array<uint8_t, 2>, double>> range_p1;  // Hole cards -> probability
    std::vector<std::pair<std::array<uint8_t, 2>, double>> range_p2;

    int starting_pot;
    int starting_stack_p1;
    int starting_stack_p2;

    // Current game state
    uint8_t p1_hole[2];
    uint8_t p2_hole[2];
    uint8_t turn_card;
    uint8_t river_card;
    Street street;

    int pot;
    int stack_p1;
    int stack_p2;
    int bet_this_street;
    int to_call;

    uint64_t action_history;  // Stores betting actions (4 bits each)
    int num_actions;

    mutable PRNG prng{static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    )};

    // Default constructor (for static helper functions)
    PostflopPoker() : flop_cards{0, 0, 0}, starting_pot(20),
                      starting_stack_p1(100), starting_stack_p2(100) {
        reset();
    }

    // Constructor for solver (with ranges)
    PostflopPoker(
        const std::array<uint8_t, 3>& flop,
        const std::vector<std::pair<std::array<uint8_t, 2>, double>>& r1,
        const std::vector<std::pair<std::array<uint8_t, 2>, double>>& r2,
        int starting_pot = 20,
        int starting_stack = 100
    ) : flop_cards(flop), range_p1(r1), range_p2(r2),
        starting_pot(starting_pot),
        starting_stack_p1(starting_stack),
        starting_stack_p2(starting_stack) {
        reset();
    }

    // Constructor for single hand analysis
    PostflopPoker(
        const std::array<uint8_t, 3>& flop,
        const std::array<uint8_t, 2>& h1,
        const std::array<uint8_t, 2>& h2,
        int starting_pot = 20,
        int starting_stack = 100
    ) : flop_cards(flop),
        starting_pot(starting_pot),
        starting_stack_p1(starting_stack),
        starting_stack_p2(starting_stack) {
        p1_hole[0] = h1[0];
        p1_hole[1] = h1[1];
        p2_hole[0] = h2[0];
        p2_hole[1] = h2[1];
        street = FLOP_BETTING;
        pot = starting_pot;
        stack_p1 = starting_stack;
        stack_p2 = starting_stack;
        turn_card = INVALID_CARD;
        river_card = INVALID_CARD;
        bet_this_street = 0;
        to_call = 0;
        action_history = 0;
        num_actions = 0;
    }

    void reset() {
        street = INITIAL_DEAL;
        pot = starting_pot;
        stack_p1 = starting_stack_p1;
        stack_p2 = starting_stack_p2;
        turn_card = INVALID_CARD;
        river_card = INVALID_CARD;
        bet_this_street = 0;
        to_call = 0;
        action_history = 0;
        num_actions = 0;
        p1_hole[0] = p1_hole[1] = INVALID_CARD;
        p2_hole[0] = p2_hole[1] = INVALID_CARD;
    }

    State get_state() const {
        State state = 0;
        state |= static_cast<uint64_t>(p1_hole[0]) << 0;
        state |= static_cast<uint64_t>(p1_hole[1]) << 6;
        state |= static_cast<uint64_t>(p2_hole[0]) << 12;
        state |= static_cast<uint64_t>(p2_hole[1]) << 18;
        state |= static_cast<uint64_t>(turn_card) << 24;
        state |= static_cast<uint64_t>(river_card) << 30;
        state |= static_cast<uint64_t>(street) << 36;
        state |= action_history << 40;
        return state;
    }

    void set_state(State state) {
        p1_hole[0] = (state >> 0) & 0x3F;
        p1_hole[1] = (state >> 6) & 0x3F;
        p2_hole[0] = (state >> 12) & 0x3F;
        p2_hole[1] = (state >> 18) & 0x3F;
        turn_card = (state >> 24) & 0x3F;
        river_card = (state >> 30) & 0x3F;
        street = static_cast<Street>((state >> 36) & 0xF);
        action_history = state >> 40;
        // Reconstruct pot, stacks from history (simplified)
        pot = starting_pot;
        stack_p1 = starting_stack_p1;
        stack_p2 = starting_stack_p2;
    }

    InfoSet get_info_set(int player) const {
        // Info set includes: own hole cards, board, action history
        // Does NOT include opponent's hole cards
        InfoSet info = 0;

        if (player == PLAYER1) {
            info |= static_cast<uint64_t>(p1_hole[0]) << 0;
            info |= static_cast<uint64_t>(p1_hole[1]) << 6;
        } else {
            info |= static_cast<uint64_t>(p2_hole[0]) << 0;
            info |= static_cast<uint64_t>(p2_hole[1]) << 6;
        }

        // Board cards (visible to both)
        info |= static_cast<uint64_t>(flop_cards[0]) << 12;
        info |= static_cast<uint64_t>(flop_cards[1]) << 18;
        info |= static_cast<uint64_t>(flop_cards[2]) << 24;
        info |= static_cast<uint64_t>(turn_card) << 30;
        info |= static_cast<uint64_t>(river_card) << 36;

        // Street and action history
        info |= static_cast<uint64_t>(street) << 42;
        info |= action_history << 46;

        return info;
    }

    bool game_over() const {
        return street == SHOWDOWN;
    }

    bool is_chance_player() const {
        return street == INITIAL_DEAL || street == TURN_DEAL || street == RIVER_DEAL;
    }

    int current_player() const {
        if (is_chance_player()) return CHANCE;
        if (game_over()) return CHANCE;

        // Determine from action history who acts next
        if (num_actions == 0) {
            return PLAYER1;  // First to act on each street
        }

        Action last_action = get_last_action();

        // If last action was fold, game is over
        if (last_action == FOLD) {
            return CHANCE;
        }

        // If both players checked, move to next street
        if (num_actions >= 2 && last_action == CHECK && to_call == 0) {
            Action prev_action = static_cast<Action>((action_history >> ((num_actions - 2) * 4)) & 0xF);
            if (prev_action == CHECK) {
                return CHANCE;  // Both checked, street is over
            }
        }

        // If last action was call, street is over
        if (last_action == CALL) {
            return CHANCE;
        }

        // Otherwise, alternate players
        return (num_actions % 2 == 0) ? PLAYER1 : PLAYER2;
    }

    void play(Action a) {
        if (street == INITIAL_DEAL) {
            // Deal hands from range
            // Action encodes which hand combo (simplified for now)
            // In practice, you'd map action to specific hand combination
            if (!range_p1.empty() && !range_p2.empty()) {
                // Sample from ranges (simplified)
                int combo_idx = a - DEAL_HANDS_START;
                if (combo_idx < range_p1.size() * range_p2.size()) {
                    int p1_idx = combo_idx / range_p2.size();
                    int p2_idx = combo_idx % range_p2.size();
                    p1_hole[0] = range_p1[p1_idx].first[0];
                    p1_hole[1] = range_p1[p1_idx].first[1];
                    p2_hole[0] = range_p2[p2_idx].first[0];
                    p2_hole[1] = range_p2[p2_idx].first[1];
                }
            }
            street = FLOP_BETTING;
            bet_this_street = 0;
            to_call = 0;
        }
        else if (street == TURN_DEAL) {
            turn_card = static_cast<uint8_t>(a);
            street = TURN_BETTING;
            bet_this_street = 0;
            to_call = 0;
        }
        else if (street == RIVER_DEAL) {
            river_card = static_cast<uint8_t>(a);
            street = RIVER_BETTING;
            bet_this_street = 0;
            to_call = 0;
        }
        else {
            // Betting action
            int current = current_player();  // Get current player BEFORE incrementing

            action_history |= static_cast<uint64_t>(a) << (num_actions * 4);
            num_actions++;

            switch (a) {
                case FOLD:
                    street = SHOWDOWN;
                    break;

                case CHECK:
                    if (to_call == 0) {
                        // Both checked, move to next street
                        if (num_actions >= 2 && get_last_action() == CHECK) {
                            advance_street();
                        }
                    }
                    break;

                case CALL:
                    if (current == PLAYER1) {
                        pot += to_call;
                        stack_p1 -= to_call;
                    } else {
                        pot += to_call;
                        stack_p2 -= to_call;
                    }
                    to_call = 0;
                    advance_street();
                    break;

                case BET_HALF_POT: {
                    int bet_size = pot / 2;
                    if (current == PLAYER1) {
                        pot += bet_size;
                        stack_p1 -= bet_size;
                    } else {
                        pot += bet_size;
                        stack_p2 -= bet_size;
                    }
                    bet_this_street = bet_size;
                    to_call = bet_size;
                    break;
                }

                case BET_POT: {
                    int bet_size = pot;
                    if (current == PLAYER1) {
                        pot += bet_size;
                        stack_p1 -= bet_size;
                    } else {
                        pot += bet_size;
                        stack_p2 -= bet_size;
                    }
                    bet_this_street = bet_size;
                    to_call = bet_size;
                    break;
                }

                case RAISE_POT: {
                    int raise_size = pot + to_call;
                    if (current == PLAYER1) {
                        pot += raise_size;
                        stack_p1 -= raise_size;
                    } else {
                        pot += raise_size;
                        stack_p2 -= raise_size;
                    }
                    bet_this_street += raise_size;
                    to_call = bet_this_street;
                    break;
                }

                case ALL_IN: {
                    int bet_size = (current == PLAYER1) ? stack_p1 : stack_p2;
                    pot += bet_size;
                    if (current == PLAYER1) {
                        stack_p1 = 0;
                    } else {
                        stack_p2 = 0;
                    }
                    to_call = bet_size - to_call;
                    break;
                }
            }
        }
    }

    void undo(Action a) {
        // IMPORTANT: For proper game tree traversal with postflop poker,
        // use get_state() before play() and set_state() to restore.
        // This undo is a simplified version that doesn't restore pot/stacks correctly.

        if (street == SHOWDOWN) {
            // Undo showdown - go back to last betting street
            street = RIVER_BETTING;
        } else if (street == RIVER_BETTING) {
            // Check if we're undoing a deal or a bet
            if (num_actions > 0) {
                num_actions--;
                action_history &= ~(0xFULL << (num_actions * 4));
            } else {
                // Undoing river deal
                street = RIVER_DEAL;
                river_card = INVALID_CARD;
            }
        } else if (street == RIVER_DEAL) {
            street = TURN_BETTING;
        } else if (street == TURN_BETTING) {
            if (num_actions > 0) {
                num_actions--;
                action_history &= ~(0xFULL << (num_actions * 4));
            } else {
                // Undoing turn deal
                street = TURN_DEAL;
                turn_card = INVALID_CARD;
            }
        } else if (street == TURN_DEAL) {
            street = FLOP_BETTING;
        } else if (street == FLOP_BETTING) {
            if (num_actions > 0) {
                num_actions--;
                action_history &= ~(0xFULL << (num_actions * 4));
            } else {
                // Undoing initial deal
                street = INITIAL_DEAL;
            }
        }
    }

    Action get_last_action() const {
        if (num_actions == 0) return CHECK;
        return static_cast<Action>((action_history >> ((num_actions - 1) * 4)) & 0xF);
    }

    void advance_street() {
        switch (street) {
            case FLOP_BETTING:
                street = TURN_DEAL;
                break;
            case TURN_BETTING:
                street = RIVER_DEAL;
                break;
            case RIVER_BETTING:
                street = SHOWDOWN;
                break;
            default:
                break;
        }
        bet_this_street = 0;
        to_call = 0;
        num_actions = 0;
    }

    bool cards_conflict(const std::vector<uint8_t>& cards) const {
        std::set<uint8_t> card_set(cards.begin(), cards.end());
        return card_set.size() != cards.size();
    }

    template<int SIZE>
    void actions(List<Action, SIZE>& actions) {
        Action* action_list = actions.list;

        if (street == INITIAL_DEAL) {
            // Enumerate all valid hand combinations from ranges
            for (size_t i = 0; i < range_p1.size(); i++) {
                for (size_t j = 0; j < range_p2.size(); j++) {
                    // Check no card conflicts
                    std::vector<uint8_t> all_cards = {
                        flop_cards[0], flop_cards[1], flop_cards[2],
                        range_p1[i].first[0], range_p1[i].first[1],
                        range_p2[j].first[0], range_p2[j].first[1]
                    };
                    if (!cards_conflict(all_cards)) {
                        *action_list++ = static_cast<Action>(DEAL_HANDS_START + i * range_p2.size() + j);
                    }
                }
            }
        }
        else if (street == TURN_DEAL || street == RIVER_DEAL) {
            // Deal all available cards
            std::set<uint8_t> used;
            used.insert(flop_cards.begin(), flop_cards.end());
            used.insert(p1_hole[0]);
            used.insert(p1_hole[1]);
            used.insert(p2_hole[0]);
            used.insert(p2_hole[1]);
            if (turn_card != INVALID_CARD) used.insert(turn_card);

            for (int c = 0; c < 52; c++) {
                if (!used.count(c)) {
                    *action_list++ = static_cast<Action>(c);
                }
            }
        }
        else {
            // Betting actions
            if (to_call > 0) {
                // Facing a bet
                *action_list++ = FOLD;
                *action_list++ = CALL;
                if (stack_p1 > to_call && stack_p2 > to_call) {
                    *action_list++ = RAISE_POT;
                }
                *action_list++ = ALL_IN;
            } else {
                // No bet facing
                *action_list++ = CHECK;
                if (stack_p1 > 0 && stack_p2 > 0) {
                    *action_list++ = BET_HALF_POT;
                    *action_list++ = BET_POT;
                }
                *action_list++ = ALL_IN;
            }
        }

        actions.last = action_list;
    }

    void probas(List<int, MAX_NB_CHANCE_ACTIONS>& probas) {
        int* proba_list = probas.list;

        if (street == INITIAL_DEAL) {
            // Probabilities from ranges
            for (size_t i = 0; i < range_p1.size(); i++) {
                for (size_t j = 0; j < range_p2.size(); j++) {
                    std::vector<uint8_t> all_cards = {
                        flop_cards[0], flop_cards[1], flop_cards[2],
                        range_p1[i].first[0], range_p1[i].first[1],
                        range_p2[j].first[0], range_p2[j].first[1]
                    };
                    if (!cards_conflict(all_cards)) {
                        double prob = range_p1[i].second * range_p2[j].second;
                        *proba_list++ = static_cast<int>(prob * 1000000);
                    }
                }
            }
        }
        else if (street == TURN_DEAL || street == RIVER_DEAL) {
            // Uniform over remaining cards
            std::set<uint8_t> used;
            used.insert(flop_cards.begin(), flop_cards.end());
            used.insert(p1_hole[0]);
            used.insert(p1_hole[1]);
            used.insert(p2_hole[0]);
            used.insert(p2_hole[1]);
            if (turn_card != INVALID_CARD) used.insert(turn_card);

            int num_remaining = 52 - used.size();
            int prob_each = 1000000 / num_remaining;

            for (int c = 0; c < 52; c++) {
                if (!used.count(c)) {
                    *proba_list++ = prob_each;
                }
            }
        }

        probas.last = proba_list;
    }

    Action sample_action() const {
        if (street == INITIAL_DEAL) {
            // Sample from ranges
            // Simplified: uniform random
            if (range_p1.empty() || range_p2.empty()) return DEAL_HANDS_START;

            int idx = prng.rand<uint32_t>() % (range_p1.size() * range_p2.size());
            return static_cast<Action>(DEAL_HANDS_START + idx);
        }
        else if (street == TURN_DEAL || street == RIVER_DEAL) {
            // Sample random available card
            std::set<uint8_t> used;
            used.insert(flop_cards.begin(), flop_cards.end());
            used.insert(p1_hole[0]);
            used.insert(p1_hole[1]);
            used.insert(p2_hole[0]);
            used.insert(p2_hole[1]);
            if (turn_card != INVALID_CARD) used.insert(turn_card);

            std::vector<uint8_t> available;
            for (int c = 0; c < 52; c++) {
                if (!used.count(c)) available.push_back(c);
            }

            if (available.empty()) return static_cast<Action>(0);
            return static_cast<Action>(available[prng.rand<uint32_t>() % available.size()]);
        }

        return CHECK;  // Default betting action
    }

    int payoff(int player) const {
        // Returns chips won/lost for the player
        // Assumes symmetric contributions (starting_pot / 2 each)

        if (get_last_action() == FOLD) {
            // Someone folded
            int folder = (num_actions % 2 == 0) ? PLAYER2 : PLAYER1;
            int my_contribution = starting_pot / 2 + (starting_stack_p1 - stack_p1);
            if (player == folder) {
                return -my_contribution;  // Lost my contribution
            } else {
                return pot - my_contribution;  // Won the pot minus my contribution
            }
        }

        // Showdown - use hand evaluation
        int winner = evaluate_showdown();

        // Calculate what this player contributed
        int my_contribution = starting_pot / 2;
        if (player == PLAYER1) {
            my_contribution += (starting_stack_p1 - stack_p1);
        } else {
            my_contribution += (starting_stack_p2 - stack_p2);
        }

        if (winner == -1) {
            // Tie - split pot
            return pot / 2 - my_contribution;
        } else if (player == winner) {
            return pot - my_contribution;
        } else {
            return -my_contribution;
        }
    }

    int evaluate_showdown() const {
        // Returns PLAYER1 (0), PLAYER2 (1), or -1 for tie
        // Need at least 5 cards (flop) to evaluate
        if (street < FLOP_BETTING) return 0;

        uint8_t p1_cards[7], p2_cards[7];
        int num_board_cards = 3;  // Start with flop

        // Flop cards
        p1_cards[0] = p2_cards[0] = flop_cards[0];
        p1_cards[1] = p2_cards[1] = flop_cards[1];
        p1_cards[2] = p2_cards[2] = flop_cards[2];

        // Turn card
        if (turn_card != INVALID_CARD) {
            p1_cards[num_board_cards] = p2_cards[num_board_cards] = turn_card;
            num_board_cards++;
        }

        // River card
        if (river_card != INVALID_CARD) {
            p1_cards[num_board_cards] = p2_cards[num_board_cards] = river_card;
            num_board_cards++;
        }

        // Add hole cards
        p1_cards[num_board_cards] = p1_hole[0];
        p1_cards[num_board_cards + 1] = p1_hole[1];
        p2_cards[num_board_cards] = p2_hole[0];
        p2_cards[num_board_cards + 1] = p2_hole[1];

        int total_cards = num_board_cards + 2;

        // Debug output
        #ifdef DEBUG_SHOWDOWN
        std::cout << "P1 cards: ";
        for (int i = 0; i < total_cards; i++) std::cout << (int)p1_cards[i] << " ";
        std::cout << "\nP2 cards: ";
        for (int i = 0; i < total_cards; i++) std::cout << (int)p2_cards[i] << " ";
        std::cout << "\n";
        #endif

        if (total_cards == 7) {
            // Full 7-card evaluation (flop + turn + river + 2 hole cards)
            uint16_t p1_rank = get_evaluator().evaluate7(p1_cards);
            uint16_t p2_rank = get_evaluator().evaluate7(p2_cards);

            #ifdef DEBUG_SHOWDOWN
            std::cout << "P1 rank: " << p1_rank << ", P2 rank: " << p2_rank << "\n";
            #endif

            if (p1_rank < p2_rank) return PLAYER1;  // Lower rank is better
            if (p2_rank < p1_rank) return PLAYER2;
            return -1;  // Tie
        } else if (total_cards == 6) {
            // 6 cards (flop + turn + 2 hole cards)
            // Use 5-card evaluation with first 5 cards
            uint16_t p1_rank = get_evaluator().evaluate5(p1_cards);
            uint16_t p2_rank = get_evaluator().evaluate5(p2_cards);

            if (p1_rank < p2_rank) return PLAYER1;
            if (p2_rank < p1_rank) return PLAYER2;
            return -1;  // Tie
        } else {
            // 5 cards (flop + 2 hole cards)
            uint16_t p1_rank = get_evaluator().evaluate5(p1_cards);
            uint16_t p2_rank = get_evaluator().evaluate5(p2_cards);

            if (p1_rank < p2_rank) return PLAYER1;
            if (p2_rank < p1_rank) return PLAYER2;
            return -1;  // Tie
        }
    }

    // Helpers for best response calculation
    static std::vector<std::pair<InfoSet, Action>> info_sets_and_actions(State state, int player) {
        // Returns all (infoset, action) pairs for the given player on the path to this state
        std::vector<std::pair<InfoSet, Action>> res;

        PostflopPoker game;
        game.set_state(state);

        // Extract street from state
        Street street = static_cast<Street>((state >> 36) & 0xF);

        // If game hasn't started yet, no info sets
        if (street == INITIAL_DEAL) return res;

        // We need to replay the game to find all decision points for the player
        // For now, simplified: just add the current info set if it's the player's turn
        // A full implementation would need to track all decision points

        // TODO: This is a simplified placeholder. For proper best response calculation,
        // we would need to reconstruct the entire game tree path and extract all
        // (infoset, action) pairs where this player made a decision.

        return res;
    }

    template<typename T = double>
    static T chance_reach_proba(State state) {
        // Calculate the probability of reaching this state through chance actions
        PostflopPoker game;
        game.set_state(state);

        T prob = T(1.0);

        // Initial deal probability (if hands were dealt from ranges)
        // This would depend on the range probabilities

        // Turn card probability (if turn was dealt)
        if (game.turn_card != INVALID_CARD) {
            // Probability of dealing this specific turn card
            // There are 52 - 7 = 45 cards remaining after flop and hole cards
            prob *= T(1.0 / 45.0);
        }

        // River card probability (if river was dealt)
        if (game.river_card != INVALID_CARD) {
            // Probability of dealing this specific river card
            // There are 52 - 8 = 44 cards remaining after flop, turn, and hole cards
            prob *= T(1.0 / 44.0);
        }

        return prob;
    }

    friend std::ostream& operator<<(std::ostream& os, const PostflopPoker& game) {
        os << "Street: " << static_cast<int>(game.street) << " Pot: " << game.pot;
        return os;
    }

    friend std::ostream& operator<<(std::ostream& os, const PostflopPoker::Action& action) {
        static const char* names[] = {
            "fold", "check", "call", "bet_1/2", "bet_pot", "raise_pot", "all_in"
        };
        if (action < 7) {
            os << names[action];
        } else {
            os << "deal_" << static_cast<int>(action - DEAL_HANDS_START);
        }
        return os;
    }
};

#endif // POSTFLOP_POKER_H
