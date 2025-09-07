#ifndef STRATEGY_H
#define STRATEGY_H
#include "misc.h"
#include <chrono>
#include "list.h"

// template <typename Game>
// struct Strategy {
//     mutable PRNG prng{static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
//     float strategy[Game::STATE_SPACE_SIZE][Game::MAX_NB_ACTIONS]{};    
//     typename Game::Action get_action(typename Game::State state, 
//                                      const ActionList<typename Game::Action, Game::MAX_NB_ACTIONS>& actions) const {            
        
//         uint32_t p[Game::MAX_NB_ACTIONS];
//         constexpr uint32_t COEFF = 1000;
//         p[0] = strategy[0] * COEFF;
//         int nb_actions = actions.size();
//         for (int i = 1; i < nb_actions; i++) {
//             p[i] = p[i - 1] + strategy[i] * COEFF;
//         }
//         uint32_t r = reduce(prng.rand<uint32_t>(), p[nb_actions - 1]);
//         for (int i = 0; i < nb_actions - 1; i++) {
//             if (r < p[i]) {
//                 return actions[i];
//             }
//         }
//         return actions[nb_actions - 1];
//     }
// };

#endif
