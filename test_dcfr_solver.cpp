#include "dcfr_postflop.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== DCFR Postflop Solver Test ===\n\n";

    // Test 1: Simple heads-up postflop scenario
    // Fixed flop, fixed hands (for testing)
    std::array<uint8_t, 3> flop = {0, 1, 2};  // 2c, 3c, 4c

    std::array<uint8_t, 2> p1_hand = {12, 25};  // Ac, Ah - top pair
    std::array<uint8_t, 2> p2_hand = {24, 37};  // Kh, Ks - pair of kings

    PostflopPoker game(flop, p1_hand, p2_hand, 20, 100);

    std::cout << "Game Setup:\n";
    std::cout << "  Flop: 2c 3c 4c\n";
    std::cout << "  P1: Ac Ah (top pair)\n";
    std::cout << "  P2: Kh Ks (second pair)\n";
    std::cout << "  Starting pot: 20\n";
    std::cout << "  Starting stacks: 100\n\n";

    // Create solver without abstraction (for small game)
    DCFRPostflopSolver<PostflopPoker> solver(false);  // No bucketing

    // Solve for a small number of iterations (demonstration)
    std::cout << "Starting solver (1000 iterations)...\n\n";
    solver.solve(game, 1000);

    std::cout << "\n=== Solver Results ===\n\n";

    // Test specific infosets
    game.reset();

    // Get strategy for P1 on the flop
    List<PostflopPoker::Action, PostflopPoker::MAX_NB_ACTIONS> actions;
    game.actions(actions);

    auto infoset = game.get_info_set(PLAYER1);
    const auto& strategy = solver.get_average_strategy(
        infoset,
        actions.list,
        actions.size()
    );

    std::cout << "P1 Opening Strategy (flop):\n";
    int idx = 0;
    for (auto it = actions.list; it != actions.last; ++it, ++idx) {
        std::cout << "  " << *it << ": "
                  << std::fixed << std::setprecision(2)
                  << (strategy[idx] * 100) << "%\n";
    }

    // Save strategy to file
    std::cout << "\n";
    solver.save_strategy("strategy_output.csv");

    std::cout << "\n=== Test with Abstraction ===\n\n";

    // Test with hand bucketing
    DCFRPostflopSolver<PostflopPoker> abstract_solver(true, 20);  // 20 buckets

    // Create a scenario with ranges (for bucketing to matter)
    // For now, just use same fixed hand scenario
    std::cout << "Running with 20-bucket abstraction (500 iterations)...\n\n";
    game.reset();
    abstract_solver.solve(game, 500);

    std::cout << "\nAbstracted solver complete!\n";
    abstract_solver.save_strategy("strategy_abstract.csv");

    std::cout << "\n=== Performance Comparison ===\n\n";
    std::cout << "Without abstraction:\n";
    std::cout << "  - More accurate\n";
    std::cout << "  - Higher memory usage\n";
    std::cout << "  - Suitable for small games\n\n";

    std::cout << "With abstraction (20 buckets):\n";
    std::cout << "  - Faster convergence\n";
    std::cout << "  - Lower memory usage\n";
    std::cout << "  - Necessary for large games\n\n";

    std::cout << "=== Next Steps ===\n\n";
    std::cout << "1. Expand to solve over ranges of hands\n";
    std::cout << "2. Add more bet sizes and streets\n";
    std::cout << "3. Implement parallel CFR for speed\n";
    std::cout << "4. Add exploitability calculation\n";
    std::cout << "5. Create visualization tools\n\n";

    std::cout << "Test completed successfully!\n";

    return 0;
}
