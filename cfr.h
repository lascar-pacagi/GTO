#ifndef CFR_H
#define CFR_H
#include <unordered_map>

template<typename Game, typename Action>
struct CFR {
    struct Node {
        float regrets[Game::MAX_NB_MOVES];
        float strategies[Game::MAX_NB_MOVES];
        void current_strategy(float* strategy) const;
        void equilibrium(float* strategy) const;
    };
    std::unordered_map<typename Game::State, Node> nodes;
    struct Strategy {

    };
    float cfr(Game& g, int player, float pi1, float pi2) {

    }
    void solve(size_t nb_iterations) {
        for (size_t i = 0; i < nb_iterations; i++) {
            for (auto player : { Game::Player1, Game::Player2 }) {
                Game g;
                cfr(g, player, 1, 1);
            }
        }
    }
};

#endif
