#include "postflop_poker.h"
#include <iostream>

int main() {
    // Test with a clearer winner
    std::array<uint8_t, 3> flop = {0, 1, 2};  // 2c, 3c, 4c
    std::array<uint8_t, 2> p1_hand = {12, 25};  // Ac, Ah - has flush
    std::array<uint8_t, 2> p2_hand = {24, 37};  // Kh, Ks - has pair of kings

    PostflopPoker game(flop, p1_hand, p2_hand, 20, 100);

    std::cout << "Postflop Poker Test 2\n";
    std::cout << "=====================\n";
    std::cout << "Flop: 2c 3c 4c\n";
    std::cout << "P1: Ac Ah\n";
    std::cout << "P2: Kh Ks\n\n";

    // Play to showdown quickly
    game.play(PostflopPoker::CHECK);
    game.play(PostflopPoker::CHECK);

    // Deal turn (5c - gives P1 a flush)
    game.play(PostflopPoker::Action(3));  // 5c
    std::cout << "Turn: 5c\n";

    game.play(PostflopPoker::CHECK);
    game.play(PostflopPoker::CHECK);

    // Deal river
    game.play(PostflopPoker::Action(13));  // 2d
    std::cout << "River: 2d\n";

    game.play(PostflopPoker::CHECK);
    game.play(PostflopPoker::CHECK);

    std::cout << "\nGame over: " << game.game_over() << "\n";

    if (game.game_over()) {
        // Check the cards being evaluated
        std::cout << "\nP1 has: Ac Ah with board 2c 3c 4c 5c 2d\n";
        std::cout << "P2 has: Kh Ks with board 2c 3c 4c 5c 2d\n";

        int winner = game.evaluate_showdown();
        std::cout << "\nWinner value: " << winner << "\n";
        std::cout << "Winner: ";
        if (winner == -1) std::cout << "Tie\n";
        else if (winner == PLAYER1) std::cout << "P1 (should win with flush)\n";
        else if (winner == PLAYER2) std::cout << "P2\n";
        else std::cout << "Unknown\n";

        std::cout << "P1 payoff: " << game.payoff(PLAYER1) << "\n";
        std::cout << "P2 payoff: " << game.payoff(PLAYER2) << "\n";
    }

    return 0;
}
