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

template<typename Game, typename T = double>
struct CFR {
    const GameTree<Game>& tree;
    std::vector<T> probas_regrets_and_strategies;
    std::vector<int> idx_to_offset;
    
    CFR(const GameTree<Game>& tree) : tree(tree) {
        idx_to_offset.resize(tree.nb_nodes());
        std::map<typename Game::InfoSet, int> info_set_to_offset;
        int size = init_idx_to_offset(info_set_to_offset, 0, 0);
        std::cout << size << '\n';
        probas_regrets_and_strategies.resize(size);
        fill_probas(0);
    }
    int init_idx_to_offset(std::map<typename Game::InfoSet, int>& info_set_to_offset, int idx, int offset) {
        int n = tree.nb_children[idx] >> 2;
        int player = tree.nb_children[idx] & 3;                
        if (n == 0) {
            return offset;
        }
        auto info_set = tree.info_sets[idx];
        bool already_seen = info_set_to_offset.contains(info_set); 
        if (!already_seen) {
            info_set_to_offset[info_set] = offset;
        }
        idx_to_offset[idx] = info_set_to_offset[info_set];
        int start = tree.start_children_and_actions[idx];
        if (player == CHANCE) {
            if (!already_seen) offset += n;                        
            for (int i = 0; i < n; i++) {
                offset = init_idx_to_offset(info_set_to_offset, tree.children[start + 2*i], offset);
            }
        } else {
            if (!already_seen) offset += 2 * n;
            for (int i = 0; i < n; i++) {
                offset = init_idx_to_offset(info_set_to_offset, tree.children[start + i], offset);
            }
        }
        return offset;
    }
    void fill_probas(int idx) {
        int n = tree.nb_children[idx] >> 2;
        int player = tree.nb_children[idx] & 3;
        if (n == 0) {
            return;
        }
        int start = tree.start_children_and_actions[idx];
        if (player == CHANCE) {            
            T* probas = &probas_regrets_and_strategies[idx_to_offset[idx]];
            T sum{};
            for (int i = 0; i < n; i++) {   
                sum += tree.children[start + 2*i + 1];
            }            
            for (int i = 0; i < n; i++) {
                probas[i] = tree.children[start + 2*i + 1] / sum;
            }
            for (int i = 0; i < n; i++) {
                fill_probas(tree.children[start + 2*i]);
            }
        } else {
            for (int i = 0; i < n; i++) {
                fill_probas(tree.children[start + i]);
            }
        }
    }
    T cfr(int idx, T pi1, T pi2) {
        int n = tree.nb_children[idx] >> 2;
        int player = tree.nb_children[idx] & 3;
        int start = tree.start_children_and_actions[idx];
        if (n == 0) {
            return static_cast<T>(tree.children[start]);
        }
        int offset = idx_to_offset[idx];
        if (player == CHANCE) {
            T u{};
            T* probas = &probas_regrets_and_strategies[offset];
            for (int i = 0; i < n; i++) {
                T p = probas[i];
                T v = cfr(tree.children[start + 2*i], pi1 * p, pi2 * p);
                u += p * v;
            }
            return u;
        }
        T* regrets = &probas_regrets_and_strategies[offset];
        T* strategies = &probas_regrets_and_strategies[offset + n];
        T regrets_sum{};
        for (int i = 0; i < n; i++) {
            regrets_sum += regrets[i];
        }
        std::array<T, Game::MAX_NB_PLAYER_ACTIONS> s;
        if (regrets_sum > 0) {
            for (int i = 0; i < n; i++) {
                s[i] = regrets[i] / regrets_sum;
            }
        } else {
            T p = static_cast<T>(1.0) / n;
            for (int i = 0; i < n; i++) {
                s[i] = p;
            }
        }
        T u{};
        std::array<T, Game::MAX_NB_PLAYER_ACTIONS> utils;
        for (int i = 0; i < n; i++) {
            if (player == PLAYER1) {
                utils[i] = cfr(tree.children[start + i], s[i] * pi1, pi2);
            } else {
                utils[i] = cfr(tree.children[start + i], pi1, s[i] * pi2);
            }
            u += s[i] * utils[i];
        }
        for (int i = 0; i < n; i++) {
            regrets[i] += (player == PLAYER1 ? pi2 : pi1) * (utils[i] - u) * (player == PLAYER1 ? 1 : -1);
            if (regrets[i] < 0) regrets[i] = 0;
            strategies[i] += (player == PLAYER1 ? pi1 : pi2) * s[i];
        }
        return u;
    }
    void solve(int nb_iterations) {
        T game_value{};
        for (int i = 0; i < nb_iterations; i++) {
            game_value += cfr(0, 1, 1);
        }        
        std::cout << game_value / nb_iterations << '\n'; 
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
            int offset = idx_to_offset[idx];
            const T* s = &probas_regrets_and_strategies[offset + n];
            T sum{};
            for (int i = 0; i < n; i++) {
                sum += s[i];
            }
            for (int i = 0; i < n; i++) {
                strategy.strategies.push_back(sum > 0 ? s[i] / sum : static_cast<T>(1.0) / n);
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
