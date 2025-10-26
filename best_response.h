#ifndef BEST_RESPONSE_H
#define BEST_RESPONSE_H
#include "game.h"
#include "game_tree.h"
#include "strategy.h"

namespace {
    template<typename Game>
    double fill_best_response(int idx, const GameTree<Game>& tree, const Strategy<Game>& s, int player,
                            std::map<typename Game::InfoSet, double>& values, Strategy<Game>& best_response) {
        int start = tree.start_children_and_actions[idx];
        int n = tree.nb_children[idx] >> 2;
        if (n == 0) {
            return static_cast<double>(tree.children[start]) * (player == PLAYER1 ? 1 : -1);
        }
        int current_player = tree.nb_children[idx] & 3;
        if (current_player == CHANCE) {
            double sum{};
            for (int i = 0; i < n; i++) {
                sum += tree.children[start + 2*i + 1];
            }
            double u{};
            for (int i = 0; i < n; i++) {
                double p = static_cast<double>(tree.children[start + 2*i + 1]) / sum;
                double v = fill_best_response(tree.children[start + 2*i], tree, s, player, values, best_response);
                u += p * v;
            }
            return u;
        }
        const typename Game::InfoSet& info_set = tree.info_sets[idx];
        if (current_player != player) {
            const double* strategy = s.get_strategy(info_set);
            double u{};
            for (int i = 0; i < n; i++) {
                u += strategy[i] * fill_best_response(tree.children[start + i], tree, s, player, values, best_response);
            }
            return u;
        }
        if (values.contains(info_set)) {
            return values[info_set];
        }
        std::array<double, Game::MAX_NB_PLAYER_ACTIONS> utils{};
        double proba_sum{};
        //std::cout << idx << " info set: " << info_set << '\n';
        for (const auto& state : tree.get_states(info_set)) {
            //std::cout << "state: " << state << '\n';
            double p = Game::chance_reach_proba(state);
            //std::cout << p << '\n';
            for (const auto& [i, a] : Game::info_sets_and_actions(state, (player == PLAYER1 ? PLAYER2 : PLAYER1))) {
                //std::cout << '(' << i << ',' << a << ")\n";
                const double* strategy = s.get_strategy(i);
                const typename Game::Action* actions = s.get_actions(i);
                // for (int k = 0; k < n; k++) {
                //     //std::cout << s.get_actions(i)[k] << ' ' << strategy[k] << " | ";
                //     std::cout << s.get_actions(i)[k] << ' ' << strategy[k] << " | ";
                // }
                // std::cout << '\n';
                p *= strategy[std::find(actions, actions + n, a) - actions];
            }
            proba_sum += p;
            int start = tree.start_children_and_actions[tree.get_state_idx(state)];
            for (int i = 0; i < n; i++) {
                utils[i] += p * fill_best_response(tree.children[start + i], tree, s, player, values, best_response);
            }
        }
        //std::cout << "end " << idx << '\n';
        double best_value = std::numeric_limits<double>::lowest();
        int best_action = -1;
        for (int i = 0; i < n; i++) {
            if (utils[i] > best_value) {
                best_value = utils[i];
                best_action = i;
            }
        }
        int offset = static_cast<int>(best_response.actions.size());
        best_response.info_set_to_idx[info_set] = offset;
        best_response.info_set_to_nb_actions[info_set] = n;
        for (int i = 0; i < n; i++) {
            best_response.actions.push_back(tree.actions[start + i]);
            best_response.strategies.push_back(0.0);
        }
        best_response.strategies[offset + best_action] = 1.0;
        double u = proba_sum == 0.0 ? 0.0 : (best_value / proba_sum);
        values[info_set] = u;
        return u;
    }
}

template<typename Game>
Strategy<Game> best_response(const GameTree<Game>& tree, const Strategy<Game>& s, int player) {
    Strategy<Game> res;
    std::map<typename Game::InfoSet, double> values;
    (void)fill_best_response(0, tree, s, player, values, res);
    return res;
}

#endif
