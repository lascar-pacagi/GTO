#include "tictactoe.h"
#include "simple_poker.h"
#include "Kuhn.h"
#include "game_tree.h"
#include "cfr.h"
#include "strategy.h"
#include <bits/stdc++.h>
using namespace std;

int main() {
    // constexpr int NB_ITERATIONS = 100000000;
    using Game = Kuhn;
    GameTree<Game> tree;
    cout << tree << '\n';
    CFR<Game> cfr(tree);
    cfr.solve(10000000);
    Strategy<Game> strategy = cfr.get_strategy();
    cout << strategy << '\n';
    // TicTacToe ttt;
    // for (;;) {
    //     ttt.reset();
    //     cout << ttt << '\n';
    //     cout << ttt.get_info_set(ttt.current_player()) << '\n';
    //     while (!ttt.game_over()) {            
    //         int a;
    //         cin >> a;
    //         ttt.play(TicTacToe::Action(a));
    //         cout << ttt << '\n';
    //         cout << ttt.get_info_set(ttt.current_player()) << '\n';
    //     }
    //     cout << "(" << ttt.payoff(PLAYER1) << ',' << ttt.payoff(PLAYER2) << ")\n";
    // }

    //set<pair<int, uint32_t>> states;
    // PRNG prng{static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
    // ActionList<Kuhn::Action, 3> actions;
    // for (int i = 0; i < 100; i++) {
    //     Kuhn kuhn;  
    //     cout << "#########################\n";      
    //     while (!kuhn.game_over()) {
    //         cout << "* Kuhn\n";
    //         cout << kuhn << endl;
    //         cout << "* Kuhn\n";
    //         cout << "CURRENT PLAYER: " << kuhn.current_player() << '\n';
    //         Kuhn::Action a;
    //         if (kuhn.is_chance_node()) {
    //             a = kuhn.sample_action();
    //         } else {
    //             kuhn.actions(actions);
    //             cout << "** Actions\n";
    //             for (int i = 0; i < actions.size(); i++) {
    //                 cout << actions[i] << ' ';
    //             }
    //             cout << "\n** Actions\n";
    //             a = actions[reduce(prng.rand<uint32_t>(), actions.size())];
    //         }
    //         cout << "*** Info set\n";
    //         auto info_set = kuhn.get_info_set(kuhn.current_player());
    //         if (kuhn.current_player() == PLAYER1) {
    //             cout << (info_set >> 15) << '\n';
    //             cout << Kuhn::Action(info_set & 0x7) << ' ';
    //             cout << Kuhn::Action((info_set >> 3) & 0x7) << ' ';
    //             cout << Kuhn::Action((info_set >> 6) & 0x7) << ' ';
    //             cout << Kuhn::Action((info_set >> 9) & 0x7) << ' ';
    //             cout << Kuhn::Action((info_set >> 12) & 0x7) << ' ';
    //         } else {
    //             cout << (info_set >> 15) << '\n';
    //             cout << Kuhn::Action(info_set & 0x7) << ' ';
    //             cout << Kuhn::Action((info_set >> 3) & 0x7) << ' ';
    //             cout << Kuhn::Action((info_set >> 6) & 0x7) << ' ';
    //             cout << Kuhn::Action((info_set >> 9) & 0x7) << ' ';
    //             cout << Kuhn::Action((info_set >> 12) & 0x7) << ' ';
    //         }
    //         cout << "\n*** Info set\n";
    //         cout << "--- " << a << endl;
    //         kuhn.play(a);
    //         // cout << kuhn << endl;
    //         kuhn.undo(a);
    //         kuhn.play(a);
    //         // auto state = kuhn.get_state();
    //         // Kuhn kk;
    //         // kk.set_state(state);
    //         // cout << kk << endl;            
    //     }
    //     cout << kuhn << '\n';
    //     cout << kuhn.payoff(PLAYER1) << '\n';
    //     string _;
    //     getline(cin, _);       
    //     //states.insert(make_pair(kuhn.nb_actions, kuhn.action_history));
    // }
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

    // set<SimplePoker::State> states;
    // PRNG prng{static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
    // List<SimplePoker::Action, SimplePoker::MAX_NB_ACTIONS> actions;
    // for (int i = 0; i < 100; i++) {
    //     SimplePoker simple_poker;  
    //     while (!simple_poker.game_over()) {
    //         SimplePoker::Action a;
    //         if (simple_poker.is_chance_node()) {
    //             a = simple_poker.sample_action();
    //         } else {
    //             simple_poker.actions(actions);
    //             a = actions[reduce(prng.rand<uint32_t>(), actions.size())];
    //         }
    //         simple_poker.play(a);
    //     }
    //     states.insert(simple_poker.action_history);
    // }
    // cout << "size: " << states.size() << '\n';
    // for (const auto [nb_actions, history] : states) {
    //     cout << history << ' ' << std::bitset<32>(history) << '\n';
    //     Kuhn k;
    //     k.action_history = history;
    //     k.nb_actions = nb_actions;
    //     cout << k << '\n';
    // }
    /*
    131126 [HHHHkk] 0
    176694 [HHHHkbf] -1
    172598 [HHHHkbc] 0
    132726 [HHHHbf] 1
    132214 [HHHHbc] 0
    131118 [HHLLkk] 1
    176686 [HHLLkbf] -1
    172590 [HHLLkbc] 3
    132718 [HHLLbf] 1
    132206 [HHLLbc] 3
    131110 [HHHLkk] 1
    176678 [HHHLkbf] -1
    172582 [HHHLkbc] 3
    132710 [HHHLbf] 1
    132198 [HHHLbc] 3
    131125 [LLHHkk] -1
    176693 [LLHHkbf] -1
    172597 [LLHHkbc] -3
    132725 [LLHHbf] 1
    132213 [LLHHbc] -3
    131117 [LLLLkk] 0
    176685 [LLLLkbf] -1
    172589 [LLLLkbc] 0
    132717 [LLLLbf] 1
    132205 [LLLLbc] 0
    131109 [LLHLkk] 1
    176677 [LLHLkbf] -1
    172581 [LLHLkbc] 3
    132709 [LLHLbf] 1
    132197 [LLHLbc] 3
    131124 [HLHHkk] -1
    176692 [HLHHkbf] -1
    172596 [HLHHkbc] -3
    132724 [HLHHbf] 1
    132212 [HLHHbc] -3
    131116 [HLLLkk] -1
    176684 [HLLLkbf] -1
    172588 [HLLLkbc] -3
    132716 [HLLLbf] 1
    132204 [HLLLbc] -3
    131108 [HLHLkk] 0
    176676 [HLHLkbf] -1
    172580 [HLHLkbc] 0
    132708 [HLHLbf] 1
    132196 [HLHLbc] 0
    */
    /*
    cout << "search for magic\n";
    PRNG prng{static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
    int n = 5;
    //uint32_t mask = (1 << n) - 1;
    map<uint32_t, int> payoffs{
        {54, 0},
        {12854, -1},
        {8758, 0},
        {1654, 1},
        {1142, 0},
        {46, 1},
        {12846, -1},
        {8750, 3},
        {1646, 1},
        {1134, 3},
        {38, 1},
        {12838, -1},
        {8742, 3},
        {1638, 1},
        {1126, 3},
        {53, -1},
        {12853, -1},
        {8757, -3},
        {1653, 1},
        {1141, -3},
        {45, 0},
        {12845, -1},
        {8749, 0},
        {1645, 1},
        {1133, 0},
        {37, 1},
        {12837, -1},
        {8741, 3},
        {1637, 1},
        {1125, 3},
        {52, -1},
        {12852, -1},
        {8756, -3},
        {1652, 1},
        {1140, -3},
        {44, -1},
        {12844, -1},
        {8748, -3},
        {1644, 1},
        {1132, -3},
        {36, 0},
        {12836, -1},
        {8740, 0},
        {1636, 1},
        {1124, 0},
    };    
    for (;;) {
        uint32_t magic = prng.rand<uint32_t>();
        map<uint32_t, int> seen;
        bool ok = true;
        for (const auto [state, payoff] : payoffs) {
            uint32_t key = state * magic >> (32 - n);
            if (seen.count(key) == 0) {
                seen[key] = payoff;
            } else {
                int v = seen[key];
                if (v != payoff) {
                    ok = false;
                    break;
                }
            }
        }
        if (ok) {
            cout << "found magic!: " << magic << " for n = " << n << endl;            
            for (const auto [key, v] : seen) {
                cout << key << ',' << v << '\n';
            }
            break;
        }
    }
    */   
}
