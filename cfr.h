#ifndef CFR_H
#define CFR_H
#include "game.h"
#include "game_tree.h"
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
    const GameTree<Game>& tree;
    std::vector<Node> 
    CFR(const GameTree<Game>& tree) : tree(tree) {
    }    
    double cfr(Game& g, double pi1, double pi2, int iter) {
        if (g.game_over()) {
            return g.payoff(PLAYER1);
        }
        if (g.is_chance_node()) {
            auto action = g.sample_action();
            g.play(action);
            double v = cfr(g, pi1, pi2, iter);
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
            if (current_player == PLAYER1) {
                utils[i] = cfr(g, s[i] * pi1, pi2, iter);
            } else {
                utils[i] = cfr(g, pi1, s[i] * pi2, iter);
            }
            g.undo(a);
            u += s[i] * utils[i];
        }
        for (int i = 0; i < n.nb_actions; i++) {
            n.regrets[i] += (current_player == PLAYER1 ? pi2 : pi1) * (utils[i] - u) * (current_player == PLAYER1 ? 1 : -1);
            if (n.regrets[i] < 0) n.regrets[i] = 0;
            n.strategies[i] += (current_player == PLAYER1 ? pi1 : pi2) * s[i];
        }
        return u;
    }
    void solve(size_t nb_iterations) {
        Game g;
        double game_value = 0;
        for (size_t i = 1; i <= nb_iterations; i++) {
            g.reset();
            game_value += cfr(g, 1, 1, i);                        
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
        std::cout << game_value / nb_iterations << std::endl;
    }
};

#endif
