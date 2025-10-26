#ifndef MCCFR_H
#define MCCFR_H

#include "game.h"
#include "game_tree.h"
#include "strategy.h"
#include "list.h"
#include <iostream>
#include <cstring>
#include <array>
#include "misc.h"
#include <memory>
#include <map>
#include <atomic>
#include <omp.h>
#include <thread>
#include <random>
#include <chrono>

/*
 * Monte Carlo CFR (MCCFR) Implementation
 *
 * Key differences from vanilla CFR:
 * 1. Samples one trajectory per iteration instead of traversing entire tree
 * 2. Much faster per iteration (O(depth) vs O(tree size))
 * 3. Higher variance, needs more iterations to converge
 * 4. Three main variants:
 *    - Outcome Sampling: sample one outcome at chance nodes
 *    - External Sampling: sample opponent actions + chance
 *    - Chance Sampling: sample only chance actions (implemented here)
 *
 * This implementation uses External Sampling MCCFR which is most common for poker.
 */

template<typename Game>
struct MCCFR {
    struct alignas(64) Shard {
        std::array<double, Game::MAX_NB_PLAYER_ACTIONS> regrets_and_strategies;
        std::atomic<bool> lock{false};

        Shard() = default;
        Shard(Shard&& shard) : regrets_and_strategies(std::move(shard.regrets_and_strategies)) {}

        void add_regret_and_strategies(const std::array<double, 2 * Game::MAX_NB_PLAYER_ACTIONS>& values, int n) {
            for (;;) {
                if (lock.load(std::memory_order_relaxed)) {
                    continue;
                }
                bool expected = false;
                if (lock.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
                    break;
                }
            }
            for (int i = 0; i < 2 * n; i++) {
                regrets_and_strategies[i] += values[i];
            }
            lock.store(false, std::memory_order_release);
        }

        void get_strategy_from_regrets(std::array<double, Game::MAX_NB_PLAYER_ACTIONS>& s, int n) {
            double sum{};
            for (int i = 0; i < n; i++) {
                double regret = std::max(regrets_and_strategies[i], 0.0);
                s[i] = regret;
                sum += regret;
            }
            if (sum > 0) {
                for (int i = 0; i < n; i++) s[i] /= sum;
            } else {
                for (int i = 0; i < n; i++) s[i] = 1.0 / n;
            }
        }
    };

    const GameTree<Game>& tree;
    std::vector<Shard> shards;
    std::vector<double> probas;
    std::vector<int> node_idx_to_data_idx;

    // Thread-local RNG for sampling
    struct PRNG {
        std::mt19937_64 gen;
        std::uniform_real_distribution<double> dist{0.0, 1.0};

