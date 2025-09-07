#ifndef GAME_H
#define GAME_H
#include "strategy.h"
#include <iostream>
#include <string>

constexpr int PLAYER1 = 0;
constexpr int PLAYER2 = 1;
constexpr int CHANCE  = 2;

// template <typename Game>
// void play(const Strategy<Game>& s1, const Strategy<Game>& s2, size_t nb_iterations) {
//         double game_value[2]{};
//         Game g;
//         for (size_t i = 0; i < nb_iterations; i++) {
//             g.reset();
//             while (!g.game_over()) {
//                 typename Game::Action a;
//                 if (g.is_chance_node()) {
//                     a = g.sample_action();                    
//                 } else {
//                     ActionList<typename Game::Action, Game::MAX_NB_ACTIONS> actions;
//                     g.actions(actions);
//                     a = (g.current_player() == PLAYER1 ? s1 : s2).get_action(g.get_info_set(g.current_player()), actions);
//                 }  
//                 g.play(a);                 
//             }
//             game_value[PLAYER1] += g.payoff(PLAYER1);
//             game_value[PLAYER2] += g.payoff(PLAYER2);
//         }
//         std::cout << game_value[PLAYER1] / nb_iterations << '\n';
//         std::cout << game_value[PLAYER2] / nb_iterations << '\n';
//     }

#endif