#include "tictactoe.h"
#include "simple_poker.h"
#include "Kuhn.h"
#include "Leduc_no_raise.h"
#include "Leduc.h"
#include "game_tree.h"
#include "cfr.h"
#include "mccfr.h"
#include "strategy.h"
#include <omp.h>
#include "best_response.h"
#include "util.h"

using namespace std;

int main() {
    std::cout << "Threads available (omp_get_num_procs): " << omp_get_num_procs() << '\n';
    using Game = Leduc;
    GameTree<Game> tree;
    cout << tree << '\n';
    CFR<Game> cfr(tree);
    cfr.solve(1000000);
    Strategy<Game> strategy = cfr.get_strategy();
    cout << '\n' << strategy << '\n';
    // evaluate(tree, strategy, strategy);
    // Strategy<Game> br_player1 = best_response(tree, strategy, PLAYER1);
    // cout << '\n' << br_player1 << '\n';
    // evaluate(tree, br_player1, strategy);
    // Strategy<Game> br_player2 = best_response(tree, strategy, PLAYER2);
    // cout << '\n' << br_player2 << '\n';
    // evaluate(tree, strategy, br_player2);
    cout << "exploitability: " << cfr.exploitability() << '\n';
}
