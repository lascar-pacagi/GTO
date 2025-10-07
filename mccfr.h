#ifndef MCCFR_H
#define MCCFR_H
#include "game.h"
#include "game_tree.h"
#include "list.h"
#include <iostream>
#include <cstring>
#include <array>
#include "misc.h"
#include <memory>
#include <map>
#include <atomic>
#include <omp.h>
#include <random>

template<typename Game, typename T = double>
struct MCCFR {
    struct alignas(64) Shard {
        std::vector<T> probas;
        std::vector<T> regrets;
        std::vector<T> strategies;
        std::atomic<bool> lock{false};

        Shard() = default;
        Shard(Shard&& shard) : probas(std::move(shard.probas)),
                                regrets(std::move(shard.regrets)),
                                strategies(std::move(shard.strategies)) {}

        void add_regrets(const std::array<T, Game::MAX_NB_PLAYER_ACTIONS>& regret_values, size_t n) {
            bool expected = false;
            while (!lock.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
                expected = false;
            }
            for (size_t i = 0; i < n; i++) {
                regrets[i] += regret_values[i];
            }
            lock.store(false, std::memory_order_release);
        }

        void add_strategies(const std::array<T, Game::MAX_NB_PLAYER_ACTIONS>& strategy_values, size_t n) {
            bool expected = false;
            while (!lock.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
                expected = false;
            }
            for (size_t i = 0; i < n; i++) {
                strategies[i] += strategy_values[i];
            }
            lock.store(false, std::memory_order_release);
        }

        void get_strategy_from_regrets(std::array<T, Game::MAX_NB_PLAYER_ACTIONS>& s, size_t n) {
            bool expected = false;
            while (!lock.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
                expected = false;
            }
            T sum{};
            for (size_t i = 0; i < n; i++) {
                s[i] = std::max(regrets[i], T{});
                sum += s[i];
            }
            lock.store(false, std::memory_order_release);

            if (sum > 0) {
                for (size_t i = 0; i < n; i++) s[i] /= sum;
            } else {
                for (size_t i = 0; i < n; i++) s[i] = static_cast<T>(1.0) / n;
            }
        }
    };

    const GameTree<Game>& tree;
    std::vector<Shard> shards;
    std::vector<int> node_idx_to_shard_idx;

    MCCFR(const GameTree<Game>& tree) : tree(tree), shards(tree.nb_nodes()) {
        node_idx_to_shard_idx.resize(tree.nb_nodes());
        std::map<typename Game::InfoSet, int> info_set_to_shard_idx;
        init(info_set_to_shard_idx, 0);
        shards.shrink_to_fit();
    }

    void init(std::map<typename Game::InfoSet, int>& info_set_to_shard_idx, int idx) {
        int n = tree.nb_children[idx] >> 2;
        int player = tree.nb_children[idx] & 3;
        if (n == 0) {
            return;
        }
        const auto& info_set = tree.info_sets[idx];
        bool already_seen = info_set_to_shard_idx.contains(info_set);
        if (!already_seen) {
            info_set_to_shard_idx[info_set] = static_cast<int>(shards.size());
            shards.emplace_back();
        }
        node_idx_to_shard_idx[idx] = info_set_to_shard_idx[info_set];
        int start = tree.start_children_and_actions[idx];
        if (player == CHANCE) {
            if (!already_seen) {
                T sum{};
                for (int i = 0; i < n; i++) {
                    sum += tree.children[start + 2*i + 1];
                }
                for (int i = 0; i < n; i++) {
                    shards.back().probas.push_back(tree.children[start + 2*i + 1] / sum);
                }
            }
            shards.back().probas.shrink_to_fit();
            for (int i = 0; i < n; i++) {
                init(info_set_to_shard_idx, tree.children[start + 2*i]);
            }
        } else {
            if (!already_seen) {
                shards.back().regrets.resize(n, T{});
                shards.back().strategies.resize(n, T{});
                shards.back().regrets.shrink_to_fit();
                shards.back().strategies.shrink_to_fit();
            }
            for (int i = 0; i < n; i++) {
                init(info_set_to_shard_idx, tree.children[start + i]);
            }
        }
    }

    // Sample an action based on probabilities
    int sample_action(const std::array<T, Game::MAX_NB_PLAYER_ACTIONS>& probs, int n, std::mt19937& rng) {
        std::uniform_real_distribution<T> dist(0.0, 1.0);
        T r = dist(rng);
        T cumulative = 0;
        for (int i = 0; i < n; i++) {
            cumulative += probs[i];
            if (r <= cumulative) return i;
        }
        return n - 1;
    }

