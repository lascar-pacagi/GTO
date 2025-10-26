#ifndef DCFR_POSTFLOP_H
#define DCFR_POSTFLOP_H

#include "postflop_poker.h"
#include "hand_bucketing.h"
#include <unordered_map>
#include <cmath>
#include <iostream>

/*
 * Discounted CFR Solver for Postflop Poker
 *
 * Based on state-of-the-art algorithms from:
 * - "Solving Large Imperfect Information Games Using CFR+" (Brown et al.)
 * - Discounted CFR paper: https://arxiv.org/abs/1809.04040
 * - TexasSolver: https://github.com/bupticybee/TexasSolver
 * - postflop-solver: https://github.com/b-inary/postflop-solver
 *
 * Key improvements over vanilla CFR:
 * - Faster convergence with discounting
 * - Strategy resets at powers of 4
 * - Optional hand bucketing for abstraction
 */

template<typename Game = PostflopPoker>
class DCFRPostflopSolver {
public:
    // DCFR parameters (from postflop-solver empirical tuning)
    static constexpr double GAMMA = 3.0;    // Regret discount factor (not 2.0!)
    static constexpr double ALPHA = 1.5;    // Positive regret discount
    static constexpr double BETA = 0.0;     // Not used in this variant

    using InfoSet = typename Game::InfoSet;
    using Action = typename Game::Action;
    using State = typename Game::State;

    DCFRPostflopSolver(bool use_bucketing = false, int num_buckets = 50)
        : use_abstraction(use_bucketing),
          bucketer(num_buckets) {
    }

    // Main solving loop
    void solve(Game& game, int num_iterations) {
        std::cout << "Starting DCFR solver for " << num_iterations << " iterations\n";
        std::cout << "Abstraction: " << (use_abstraction ? "enabled" : "disabled") << "\n";

        for (int iter = 1; iter <= num_iterations; iter++) {
            // Alternate training player each iteration
            for (int player = PLAYER1; player <= PLAYER2; player++) {
                game.reset();
                cfr_traverse(game, player, iter, 1.0, 1.0);
            }

            // Strategy reset at powers of 4 (4, 16, 64, 256, 1024, ...)
            if (is_power_of_4(iter)) {
                std::cout << "Iteration " << iter << ": Resetting cumulative strategy\n";
                avg_strategy.clear();
                strategy_iteration_count.clear();
            }

            // Progress reporting
            if (iter % 100 == 0 || iter == num_iterations) {
                std::cout << "Iteration " << iter << " / " << num_iterations;
                std::cout << " (Infosets: " << regrets.size() << ")\n";
            }
        }

        std::cout << "Solving complete!\n";
        std::cout << "Total information sets: " << regrets.size() << "\n";
    }

    // Get average strategy for an infoset
    const std::vector<double>& get_average_strategy(InfoSet infoset,
                                                     const Action* actions,
                                                     int num_actions) {
        static std::vector<double> strategy;
        strategy.resize(num_actions);

        auto it = avg_strategy.find(infoset);
        if (it == avg_strategy.end()) {
            // No data yet - return uniform strategy
            for (int i = 0; i < num_actions; i++) {
                strategy[i] = 1.0 / num_actions;
            }
            return strategy;
        }

        const auto& avg_strat = it->second;
        double sum = 0.0;

        for (int i = 0; i < num_actions; i++) {
            sum += avg_strat[i];
        }

        if (sum > 0) {
            for (int i = 0; i < num_actions; i++) {
                strategy[i] = avg_strat[i] / sum;
            }
        } else {
            // Fallback to uniform
            for (int i = 0; i < num_actions; i++) {
                strategy[i] = 1.0 / num_actions;
            }
        }

        return strategy;
    }

    // Calculate exploitability (for testing)
    double calculate_exploitability(Game& game) {
        // TODO: Implement best response calculation
        // This requires implementing the best_response.h helpers
        return 0.0;
    }

