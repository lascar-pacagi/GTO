#include "Kuhn.h"
#include <bits/stdc++.h>
using namespace std;

int main() {
    //set<pair<int, uint32_t>> states;
    PRNG prng{static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
    ActionList<Kuhn::Action, 3> actions;
    for (int i = 0; i < 100; i++) {
        Kuhn kuhn;  
        cout << "#########################\n";      
        while (!kuhn.game_over()) {
            Kuhn::Action a;
            if (kuhn.is_chance_node()) {
                a = kuhn.sample_action();
            } else {
                kuhn.actions(actions);
                a = actions[reduce(prng.rand<uint32_t>(), actions.size())];
            }   
            cout << a << endl;         
            kuhn.play(a);
        }
        cout << kuhn << '\n';
        cout << kuhn.payoff(PLAYER1) << '\n';       
        //states.insert(make_pair(kuhn.nb_actions, kuhn.action_history));
    }
    //cout << "size: " << states.size() << '\n';
    // for (const auto [nb_actions, history] : states) {
    //     cout << history << ' ' << std::bitset<32>(history) << '\n';
    //     Kuhn k;
    //     k.action_history = history;
    //     k.nb_actions = nb_actions;
    //     cout << k << '\n';
    // }
    // cout << "search for magic\n";
    // int n = 5;
    // uint32_t mask = (1 << n) - 1;
    // map<uint32_t, int> payoffs{
    //     {37, 1}, {38, 1}, {44, -1}, {46, 1}, {52, -1}, {53, -1},
    //     {1125, 2}, {1126, 2}, {1132, -2}, {1134, 2}, {1140, -2}, {1141, -2},
    //     {1637, 1}, {1638, 1}, {1644, 1}, {1646, 1}, {1652, 1}, {1653, 1},
    //     {8741, 2}, {8742, 2}, {8748, -2}, {8750, 2}, {8756, -2}, {8757, -2},
    //     {12837, -1}, {12838, -1}, {12844, -1}, {12846, -1}, {12852, -1}, {12853, -1},
    // };
    // for (;;) {
    //     uint32_t magic = prng.rand<uint32_t>();
    //     map<uint32_t, int> seen;
    //     bool ok = true;
    //     for (const auto [_, state] : states) {
    //         int payoff = payoffs[state];
    //         uint32_t key = state * magic >> (32 - n);
    //         if (seen.count(key) == 0) {
    //             seen[key] = payoff;
    //         } else {
    //             int v = seen[key];
    //             if (v != payoff) {
    //                 ok = false;
    //                 break;
    //             }
    //         }
    //     }
    //     if (ok) {
    //         cout << "found magic!: " << magic << " for n = " << n << endl;            
    //         for (const auto [key, v] : seen) {
    //             cout << key << ',' << v << '\n';
    //         }
    //         break;
    //     }
    // }
}
