#include "postflop_poker.h"
#include <iostream>

int main() {
    // Test 1: Create a simple postflop game
    std::array<uint8_t, 3> flop = {0, 13, 26};  // 2c, 2d, 2h
    std::array<uint8_t, 2> p1_hand = {12, 25};  // Ac, Ah
    std::array<uint8_t, 2> p2_hand = {11, 24};  // Kc, Kh

    PostflopPoker game(flop, p1_hand, p2_hand, 20, 100);

    std::cout << "Postflop Poker Test\n";
    std::cout << "===================\n";
    std::cout << "Flop: 2c 2d 2h\n";
    std::cout << "P1: Ac Ah\n";
    std::cout << "P2: Kc Kh\n";
    std::cout << game << "\n";

    // Test 2: Check current player
    std::cout << "\nCurrent player: " << game.current_player() << " (0=P1, 1=P2)\n";

    // Test 3: Get available actions
    List<PostflopPoker::Action, PostflopPoker::MAX_NB_ACTIONS> actions;
    game.actions(actions);

    std::cout << "Available actions: ";
    for (auto it = actions.list; it != actions.last; ++it) {
        std::cout << *it << " ";
    }
    std::cout << "\n";

    // Test 4: Play CHECK-CHECK and evaluate showdown
    game.play(PostflopPoker::CHECK);
    std::cout << "\nP1 checks. Current player: " << game.current_player() << "\n";

    game.play(PostflopPoker::CHECK);
    std::cout << "P2 checks. Current player: " << game.current_player() << "\n";
    std::cout << "Street: " << static_cast<int>(game.street) << "\n";

    // Test 5: Deal turn
    if (game.is_chance_player()) {
        game.play(PostflopPoker::Action(39));  // Deal Qc as turn
        std::cout << "\nDealt turn card\n";
        std::cout << "Street: " << static_cast<int>(game.street) << "\n";
    }

    // Test 6: More betting
    game.play(PostflopPoker::CHECK);
    game.play(PostflopPoker::CHECK);

    // Test 7: Deal river
    if (game.is_chance_player()) {
        game.play(PostflopPoker::Action(38));  // Deal Jc as river
        std::cout << "\nDealt river card\n";
    }

    // Test 8: Final betting
    game.play(PostflopPoker::CHECK);
    game.play(PostflopPoker::CHECK);

    // Test 9: Check game over and payoff
    std::cout << "\nGame over: " << game.game_over() << "\n";
    if (game.game_over()) {
        int winner = game.evaluate_showdown();
        std::cout << "Winner: " << (winner == 0 ? "Tie" : (winner == PLAYER1 ? "P1" : "P2")) << "\n";
        std::cout << "P1 payoff: " << game.payoff(PLAYER1) << "\n";
        std::cout << "P2 payoff: " << game.payoff(PLAYER2) << "\n";
    }

    std::cout << "\nTest completed successfully!\n";
    return 0;
}
