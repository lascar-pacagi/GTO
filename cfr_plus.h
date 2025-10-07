#ifndef CFR_PLUS_H
#define CFR_PLUS_H
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

template<typename Game, typename T = double>
struct CFRPlus {
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
                // CFR+ key difference: regrets are floored at 0
                regrets[i] = std::max(T{}, regrets[i] + regret_values[i]);
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

    CFRPlus(const GameTree<Game>& tree) : tree(tree), shards(tree.nb_nodes()) {
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

    T cfr_plus_iteration(int idx, T pi1, T pi2, T pc) {
        if (pi1 == T{} && pi2 == T{}) return T{};
        int start = tree.start_children_and_actions[idx];
        int n = tree.nb_children[idx] >> 2;
        if (n == 0) {
            return static_cast<T>(tree.children[start]);
        }
        int player = tree.nb_children[idx] & 3;
        auto& shard = shards[node_idx_to_shard_idx[idx]];

        if (player == CHANCE) {
            T u{};
            const T* probas = shard.probas.data();
            for (int i = 0; i < n; i++) {
                T p = probas[i];
                T v = cfr_plus_iteration(tree.children[start + 2*i], pi1, pi2, pc * p);
                u += p * v;
            }
            return u;
        }

        std::array<T, Game::MAX_NB_PLAYER_ACTIONS> s;
        shard.get_strategy_from_regrets(s, n);
        T u{};
        std::array<T, Game::MAX_NB_PLAYER_ACTIONS> utils;
        std::array<T, Game::MAX_NB_PLAYER_ACTIONS> regret_updates;
        std::array<T, Game::MAX_NB_PLAYER_ACTIONS> strategy_updates;

        if (player == PLAYER1) {
            for (int i = 0; i < n; i++) {
                utils[i] = cfr_plus_iteration(tree.children[start + i], s[i] * pi1, pi2, pc);
                u += s[i] * utils[i];
            }
            for (int i = 0; i < n; i++) {
                regret_updates[i] = pi2 * pc * (utils[i] - u);
                strategy_updates[i] = pi1 * s[i];
            }
        } else {
            for (int i = 0; i < n; i++) {
                utils[i] = cfr_plus_iteration(tree.children[start + i], pi1, s[i] * pi2, pc);
                u += s[i] * utils[i];
            }
            for (int i = 0; i < n; i++) {
                regret_updates[i] = pi1 * pc * (u - utils[i]);
                strategy_updates[i] = pi2 * s[i];
            }
        }

        shard.add_regrets(regret_updates, n);
        shard.add_strategies(strategy_updates, n);
        return u;
    }

    void solve(int nb_iterations) {
        std::atomic<T> game_value{};
        std::atomic<int> iter{};

        #pragma omp parallel for
        for (int i = 0; i < nb_iterations; i++) {
            T value = cfr_plus_iteration(0, 1, 1, 1);
            game_value += value;
            ++iter;
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
