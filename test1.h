#ifndef TEST1_H
#define TEST1_H

#include <unordered_map>
#include <array>
#include <numeric>
#include <algorithm>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include "game.h"
#include "action_list.h"
#include <iostream>

template<typename Game>
struct Parallel_Vanilla_CFR {
    using Action = typename Game::Action;
    using State = typename Game::State;

    struct Node {
        std::array<double, Game::MAX_NB_ACTIONS> regrets{};
        std::array<double, Game::MAX_NB_ACTIONS> strategies{};
        int nb_actions = 0;

        void current_strategy(std::array<double, Game::MAX_NB_ACTIONS>& strategy) const {
            double sum = 0;
            for (int i = 0; i < nb_actions; ++i) {
                if (regrets[i] > 0) {
                    sum += regrets[i];
                }
            }

            if (sum > 0) {
                for (int i = 0; i < nb_actions; ++i) {
                    strategy[i] = (regrets[i] > 0) ? regrets[i] / sum : 0;
                }
            } else {
                for (int i = 0; i < nb_actions; ++i) {
                    strategy[i] = 1.0 / nb_actions;
                }
            }
        }

        void equilibrium(std::array<double, Game::MAX_NB_ACTIONS>& strategy) const {
            double sum = 0;
            for (int i = 0; i < nb_actions; ++i) {
                sum += strategies[i];
            }

            if (sum > 0) {
                for (int i = 0; i < nb_actions; ++i) {
                    strategy[i] = strategies[i] / sum;
                }
            } else {
                for (int i = 0; i < nb_actions; ++i) {
                    strategy[i] = 1.0 / nb_actions;
                }
            }
        }
    };

    std::unordered_map<State, Node> nodes;
    // We use unique_ptr because std::mutex is not copyable or movable.
    std::unordered_map<State, std::unique_ptr<std::mutex>> node_mutexes;
    std::mutex map_mutex; // Protects access to the maps themselves.

    std::atomic<double> total_game_value{0.0};

    // Returns utility from PLAYER1's perspective
    // Game state is passed by value to ensure each thread has its own copy.
    double cfr(Game g, double pi1, double pi2) {
        if (g.game_over()) {
            return g.payoff(PLAYER1);
        }

        if (g.is_chance_node()) {
            auto action = g.sample_action();
            g.play(action);
            // We can move g as it's a local copy and won't be used again.
            return cfr(std::move(g), pi1, pi2);
        }

        ActionList<Action, Game::MAX_NB_ACTIONS> actions;
        g.actions(actions);
        auto current_player = g.current_player();
        auto info_set = g.get_info_set(current_player);

        // Step 1: Ensure node and its mutex exist. This is a write operation on the maps,
        // so it must be protected by the global map_mutex.
        {
            std::lock_guard<std::mutex> lock(map_mutex);
            if (nodes.find(info_set) == nodes.end()) {
                nodes[info_set].nb_actions = actions.size();
                node_mutexes[info_set] = std::make_unique<std::mutex>();
            }
        }

        // Now we are guaranteed that the node and its mutex exist.
        Node& n = nodes.at(info_set);
        std::mutex& node_lock = *node_mutexes.at(info_set);

        // Step 2: Calculate current strategy. This is a read on the node's data.
        std::array<double, Game::MAX_NB_ACTIONS> s{};
        {
            std::lock_guard<std::mutex> lock(node_lock);
            n.current_strategy(s);
        }

        // Step 3: Recurse down the tree. This is done without holding any locks.
        double node_util = 0;
        std::array<double, Game::MAX_NB_ACTIONS> utils{};
        for (int i = 0; i < actions.size(); ++i) {
            auto a = actions[i];
            Game next_g = g; // Create a copy of the game state for this path
            next_g.play(a);
            utils[i] = (current_player == PLAYER1) ? cfr(std::move(next_g), s[i] * pi1, pi2) : cfr(std::move(next_g), pi1, s[i] * pi2);
            node_util += s[i] * utils[i];
        }

        // Step 4: Update regrets and strategies. This is a write on the node's data.
        {
            std::lock_guard<std::mutex> lock(node_lock);
            double self_reach_prob = (current_player == PLAYER1) ? pi1 : pi2;
            double other_reach_prob = (current_player == PLAYER1) ? pi2 : pi1;

            for (int i = 0; i < actions.size(); ++i) {
                double regret = (current_player == PLAYER1) ? (utils[i] - node_util) : -(utils[i] - node_util);
                n.regrets[i] += other_reach_prob * regret;
                n.strategies[i] += self_reach_prob * s[i];
            }
        }
        
        return node_util;
    }

    void solve(size_t nb_iterations) {
        unsigned int num_threads = std::thread::hardware_concurrency();
        std::cout << "\nRunning parallel vanilla CFR with " << num_threads << " threads." << std::endl;

        std::vector<std::thread> threads;
        total_game_value = 0.0;

        for (unsigned int i = 0; i < num_threads; ++i) {
            threads.emplace_back([this, nb_iterations, num_threads]() {
                Game g;
                double local_game_value = 0.0;
                for (size_t j = 0; j < nb_iterations / num_threads; ++j) {
                    g.reset();
                    local_game_value += this->cfr(g, 1.0, 1.0);
                }
                this->total_game_value += local_game_value; // One atomic write per thread
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        std::cout << "Average game value: " << total_game_value / nb_iterations << std::endl;
    }
};

#endif // TEST1_H
