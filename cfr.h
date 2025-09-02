#ifndef CFR_H
#define CFR_H

#include <unordered_map>

#include "game.h"
#include "action_list.h"
#include <iostream>
#include <cstring>
#include <array>
#include "misc.h"
#include <memory>

template<typename Game>
struct CFR {

    struct Node {
        double regrets[Game::MAX_NB_ACTIONS]{};
        double strategies[Game::MAX_NB_ACTIONS]{};
        int nb_actions = 0;
        Node() = default;
        Node(int nb_actions) : nb_actions(nb_actions) {
        }
        void current_strategy(double* strategy) const {
            double sum = 0;
            for (int i = 0; i < nb_actions; i++) {
                if (regrets[i] > 0) sum += regrets[i];
            }
            if (sum != 0) {
                for (int i = 0; i < nb_actions; i++) {
                    if (regrets[i] > 0) {
                        strategy[i] = regrets[i] / sum;
                    } else {
                        strategy[i] = 0;
                    }
                }
            } else {
                double p = 1.0 / nb_actions;
                for (int i = 0; i < nb_actions; i++) {
                    strategy[i] = p;                    
                }
            }
        }
        void equilibrium(double* strategy) const {
            double sum = 0;
            for (int i = 0; i < nb_actions; i++) {
                sum += strategies[i];
            }
            if (sum != 0) {
                for (int i = 0; i < nb_actions; i++) {
                    strategy[i] = strategies[i] / sum;                    
                }
            } else {
                float p = 1.0 / nb_actions;
                for (int i = 0; i < nb_actions; i++) {
                    strategy[i] = p;    
                }
            }
        }
    };
    Node nodes[Game::STATE_SPACE_SIZE]{};
    Node& get_node(typename Game::State state, int nb_actions) {
        Node& node = nodes[state];
        node.nb_actions = nb_actions; 
        return node;
    }
    double cfr(Game& g, int player, double pi_player, double pi_other) {
        if (g.game_over()) {
            return g.payoff(player);
        }
        if (g.is_chance_node()) {
            auto action = g.sample_action();
            g.play(action);
            double v = cfr(g, player, pi_player, pi_other);
            g.undo(action);
            return v;
        }
        ActionList<typename Game::Action, Game::MAX_NB_ACTIONS> actions;
        g.actions(actions);
        auto current_player = g.current_player();
        Node& n = get_node(g.get_info_set(current_player), actions.size());
        double s[Game::MAX_NB_ACTIONS];
        n.current_strategy(s);
        double u = 0;
        double utils[Game::MAX_NB_ACTIONS];        
        for (int i = 0; i < n.nb_actions; i++) {
            auto a = actions[i];
            g.play(a);
            if (current_player == player) {
                utils[i] = cfr(g, player, s[i] * pi_player, pi_other);
            } else {
                utils[i] = cfr(g, player, pi_player, s[i] * pi_other);
            }
            g.undo(a);
            u += s[i] * utils[i];
        }
        if (current_player == player) {
            for (int i = 0; i < n.nb_actions; i++) {
                n.regrets[i] += pi_other * (utils[i] - u);
                n.strategies[i] += pi_player * s[i];
            }
        }                
        return u;
    }

    void solve(size_t nb_iterations) {
        double game_value[2]{};
        Game g;
        for (size_t i = 0; i < nb_iterations; i++) {
            for (auto player : { PLAYER1, PLAYER2 }) {
                g.reset();
                game_value[player] += cfr(g, player, 1, 1);
            }            
        }
        for (auto player : { PLAYER1, PLAYER2 }) {
            Node& n = nodes[player];
            double s[Game::MAX_NB_ACTIONS];
            n.equilibrium(s);
            for (int i = 0; i < 3; i++) {
                std::cout << s[i] << ' ';
            }
            std::cout << '\n';
        }
        // std::cout << game_value[PLAYER1] / nb_iterations << '\n';
        // std::cout << game_value[PLAYER2] / nb_iterations << '\n';
        std::cout << (game_value[PLAYER1] + game_value[PLAYER2]) / (2 * nb_iterations) << std::endl;
    }
};

#endif
