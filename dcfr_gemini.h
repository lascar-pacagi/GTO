#ifndef DCFR_GEMINI_H
#define DCFR_GEMINI_H

#include <unordered_map>
#include <array>
#include <numeric>
#include <algorithm>
#include <cmath>
#include "game.h"
#include "action_list.h"
#include <iostream>

template<typename Game>
struct DCFR {
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
    const double alpha; // For strategy updates
    const double beta;  // For regret updates

    DCFR(double alpha = 1.5, double beta = 0.5, double gamma = 2.0)
        : alpha(alpha), beta(beta) {}

    Node& get_node(State state, int nb_actions) {
        Node& node = nodes[state];
        if (node.nb_actions == 0) {
            node.nb_actions = nb_actions;
        }
        return node;
    }

    // Returns utility from PLAYER1's perspective
    double cfr(Game& g, double pi1, double pi2, size_t t) {
        if (g.game_over()) {
            return g.payoff(PLAYER1);
        }

        if (g.is_chance_node()) {
            auto action = g.sample_action();
            g.play(action);
            double v = cfr(g, pi1, pi2, t);
            g.undo(action);
            return v;
        }

        ActionList<Action, Game::MAX_NB_ACTIONS> actions;
        g.actions(actions);
        auto current_player = g.current_player();
        auto info_set = g.get_info_set(current_player);
        Node& n = get_node(info_set, actions.size());

        std::array<double, Game::MAX_NB_ACTIONS> s{};
        n.current_strategy(s);

        double node_util = 0;
        std::array<double, Game::MAX_NB_ACTIONS> utils{};
        for (int i = 0; i < n.nb_actions; ++i) {
            auto a = actions[i];
            g.play(a);
            utils[i] = (current_player == PLAYER1) ? cfr(g, s[i] * pi1, pi2, t) : cfr(g, pi1, s[i] * pi2, t);
            g.undo(a);
            node_util += s[i] * utils[i];
        }

        double self_reach_prob = (current_player == PLAYER1) ? pi1 : pi2;
        double other_reach_prob = (current_player == PLAYER1) ? pi2 : pi1;
                
        double iter_alpha_weight = std::pow(static_cast<double>(t), alpha);
        double iter_beta_weight = std::pow(static_cast<double>(t), beta);

        for (int i = 0; i < n.nb_actions; ++i) {
            double regret = (current_player == PLAYER1) ? (utils[i] - node_util) : -(utils[i] - node_util);
            
            n.regrets[i] += other_reach_prob * iter_beta_weight * regret;
            if (n.regrets[i] < 0) { n.regrets[i] = 0; } // Regret Matching+

            n.strategies[i] += self_reach_prob * iter_alpha_weight * s[i];
        }

        return node_util;
    }

    void solve(size_t nb_iterations) {
        double game_value = 0;
        Game g;
        for (size_t t = 1; t <= nb_iterations; ++t) {
            g.reset();
            game_value += cfr(g, 1.0, 1.0, t);
        }
        std::cout << "Average game value (DCFR): " << game_value / nb_iterations << std::endl;
    }
};

#endif // DCFR_GEMINI_H
