#include "postflop_poker.h"
#include <iostream>

int main() {
    // Direct test of hand evaluator
    PostflopHandEvaluator eval;

    // Test 1: Flush (2c 3c 4c 5c Ac + Ah 2d)
    uint8_t flush_hand[7] = {0, 1, 2, 3, 12, 25, 13};  // 2c 3c 4c 5c Ac Ah 2d
    uint16_t flush_rank = eval.evaluate7(flush_hand);
    std::cout << "Flush hand rank: " << flush_rank << "\n";

    // Test 2: Straight (2c 3c 4c 5c 2d + Kh Ks)
    uint8_t straight_hand[7] = {0, 1, 2, 3, 13, 24, 37};  // 2c 3c 4c 5c 2d Kh Ks
    uint16_t straight_rank = eval.evaluate7(straight_hand);
    std::cout << "Straight/Pair hand rank: " << straight_rank << "\n";

    if (flush_rank < straight_rank) {
        std::cout << "Flush wins (correct!)\n";
    } else if (straight_rank < flush_rank) {
        std::cout << "Straight/Pair wins (incorrect!)\n";
    } else {
        std::cout << "Tie (incorrect!)\n";
    }

    return 0;
}
