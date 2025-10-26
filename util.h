#ifndef UTIL_H
#define UTIL_H

#include "game.h"
#include "game_tree.h"
#include "strategy.h"
#include <iostream>

template<typename Game>
void play(const Game& game, const Strategy<Game>& s1, const Strategy<Game>& s2) {
    Game g;
    std::cout << g << '\n';
    while (!g.game_over()) {
        typename Game::Action a;            
        if (g.is_chance_player()) {
            a = g.sample_action();
            std::cout << "chance: " << a << '\n';                    
        } else {
            a = (g.current_player() == PLAYER1 ? s1 : s2).get_action(g.get_info_set(g.current_player()));
            std::cout << "action: " << a << '\n';
        }
        g.play(a);
        std::cout << g << '\n';   
    }
}

namespace {
    template<typename Game>
    double evaluate(int idx, const GameTree<Game>& tree, const Strategy<Game>& s1, const Strategy<Game>& s2) {
        int start = tree.start_children_and_actions[idx];
        int n = tree.nb_children[idx] >> 2;
        if (n == 0) {
            return static_cast<double>(tree.children[start]);
        }
        int player = tree.nb_children[idx] & 3;
        if (player == CHANCE) {
            double sum{};
            for (int i = 0; i < n; i++) {
                sum += tree.children[start + 2*i + 1];
            }
            double u{};
            for (int i = 0; i < n; i++) {
                double p = static_cast<double>(tree.children[start + 2*i + 1]) / sum;
                double v = evaluate(tree.children[start + 2*i], tree, s1, s2);
                u += p * v;
            }
            return u;
        }
        const typename Game::InfoSet& info_set = tree.info_sets[idx];
        const double* strategy = (player == PLAYER1 ? s1 : s2).get_strategy(info_set);
        double u{};
        for (int i = 0; i < n; i++) {
            u += strategy[i] * evaluate(tree.children[start + i], tree, s1, s2);
        }
        return u;
    }
}

template<typename Game>
void evaluate(const GameTree<Game>& tree, const Strategy<Game>& s1, const Strategy<Game>& s2) {
    double v = evaluate(0, tree, s1, s2);
    std::cout << "Game value for player 1: " << v << '\n';
    std::cout << "Game value for player 2: " << -v << '\n';
}

#endif