        PRNG() : gen(static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
        ) + static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()))) {}

        double random() {
            return dist(gen);
        }

        // Sample action index from probability distribution
        int sample_action(const double* probs, int n) {
            double r = random();
            double cumulative = 0.0;
            for (int i = 0; i < n; i++) {
                cumulative += probs[i];
                if (r < cumulative) return i;
            }
            return n - 1;  // Numerical stability
        }
    };

    static thread_local PRNG prng;

    MCCFR(const GameTree<Game>& tree) : tree(tree), shards(tree.nb_nodes()) {
        node_idx_to_data_idx.resize(tree.nb_nodes());
        std::map<typename Game::InfoSet, int> info_set_to_data_idx;
        int shard_idx = 0;
        int probas_idx = 0;
        init(info_set_to_data_idx, 0, shard_idx, probas_idx);
        shards.shrink_to_fit();
        probas.shrink_to_fit();
    }

    void init(std::map<typename Game::InfoSet, int>& info_set_to_data_idx, int idx, int& shard_idx, int& probas_idx) {
        int n = tree.nb_children[idx] >> 2;
        int player = tree.nb_children[idx] & 3;
        if (n == 0) return;

        const auto& info_set = tree.info_sets[idx];
        bool already_seen = info_set_to_data_idx.contains(info_set);

        if (!already_seen) {
            if (player == CHANCE) {
                info_set_to_data_idx[info_set] = probas_idx;
                probas_idx += n;
            } else {
                info_set_to_data_idx[info_set] = shard_idx++;
                shards.emplace_back();
            }
        }

        int start = tree.start_children_and_actions[idx];
        if (player == CHANCE) {
            node_idx_to_data_idx[idx] = info_set_to_data_idx[info_set];
            if (!already_seen) {
                double sum{};
                for (int i = 0; i < n; i++) {
                    sum += tree.children[start + 2*i + 1];
                }
                for (int i = 0; i < n; i++) {
                    probas.push_back(tree.children[start + 2*i + 1] / sum);
                }
            }
            for (int i = 0; i < n; i++) {
                init(info_set_to_data_idx, tree.children[start + 2*i], shard_idx, probas_idx);
            }
        } else {
            node_idx_to_data_idx[idx] = info_set_to_data_idx[info_set];
            for (int i = 0; i < n; i++) {
                init(info_set_to_data_idx, tree.children[start + i], shard_idx, probas_idx);
            }
        }
    }

    /*
     * External Sampling MCCFR
     *
     * For the updating player:
     *   - Explore all actions (compute counterfactual values)
     *   - Update regrets for all actions
     *
     * For opponent and chance:
     *   - Sample one action according to strategy/probability
     *   - Only traverse that branch
     *
     * This reduces variance compared to outcome sampling while keeping it fast.
     */
    double external_sampling_mccfr(int idx, int updating_player, double pi_updating, double pi_opponent, int iter) {
        //static constexpr double EPSILON = 1e-5;

        int start = tree.start_children_and_actions[idx];
        int n = tree.nb_children[idx] >> 2;

        // Terminal node
        if (n == 0) {
            double payoff = static_cast<double>(tree.children[start]);
            return updating_player == PLAYER1 ? payoff : -payoff;
        }

        int player = tree.nb_children[idx] & 3;

        // Chance node - sample one action
        if (player == CHANCE) {
            int probas_idx = node_idx_to_data_idx[idx];

            // Sample chance action according to probabilities
            int sampled_action = prng.sample_action(&probas[probas_idx], n);
            //double p = probas[probas_idx + sampled_action];

            // Traverse only the sampled branch
            return external_sampling_mccfr(
                tree.children[start + 2*sampled_action],
                updating_player,
                pi_updating,
                pi_opponent,
                iter
            );
        }

        auto& shard = shards[node_idx_to_data_idx[idx]];
        std::array<double, Game::MAX_NB_PLAYER_ACTIONS> strategy;
        shard.get_strategy_from_regrets(strategy, n);

        // This player is being updated - explore all actions
        if (player == updating_player) {
            std::array<double, Game::MAX_NB_PLAYER_ACTIONS> action_values;
            double node_value = 0.0;

            // Compute value of each action
            for (int i = 0; i < n; i++) {
                action_values[i] = external_sampling_mccfr(
                    tree.children[start + i],
                    updating_player,
                    pi_updating * strategy[i],
                    pi_opponent,
                    iter
                );
                node_value += strategy[i] * action_values[i];
            }

            // Update regrets and cumulative strategy
            std::array<double, 2 * Game::MAX_NB_PLAYER_ACTIONS> regrets_and_strategies;
            for (int i = 0; i < n; i++) {
                // Regret = counterfactual value - node value
                regrets_and_strategies[i] = pi_opponent * (action_values[i] - node_value);

                // Cumulative strategy weighted by reach probability
                regrets_and_strategies[n + i] = pi_updating * strategy[i];
            }

            shard.add_regret_and_strategies(regrets_and_strategies, n);
            return node_value;
        }

        // Opponent - sample one action according to strategy
        else {
            int sampled_action = prng.sample_action(strategy.data(), n);

            return external_sampling_mccfr(
                tree.children[start + sampled_action],
                updating_player,
                pi_updating,
                pi_opponent * strategy[sampled_action],
                iter
            );
        }
    }

    /*
     * Outcome Sampling MCCFR (alternative variant)
     *
     * Samples actions for ALL players (including updating player)
     * Highest variance but fastest per iteration
     * Useful for very large games
     */
    double outcome_sampling_mccfr(int idx, int updating_player, double pi_updating, double pi_opponent,
                                   double sample_prob, int iter) {
        //static constexpr double EPSILON = 1e-5;

        int start = tree.start_children_and_actions[idx];
        int n = tree.nb_children[idx] >> 2;

        if (n == 0) {
            double payoff = static_cast<double>(tree.children[start]);
            double utility = updating_player == PLAYER1 ? payoff : -payoff;

            // Importance sampling correction
            return utility / sample_prob;
        }

        int player = tree.nb_children[idx] & 3;

        // Chance node
        if (player == CHANCE) {
            int probas_idx = node_idx_to_data_idx[idx];
            int sampled_action = prng.sample_action(&probas[probas_idx], n);
            double p = probas[probas_idx + sampled_action];

            return outcome_sampling_mccfr(
                tree.children[start + 2*sampled_action],
                updating_player,
                pi_updating,
                pi_opponent,
                sample_prob * p,
                iter
            );
        }

        auto& shard = shards[node_idx_to_data_idx[idx]];
        std::array<double, Game::MAX_NB_PLAYER_ACTIONS> strategy;
        shard.get_strategy_from_regrets(strategy, n);

        // Sample one action for this player
        int sampled_action = prng.sample_action(strategy.data(), n);
        double action_prob = strategy[sampled_action];

        double value;
        if (player == updating_player) {
            value = outcome_sampling_mccfr(
                tree.children[start + sampled_action],
                updating_player,
                pi_updating * action_prob,
                pi_opponent,
                sample_prob * action_prob,
                iter
            );

            // Update regrets for the sampled action
            std::array<double, 2 * Game::MAX_NB_PLAYER_ACTIONS> regrets_and_strategies{};
            regrets_and_strategies[sampled_action] = pi_opponent * value;
            regrets_and_strategies[n + sampled_action] = pi_updating;

            shard.add_regret_and_strategies(regrets_and_strategies, n);
        } else {
            value = outcome_sampling_mccfr(
                tree.children[start + sampled_action],
                updating_player,
                pi_updating,
                pi_opponent * action_prob,
                sample_prob * action_prob,
                iter
            );
        }

        return value;
    }

    /*
     * Chance Sampling MCCFR
     *
     * Only samples chance actions, explores all player actions
     * Lower variance than external sampling for games with many chance outcomes
     */
    double chance_sampling_mccfr(int idx, double pi1, double pi2, int iter) {
        //static constexpr double EPSILON = 1e-5;

        int start = tree.start_children_and_actions[idx];
        int n = tree.nb_children[idx] >> 2;

        if (n == 0) {
            return static_cast<double>(tree.children[start]);
        }

        int player = tree.nb_children[idx] & 3;

        // Chance node - sample
        if (player == CHANCE) {
            int probas_idx = node_idx_to_data_idx[idx];
            int sampled_action = prng.sample_action(&probas[probas_idx], n);

            return chance_sampling_mccfr(
                tree.children[start + 2*sampled_action],
                pi1, pi2, iter
            );
        }

        // Player node - explore all actions
        auto& shard = shards[node_idx_to_data_idx[idx]];
        std::array<double, Game::MAX_NB_PLAYER_ACTIONS> strategy;
        shard.get_strategy_from_regrets(strategy, n);

        std::array<double, Game::MAX_NB_PLAYER_ACTIONS> utils;
        double u{};

        if (player == PLAYER1) {
            for (int i = 0; i < n; i++) {
                utils[i] = chance_sampling_mccfr(tree.children[start + i], strategy[i] * pi1, pi2, iter);
                u += strategy[i] * utils[i];
            }

            std::array<double, 2 * Game::MAX_NB_PLAYER_ACTIONS> regrets_and_strategies;
            for (int i = 0; i < n; i++) {
                regrets_and_strategies[i] = pi2 * (utils[i] - u);
                regrets_and_strategies[n + i] = pi1 * strategy[i];
            }
            shard.add_regret_and_strategies(regrets_and_strategies, n);
        } else {
            for (int i = 0; i < n; i++) {
                utils[i] = chance_sampling_mccfr(tree.children[start + i], pi1, strategy[i] * pi2, iter);
                u += strategy[i] * utils[i];
            }

            std::array<double, 2 * Game::MAX_NB_PLAYER_ACTIONS> regrets_and_strategies;
            for (int i = 0; i < n; i++) {
                regrets_and_strategies[i] = pi1 * (u - utils[i]);
                regrets_and_strategies[n + i] = pi2 * strategy[i];
            }
            shard.add_regret_and_strategies(regrets_and_strategies, n);
        }

        return u;
    }

    /*
     * Solve using External Sampling (recommended for most poker games)
     */
    void solve_external_sampling(int nb_iterations) {
        std::atomic<int> iter{1};

        #pragma omp parallel for
        for (int i = 1; i <= nb_iterations; i++) {
            int current_iter = iter.fetch_add(1, std::memory_order_relaxed);

            // Alternate which player we update (important for convergence)
            int updating_player = (current_iter % 2 == 0) ? PLAYER1 : PLAYER2;

            external_sampling_mccfr(0, updating_player, 1.0, 1.0, current_iter);
        }

        std::cout << "External Sampling MCCFR completed " << nb_iterations << " iterations\n";
    }

    /*
     * Solve using Outcome Sampling (fastest, highest variance)
     */
    void solve_outcome_sampling(int nb_iterations) {
        std::atomic<int> iter{1};

        #pragma omp parallel for
        for (int i = 1; i <= nb_iterations; i++) {
            int current_iter = iter.fetch_add(1, std::memory_order_relaxed);
            int updating_player = (current_iter % 2 == 0) ? PLAYER1 : PLAYER2;

            outcome_sampling_mccfr(0, updating_player, 1.0, 1.0, 1.0, current_iter);
        }

        std::cout << "Outcome Sampling MCCFR completed " << nb_iterations << " iterations\n";
    }

    /*
     * Solve using Chance Sampling (good for games with many chance nodes)
     */
    void solve_chance_sampling(int nb_iterations) {
        std::atomic<int> iter{1};

        #pragma omp parallel for
        for (int i = 1; i <= nb_iterations; i++) {
            int current_iter = iter.fetch_add(1, std::memory_order_relaxed);
            chance_sampling_mccfr(0, 1.0, 1.0, current_iter);
        }

        std::cout << "Chance Sampling MCCFR completed " << nb_iterations << " iterations\n";
    }

    /*
     * Default solve method (uses external sampling)
     */
    void solve(int nb_iterations) {
        solve_external_sampling(nb_iterations);
    }

    void fill_strategy(int idx, Strategy<Game>& strategy) const {
        int n = tree.nb_children[idx] >> 2;
        if (n == 0) return;

        int player = tree.nb_children[idx] & 3;
        int start = tree.start_children_and_actions[idx];

        if (player == CHANCE) {
            for (int i = 0; i < n; i++) {
                fill_strategy(tree.children[start + 2*i], strategy);
            }
            return;
        }

        typename Game::InfoSet info_set = tree.info_sets[idx];
        if (!strategy.info_set_to_idx.contains(info_set)) {
            strategy.info_set_to_idx[info_set] = static_cast<int>(strategy.actions.size());
            strategy.info_set_to_nb_actions[info_set] = n;

            for (int i = 0; i < n; i++) {
                strategy.actions.push_back(tree.actions[start + i]);
            }

            const auto& shard = shards[node_idx_to_data_idx[idx]];
            double sum{};
            for (int i = 0; i < n; i++) {
                sum += shard.regrets_and_strategies[n + i];
            }

            for (int i = 0; i < n; i++) {
                strategy.strategies.push_back(sum > 0 ? shard.regrets_and_strategies[n + i] / sum : 1.0 / n);
            }
        }

        for (int i = 0; i < n; i++) {
            fill_strategy(tree.children[start + i], strategy);
        }
    }

    Strategy<Game> get_strategy() const {
        Strategy<Game> strategy;
        fill_strategy(0, strategy);
        return strategy;
    }
};

// Define thread-local PRNG
template<typename Game>
thread_local typename MCCFR<Game>::PRNG MCCFR<Game>::prng{};

#endif // MCCFR_H