    // Export strategy to file
    void save_strategy(const char* filename) {
        std::ofstream out(filename);
        out << "InfoSet,Action,Probability\n";

        for (const auto& [infoset, strat] : avg_strategy) {
            double sum = 0.0;
            for (double prob : strat) sum += prob;

            if (sum > 0) {
                for (size_t i = 0; i < strat.size(); i++) {
                    out << infoset << "," << i << ","
                        << (strat[i] / sum) << "\n";
                }
            }
        }

        std::cout << "Strategy saved to " << filename << "\n";
    }

private:
    // DCFR traversal (recursive)
    double cfr_traverse(Game& game, int training_player, int iteration,
                       double pi_player, double pi_opponent) {

        // Base cases
        if (game.game_over()) {
            return game.payoff(training_player);
        }

        int current = game.current_player();

        // Chance nodes
        if (current == CHANCE) {
            return cfr_chance_node(game, training_player, iteration,
                                  pi_player, pi_opponent);
        }

        // Get available actions
        List<Action, Game::MAX_NB_ACTIONS> action_list;
        game.actions(action_list);
        int num_actions = action_list.size();

        if (num_actions == 0) {
            std::cerr << "Warning: No actions available at non-terminal node\n";
            return 0.0;
        }

        // Get infoset (potentially abstracted)
        InfoSet infoset = use_abstraction ?
            get_abstracted_infoset(game, current) :
            game.get_info_set(current);

        // Get current strategy using Regret Matching+
        std::vector<double> strategy = get_strategy(infoset, action_list.list, num_actions);

        // Recursively compute values for each action
        std::vector<double> action_values(num_actions);
        double node_value = 0.0;

        for (int i = 0; i < num_actions; i++) {
            Action action = action_list.list[i];

            game.play(action);

            if (current == training_player) {
                action_values[i] = cfr_traverse(game, training_player, iteration,
                                               pi_player * strategy[i], pi_opponent);
            } else {
                action_values[i] = cfr_traverse(game, training_player, iteration,
                                               pi_player, pi_opponent * strategy[i]);
            }

            game.undo(action);

            node_value += strategy[i] * action_values[i];
        }

        // Update regrets for training player
        if (current == training_player) {
            update_regrets_dcfr(infoset, action_values, node_value, strategy,
                               num_actions, iteration, pi_opponent);
        }

        // Update average strategy
        if (current == training_player) {
            update_average_strategy(infoset, strategy, num_actions, pi_player, iteration);
        }

        return node_value;
    }

    // Handle chance nodes (deal turn/river)
    double cfr_chance_node(Game& game, int training_player, int iteration,
                          double pi_player, double pi_opponent) {
        List<Action, Game::MAX_NB_CHANCE_ACTIONS> action_list;
        List<int, Game::MAX_NB_CHANCE_ACTIONS> proba_list;

        game.actions(action_list);
        game.probas(proba_list);

        double expected_value = 0.0;
        double total_prob = 0.0;

        // Sum probabilities for normalization
        for (auto it = proba_list.list; it != proba_list.last; ++it) {
            total_prob += *it;
        }

        // Traverse each chance outcome
        int idx = 0;
        for (auto it = action_list.list; it != action_list.last; ++it, ++idx) {
            Action action = *it;
            double prob = proba_list.list[idx] / total_prob;

            game.play(action);
            double value = cfr_traverse(game, training_player, iteration,
                                       pi_player, pi_opponent);
            game.undo(action);

            expected_value += prob * value;
        }

        return expected_value;
    }

    // Get strategy using Regret Matching+
    std::vector<double> get_strategy(InfoSet infoset, const Action* actions,
                                     int num_actions) {
        std::vector<double> strategy(num_actions);

        auto it = regrets.find(infoset);
        if (it == regrets.end()) {
            // No regrets yet - uniform strategy
            for (int i = 0; i < num_actions; i++) {
                strategy[i] = 1.0 / num_actions;
            }
            return strategy;
        }

        const auto& regret_vec = it->second;
        double sum_positive_regrets = 0.0;

        // Sum positive regrets (Regret Matching+)
        for (int i = 0; i < num_actions; i++) {
            double regret = std::max(0.0, regret_vec[i]);
            strategy[i] = regret;
            sum_positive_regrets += regret;
        }

        // Normalize
        if (sum_positive_regrets > 0) {
            for (int i = 0; i < num_actions; i++) {
                strategy[i] /= sum_positive_regrets;
            }
        } else {
            // Uniform if no positive regrets
            for (int i = 0; i < num_actions; i++) {
                strategy[i] = 1.0 / num_actions;
            }
        }

        return strategy;
    }

