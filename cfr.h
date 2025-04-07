#ifndef CFR_H
#define CFR_H
#include <unordered_map>
#include "game.h"
#include "action_list.h"
#include <iostream>

template<typename Game>
struct CFR {
    struct Node {
        float regrets[Game::MAX_NB_ACTIONS];
        float strategies[Game::MAX_NB_ACTIONS];
        int nb_actions;
        void current_strategy(float* strategy) const {
            float sum = 0;
            for (int i = 0; i < nb_actions; i++) {
                if (regrets[i] > 0) sum += regrets[i];
            }
            for (int i = 0; i < nb_actions; i++) {
                if (regrets[i] > 0) {
                    
                }
            }
        }
        void equilibrium(float* strategy) const {

        }
    };
    std::unordered_map<typename Game::State, Node> nodes;
    struct Strategy {

    };
    Node& get_node(typename Game::State state, int nb_actions) {

    }
    float cfr(Game& g, int player, float pi1, float pi2) {
        if (g.game_over()) {
            return g.payoff(player);
        }
        if (g.is_chance_node()) {
            auto action = g.sample_action();
            g.play(action);
            float v = cfr(g, player, pi1, pi2);
            g.undo(action);
            return v;
        }
        ActionList actions;
        g.actions(actions);        
        Node& n = get_node(g.get_state(), actions.size());
        float s[Game::MAX_NB_ACTIONS];
        n.current_strategy(s);
        float u = 0;
        float utils[Game::MAX_NB_ACTIONS];
        for (int i = 0; i < n.nb_actions; i++) {
            auto a = actions[i];
            g.play(a);
            if (player == PLAYER1) {
                utils[i] = cfr(g, player, s[i] * pi1, pi2);
            } else {
                utils[i] = cfr(g, player, pi1, s[i] * pi2);
            }
            g.undo(a);
            u += s[i] * utils[i];
        }
        if (g.current_player() == player) {
            for (int i = 0; i < n.nb_actions; i++) {
                n.regrets[i] += (player == PLAYER1 ? pi2 : pi1) * (utils[i] - u);
                n.strategies[i] += (player == PLAYER1 ? pi1 : pi2) * s[i];
            }
        }
        return u;
    }
    void solve(size_t nb_iterations) {
        float game_value[2]{};
        for (size_t i = 0; i < nb_iterations; i++) {
            for (auto player : { Player1, Player2 }) {
                Game g;
                game_value[player] += cfr(g, player, 1, 1);                
            }
        }
        std::cout << game_value[PLAYER1] / nb_iterations << std::endl;
        std::cout << game_value[PLAYER2] / nb_iterations << std::endl;
    }
};

#endif