    // Outcome sampling MCCFR
    T outcome_sampling(int idx, int traversing_player, T pi1, T pi2, T sample_prob, std::mt19937& rng) {
        int start = tree.start_children_and_actions[idx];
        int n = tree.nb_children[idx] >> 2;
        if (n == 0) {
            // Terminal node - return utility
            T utility = static_cast<T>(tree.children[start]);
            // Return utility from traversing player's perspective
            if (traversing_player == PLAYER2) utility = -utility;
            return utility;
        }

        int player = tree.nb_children[idx] & 3;
        auto& shard = shards[node_idx_to_shard_idx[idx]];

        if (player == CHANCE) {
            // Sample a chance action
            int sampled_action = sample_action(shard.probas, n, rng);
            T prob = shard.probas[sampled_action];
            return outcome_sampling(tree.children[start + 2*sampled_action], traversing_player,
                                   pi1, pi2, sample_prob * prob, rng);
        }

        std::array<T, Game::MAX_NB_PLAYER_ACTIONS> s;
        shard.get_strategy_from_regrets(s, n);

        if (player == traversing_player) {
            // At traversing player's node - compute counterfactual values for all actions
            std::array<T, Game::MAX_NB_PLAYER_ACTIONS> action_values;
            T u{};

            for (int i = 0; i < n; i++) {
                T new_pi1 = (player == PLAYER1) ? pi1 * s[i] : pi1;
                T new_pi2 = (player == PLAYER2) ? pi2 * s[i] : pi2;
                action_values[i] = outcome_sampling(tree.children[start + i], traversing_player,
                                                    new_pi1, new_pi2, sample_prob, rng);
                u += s[i] * action_values[i];
            }

            // Update regrets
            std::array<T, Game::MAX_NB_PLAYER_ACTIONS> regret_updates;
            T opponent_reach = (player == PLAYER1) ? pi2 : pi1;
            T weight = opponent_reach / sample_prob;

            for (int i = 0; i < n; i++) {
                regret_updates[i] = weight * (action_values[i] - u);
            }
            shard.add_regrets(regret_updates, n);

            // Update average strategy
            std::array<T, Game::MAX_NB_PLAYER_ACTIONS> strategy_updates;
            T self_reach = (player == PLAYER1) ? pi1 : pi2;
            for (int i = 0; i < n; i++) {
                strategy_updates[i] = self_reach * s[i] / sample_prob;
            }
            shard.add_strategies(strategy_updates, n);

            return u;
        } else {
            // At opponent's node - sample according to strategy
            int sampled_action = sample_action(s, n, rng);
            T action_prob = s[sampled_action];

            T new_pi1 = (player == PLAYER1) ? pi1 * action_prob : pi1;
            T new_pi2 = (player == PLAYER2) ? pi2 * action_prob : pi2;

            return outcome_sampling(tree.children[start + sampled_action], traversing_player,
                                   new_pi1, new_pi2, sample_prob * action_prob, rng);
        }
    }

    void solve(int nb_iterations) {
        std::atomic<T> game_value{};
        std::atomic<int> iter{};

        #pragma omp parallel
        {
            // Each thread gets its own RNG
            std::random_device rd;
            std::mt19937 rng(rd() + omp_get_thread_num());

            #pragma omp for
            for (int i = 0; i < nb_iterations; i++) {
                // Alternate between players each iteration
                int traversing_player = (i % 2 == 0) ? PLAYER1 : PLAYER2;
                T value = outcome_sampling(0, traversing_player, 1.0, 1.0, 1.0, rng);

                // Convert back to PLAYER1's perspective for averaging
                if (traversing_player == PLAYER2) value = -value;

                game_value += value;
                ++iter;
            }
        }

        std::cout << "Iterations: " << iter << '\n';
        std::cout << "Game value: " << game_value / iter << '\n';
    }

    void fill_strategy(int idx, Strategy<Game>& strategy) const {
        int n = tree.nb_children[idx] >> 2;
        if (n == 0) {
            return;
        }
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
            const auto& shard = shards[node_idx_to_shard_idx[idx]];
            T sum{};
            for (int i = 0; i < n; i++) {
                sum += shard.strategies[i];
            }
            for (int i = 0; i < n; i++) {
                strategy.strategies.push_back(sum > 0 ? shard.strategies[i] / sum : static_cast<T>(1.0) / n);
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

#endif
