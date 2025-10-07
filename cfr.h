#ifndef CFR_H
#define CFR_H
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
struct CFR {
    struct alignas(64) Shard {        
        std::vector<T> probas;
        std::vector<T> regrets_and_strategies;
        std::atomic<bool> lock{false};
        Shard() = default;
        Shard(Shard&& shard) : probas(std::move(shard.probas)), regrets_and_strategies(std::move(shard.regrets_and_strategies)) {            
        }
        void add_regret_and_strategies(const std::array<T, 2 * Game::MAX_NB_PLAYER_ACTIONS>& values) {
            bool expected = false;
            while (!lock.compare_exchange_strong(expected, 0, std::memory_order_acquire));
            for (size_t i = 0; i < regrets_and_strategies.size(); i++) {
                regrets_and_strategies[i] += values[i];
            }
            lock.store(false, std::memory_order_release);            
        }
        void get_strategy_from_regrets(std::array<T, Game::MAX_NB_PLAYER_ACTIONS>& s) {
            bool expected = false;
            while (!lock.compare_exchange_strong(expected, 0, std::memory_order_acquire));
            size_t n = regrets_and_strategies.size() / 2;                
            T sum{};
            for (size_t i = 0; i < n; i++) {
                T regret = std::max(regrets_and_strategies[i], T{});
                s[i] = regret;
                sum += regret;
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
    
    CFR(const GameTree<Game>& tree) : tree(tree), shards(tree.nb_nodes()) {
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
                shards.back().regrets_and_strategies.resize(2*n);
                shards.back().regrets_and_strategies.shrink_to_fit();
            }
            for (int i = 0; i < n; i++) {
                init(info_set_to_shard_idx, tree.children[start + i]);
            }
        }
    }
    T linear_cfr(int idx, T pi1, T pi2, T pc, int iter) {
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
                T v = linear_cfr(tree.children[start + 2*i], pi1, pi2, pc * p, iter);
                u += p * v;
            }
            return u;
        }
        std::array<T, Game::MAX_NB_PLAYER_ACTIONS> s;
        shard.get_strategy_from_regrets(s);                
        T u{};
        std::array<T, Game::MAX_NB_PLAYER_ACTIONS> utils;
        std::array<T, 2 * Game::MAX_NB_PLAYER_ACTIONS> regrets_and_strategies;
        if (player == PLAYER1) {
            for (int i = 0; i < n; i++) {
                utils[i] = linear_cfr(tree.children[start + i], s[i] * pi1, pi2, pc, iter);
                u += s[i] * utils[i];
            }
            for (int i = 0; i < n; i++) {
                regrets_and_strategies[i] = iter * pi2 * pc * (utils[i] - u);
                regrets_and_strategies[n + i] = iter * pi1 * s[i];
            }
        } else {
            for (int i = 0; i < n; i++) {
                utils[i] = linear_cfr(tree.children[start + i], pi1, s[i] * pi2, pc, iter);
                u += s[i] * utils[i];
            }
            for (int i = 0; i < n; i++) {
                regrets_and_strategies[i] = iter * pi1 * pc * (u - utils[i]);
                regrets_and_strategies[n + i] = iter * pi2 * s[i];
            }
        }
        shard.add_regret_and_strategies(regrets_and_strategies);      
        return u;
    }
    void solve(int nb_iterations) {
        std::atomic<T> game_value{};
        std::atomic<int> iter{};
        #pragma omp parallel for
        for (int i = 1; i <= nb_iterations; i++) {
            game_value += linear_cfr(0, 1, 1, 1, iter);
            ++iter;
        }
        std::cout << iter << '\n';
        std::cout << game_value / iter << '\n'; 
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
                sum += shard.regrets_and_strategies[n + i];
            }
            for (int i = 0; i < n; i++) {
                strategy.strategies.push_back(sum > 0 ? shard.regrets_and_strategies[n + i] / sum : static_cast<T>(1.0) / n);
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