    // Update regrets with DCFR discounting
    void update_regrets_dcfr(InfoSet infoset, const std::vector<double>& action_values,
                            double node_value, const std::vector<double>& strategy,
                            int num_actions, int iteration, double pi_opponent) {

        auto& regret_vec = regrets[infoset];
        if (regret_vec.empty()) {
            regret_vec.resize(num_actions, 0.0);
        }

        // Compute discount factors
        double t_pow_gamma = std::pow(iteration, -GAMMA);
        double t_pow_alpha = std::pow(iteration, -ALPHA);

        // Update each action's regret with discounting
        for (int i = 0; i < num_actions; i++) {
            double instant_regret = pi_opponent * (action_values[i] - node_value);

            // Discounted update (key DCFR innovation!)
            if (regret_vec[i] > 0) {
                regret_vec[i] = regret_vec[i] * t_pow_gamma + instant_regret;
            } else {
                regret_vec[i] = regret_vec[i] * t_pow_alpha + instant_regret;
            }
        }
    }

    // Update average strategy
    void update_average_strategy(InfoSet infoset, const std::vector<double>& strategy,
                                int num_actions, double pi_player, int iteration) {
        auto& avg_strat = avg_strategy[infoset];
        if (avg_strat.empty()) {
            avg_strat.resize(num_actions, 0.0);
        }

        // Weight by reach probability
        for (int i = 0; i < num_actions; i++) {
            avg_strat[i] += pi_player * strategy[i];
        }
    }

    // Check if number is a power of 4
    bool is_power_of_4(int n) {
        // Power of 4 means power of 2 with even exponent
        // In binary: bits at positions 0,2,4,6,... (mask: 0x55555555)
        return (n > 0) &&
               (n & (n - 1)) == 0 &&  // Power of 2
               (n & 0xAAAAAAAA) == 0;  // Even power (no bits at odd positions)
    }

    // Get abstracted infoset (using hand bucketing)
    InfoSet get_abstracted_infoset(const Game& game, int player) {
        // Get hand bucket instead of exact cards
        const uint8_t* hole_cards = (player == PLAYER1) ?
            game.p1_hole : game.p2_hole;

        uint8_t board[5];
        int num_board = 3;  // Flop
        board[0] = game.flop_cards[0];
        board[1] = game.flop_cards[1];
        board[2] = game.flop_cards[2];

        if (game.turn_card != Game::INVALID_CARD) {
            board[num_board++] = game.turn_card;
        }
        if (game.river_card != Game::INVALID_CARD) {
            board[num_board++] = game.river_card;
        }

        int bucket = bucketer.get_hand_bucket(hole_cards, board, num_board,
                                             Game::get_evaluator());

        // Create abstracted infoset: bucket | board | street | simplified history
        InfoSet abstract_info = 0;
        abstract_info |= static_cast<InfoSet>(bucket) << 0;
        abstract_info |= static_cast<InfoSet>(game.street) << 16;

        // Include pot size bucket (coarse-grained)
        int pot_bucket = get_pot_bucket(game.pot, game.starting_pot);
        abstract_info |= static_cast<InfoSet>(pot_bucket) << 20;

        // Simplified action history (last few actions only)
        abstract_info |= (game.action_history & 0xFFFF) << 24;

        return abstract_info;
    }

    // Bucket pot sizes for abstraction
    int get_pot_bucket(int pot, int starting_pot) {
        double ratio = static_cast<double>(pot) / starting_pot;

        if (ratio < 1.5) return 0;
        if (ratio < 2.5) return 1;
        if (ratio < 4.0) return 2;
        if (ratio < 6.0) return 3;
        return 4;
    }

    // Data storage
    std::unordered_map<InfoSet, std::vector<double>> regrets;
    std::unordered_map<InfoSet, std::vector<double>> avg_strategy;
    std::unordered_map<InfoSet, int> strategy_iteration_count;

    bool use_abstraction;
    HandBucketer bucketer;
};

#endif // DCFR_POSTFLOP_H
