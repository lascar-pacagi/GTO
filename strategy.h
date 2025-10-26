#ifndef STRATEGY_H
#define STRATEGY_H
#include "misc.h"
#include <random>
#include <chrono>
#include "list.h"
#include <map>
#include <iostream>
#include <iomanip>

template<typename Game>
struct Strategy {
    static thread_local inline std::mt19937_64 mt{static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
    using InfoSet = Game::InfoSet;
    using Action  = Game::Action;
    std::map<InfoSet, int> info_set_to_idx;
    std::map<InfoSet, int> info_set_to_nb_actions;
    std::vector<Action> actions;
    std::vector<double> strategies;

    Action get_action(InfoSet info_set) const {
        int idx = info_set_to_idx.at(info_set);
        int n = info_set_to_nb_actions.at(info_set);
        std::uniform_real_distribution<double> d(0, 1);
        double r = d(mt);
        double sum{};
        for (int i = 0; i < n - 1; i++) {
            sum += strategies[idx + i];
            if (r < sum) return actions[idx + i];
        }
        return actions[idx + n - 1];
    }
    const double* get_strategy(const InfoSet& info_set) const {
        int idx = info_set_to_idx.at(info_set);
        return &strategies[idx];
    }
    const Action* get_actions(const InfoSet& info_set) const {
        int idx = info_set_to_idx.at(info_set);
        return &actions[idx];
    }
};

template<typename Game>
std::ostream& operator<<(std::ostream& os, const Strategy<Game>& s) {
    for (const auto [info_set, idx] : s.info_set_to_idx) {
        int n = s.info_set_to_nb_actions.at(info_set);
        os << info_set << ' ';
        for (int i = 0; i < n; i++) {
            os << '(' << s.actions[idx + i] << ',' << std::setprecision(5) << s.strategies[idx + i] << ") ";
        }
        os << '\n';
    }
    return os;
}

#endif
