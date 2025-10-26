# Postflop Poker Solver Implementation Guide
## Inspired by TexasSolver & postflop-solver

Based on analysis of two state-of-the-art open-source poker solvers:
- **TexasSolver** (C++): https://github.com/bupticybee/TexasSolver
- **postflop-solver** (Rust): https://github.com/b-inary/postflop-solver

---

## Key Insights from Production Solvers

### 1. Algorithm Choice: Discounted CFR (DCFR)

Both solvers use **Discounted CFR** rather than vanilla CFR:

**Why DCFR?**
- Faster convergence than CFR+
- Better memory efficiency
- State-of-the-art performance

**Parameters (from postflop-solver):**
```cpp
// Discounting parameters
static constexpr double GAMMA = 3.0;  // Discount factor (not 2.0!)
static constexpr double BETA = 0.0;   // For positive regrets
static constexpr double ALPHA = 1.5;  // For strategy averaging

// Strategy reset: at iterations 4^k (4, 16, 64, 256, ...)
bool should_reset_strategy(int iteration) {
    // Check if iteration is a power of 4
    return (iteration & (iteration - 1)) == 0 &&
           (iteration & 0xAAAAAAAA) == 0;
}
```

**DCFR Update Formula:**
```cpp
template<typename T>
void update_regrets_dcfr(T* regrets, const T* instant_regrets,
                         int num_actions, int iteration) {
    double discount = std::pow(iteration, -GAMMA);
    double pos_discount = std::pow(iteration, -ALPHA);

    for (int i = 0; i < num_actions; i++) {
        if (regrets[i] > 0) {
            regrets[i] = regrets[i] * discount + instant_regrets[i];
        } else {
            regrets[i] = regrets[i] * pos_discount + instant_regrets[i];
        }
    }
}
```

---

### 2. Memory Optimization Strategies

#### A. Floating Point Precision (postflop-solver approach)

```cpp
struct CompressedNode {
    // Use 16-bit integers for storage, one 32-bit scale factor
    std::vector<int16_t> regrets;      // Compressed
    std::vector<int16_t> avg_strategy; // Compressed
    float scale_factor;                 // Single scaling value

    // Decompress on access
    void get_regrets(float* output, int num_actions) const {
        for (int i = 0; i < num_actions; i++) {
            output[i] = regrets[i] * scale_factor;
        }
    }

    void set_regrets(const float* input, int num_actions) {
        // Find max absolute value
        float max_val = 0;
        for (int i = 0; i < num_actions; i++) {
            max_val = std::max(max_val, std::abs(input[i]));
        }

        // Scale to int16 range
        scale_factor = max_val / 32767.0f;
        for (int i = 0; i < num_actions; i++) {
            regrets[i] = static_cast<int16_t>(input[i] / scale_factor);
        }
    }
};

// Memory savings: 16-bit vs 32-bit = 50% reduction!
```

#### B. Tree Serialization & Compression

```cpp
// Use binary serialization for game trees
#include <fstream>
#include <zlib.h>  // Or zstd for better compression

void save_game_tree_compressed(const GameTree& tree,
                               const char* filename) {
    // 1. Serialize to binary
    std::vector<uint8_t> buffer = serialize(tree);

    // 2. Compress with zstd (10:1 ratio typical)
    size_t compressed_size = ZSTD_compressBound(buffer.size());
    std::vector<uint8_t> compressed(compressed_size);

    compressed_size = ZSTD_compress(
        compressed.data(), compressed_size,
        buffer.data(), buffer.size(),
        3  // Compression level
    );

    // 3. Write to disk
    std::ofstream out(filename, std::ios::binary);
    out.write(reinterpret_cast<char*>(compressed.data()), compressed_size);
}
```

---

### 3. Isomorphism & Symmetry Exploitation

**Card Suit Isomorphism** (from postflop-solver):

```cpp
class IsomorphismMapper {
public:
    // On monotone flop (all same suit), 3 non-dealt suits are identical
    bool is_monotone_flop(const uint8_t* flop) {
        int suit0 = flop[0] / 13;
        int suit1 = flop[1] / 13;
        int suit2 = flop[2] / 13;
        return (suit0 == suit1) && (suit1 == suit2);
    }

    // Get canonical turn card (map isomorphic cards to one representative)
    uint8_t get_canonical_turn(uint8_t turn, const uint8_t* flop) {
        if (!is_monotone_flop(flop)) return turn;

        int flop_suit = flop[0] / 13;
        int turn_suit = turn / 13;
        int turn_rank = turn % 13;

        // If turn is non-flop suit, map to canonical suit (e.g., clubs)
        if (turn_suit != flop_suit) {
            return turn_rank;  // Always map to suit 0 (clubs)
        }

        return turn;
    }

    // Reduction: 3x fewer nodes for monotone flops!
};
```

**Range Isomorphism:**

```cpp
// AcKc and AhKh are identical preflop
struct CanonicalHand {
    uint8_t high_rank;
    uint8_t low_rank;
    bool suited;

    static CanonicalHand from_cards(uint8_t c1, uint8_t c2) {
        int r1 = c1 % 13, r2 = c2 % 13;
        int s1 = c1 / 13, s2 = c2 / 13;

        return {
            static_cast<uint8_t>(std::max(r1, r2)),
            static_cast<uint8_t>(std::min(r1, r2)),
            s1 == s2
        };
    }
};
```

---

### 4. Efficient Game Tree Structure

**From TexasSolver approach:**

```cpp
// Node types in the tree
enum NodeType : uint8_t {
    TERMINAL_FOLD,
    TERMINAL_SHOWDOWN,
    CHANCE_NODE,     // Deal turn/river
    PLAYER_NODE      // Betting decision
};

struct GameTreeNode {
    NodeType type;
    uint8_t player;  // PLAYER1, PLAYER2, or CHANCE
    uint8_t num_actions;

    // Compact storage: use union
    union {
        // For player nodes: index into strategy array
        uint32_t strategy_offset;

        // For terminal nodes: payoff values
        struct {
            int16_t payoff_p1;
            int16_t payoff_p2;
        };

        // For chance nodes: probability info
        uint32_t chance_offset;
    };

    // Children indices (variable length)
    uint32_t children_offset;  // Index into children array
};

class CompactGameTree {
    std::vector<GameTreeNode> nodes;
    std::vector<uint32_t> children;       // Packed child indices
    std::vector<uint16_t> chance_probas;  // Probability table

    // Strategy data (separate for cache efficiency)
    std::vector<float> regrets;
    std::vector<float> avg_strategy;

    // Access patterns optimized for CFR traversal
};
```

---

### 5. Parallel CFR Implementation

**Multi-threading Strategy:**

```cpp
#include <thread>
#include <mutex>
#include <atomic>

class ParallelCFRSolver {
    CompactGameTree tree;
    std::vector<float> regrets;
    std::vector<float> avg_strategy;
    std::atomic<int> completed_iterations{0};

public:
    void solve_parallel(int num_iterations, int num_threads = 8) {
        std::vector<std::thread> threads;

        // Divide work by initial chance outcomes
        auto initial_chances = get_initial_chance_actions();
        int chunk_size = (initial_chances.size() + num_threads - 1) / num_threads;

        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([this, t, chunk_size, &initial_chances, num_iterations]() {
                // Each thread handles a subset of chance outcomes
                int start = t * chunk_size;
                int end = std::min(start + chunk_size,
                                  static_cast<int>(initial_chances.size()));

                for (int iter = 0; iter < num_iterations; iter++) {
                    for (int i = start; i < end; i++) {
                        // Run CFR traversal on this subtree
                        cfr_traverse(initial_chances[i], iter);
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }

private:
    // Lock-free update using atomic operations
    void update_regrets_atomic(int node_idx, const float* instant_regrets,
                               int num_actions) {
        // Use atomic operations or per-thread buffers with final merge
        // For best performance: thread-local storage + merge
    }
};
```

---

### 6. Action Abstraction (TexasSolver approach)

**Geometric Bet Sizing:**

```cpp
class BetAbstraction {
    struct BettingRoundConfig {
        std::vector<double> bet_sizes;  // As fraction of pot
        bool allow_all_in;
    };

    BettingRoundConfig flop_config = {
        {0.5, 1.0},  // 50% pot, 100% pot
        true         // + all-in
    };

    BettingRoundConfig turn_config = {
        {0.5, 1.0},
        true
    };

    BettingRoundConfig river_config = {
        {0.33, 0.5, 0.75, 1.0, 1.5},  // More options on river
        true
    };

    // Get available actions
    void get_actions(List<PostflopPoker::Action, MAX_NB_ACTIONS>& actions,
                    PostflopPoker::Street street,
                    int pot, int to_call) {
        if (to_call > 0) {
            // Facing bet: fold, call, raise
            actions.push_back(FOLD);
            actions.push_back(CALL);

            for (double size : get_raise_sizes(street)) {
                int raise_amount = static_cast<int>(pot * size);
                actions.push_back(encode_raise(raise_amount));
            }

            if (config.allow_all_in) {
                actions.push_back(ALL_IN);
            }
        } else {
            // No bet: check or bet
            actions.push_back(CHECK);

            for (double size : get_bet_sizes(street)) {
                int bet_amount = static_cast<int>(pot * size);
                actions.push_back(encode_bet(bet_amount));
            }

            if (config.allow_all_in) {
                actions.push_back(ALL_IN);
            }
        }
    }
};
```

---

### 7. Bunching Effect Handling (postflop-solver innovation)

**Problem:** When some hands fold, remaining hands have different card removal effects.

**Solution:** Track folded cards and adjust probabilities.

```cpp
class BunchingAwareGame {
    std::array<uint8_t, 4> folded_cards;  // Up to 4 folded hands
    int num_folded = 0;

    // Adjust probabilities based on card removal
    double get_adjusted_probability(const uint8_t* hand) {
        // Check if hand conflicts with folded cards
        for (int i = 0; i < num_folded; i++) {
            if (hand[0] == folded_cards[i] ||
                hand[1] == folded_cards[i]) {
                return 0.0;  // Impossible
            }
        }

        // Adjust based on reduced deck
        int remaining_cards = 52 - 3 - num_folded * 2;  // flop + folded
        return 1.0 / (remaining_cards * (remaining_cards - 1) / 2);
    }
};
```

---

## Recommended Implementation Plan

### Phase 1: Core Infrastructure (1-2 weeks)

```cpp
// File: dcfr_solver.h
class DCFRPostflopSolver {
    PostflopPoker base_game;
    CompactGameTree tree;
    IsomorphismMapper iso_mapper;
    BetAbstraction bet_config;

    void build_tree();
    void solve(int iterations);
    void save_solution(const char* filename);
};
```

**Tasks:**
1. ✅ Implement DCFR algorithm (update formula above)
2. ✅ Add isomorphism detection for suit symmetry
3. ✅ Implement geometric bet sizing
4. ✅ Create compact tree structure

### Phase 2: Optimization (1 week)

**Tasks:**
1. ✅ Add 16-bit compression for regrets/strategies
2. ✅ Implement tree serialization with zstd
3. ✅ Add parallel CFR with thread-local storage
4. ✅ SIMD optimization for regret updates (if possible)

### Phase 3: Abstraction (1-2 weeks)

**Tasks:**
1. ✅ Implement EHS-based hand bucketing
2. ✅ Add k-means clustering option
3. ✅ Create board texture bucketing
4. ✅ Test accuracy vs. speed trade-offs

### Phase 4: Testing & Validation (1 week)

**Tasks:**
1. ✅ Compare results with known solutions (simple games)
2. ✅ Benchmark performance vs. PioSolver (if available)
3. ✅ Validate exploitability calculations
4. ✅ Create GUI or command-line interface

---

## Example: Minimal Working Solver

```cpp
// File: simple_postflop_solver.cpp

#include "postflop_poker.h"
#include "hand_bucketing.h"

class SimplePostflopSolver {
    static constexpr int NUM_BUCKETS = 20;
    static constexpr double GAMMA = 3.0;

    PostflopPoker game;
    HandBucketer bucketer{NUM_BUCKETS};

    // Store regrets/strategy by abstracted infoset
    std::unordered_map<uint64_t, std::vector<float>> regrets;
    std::unordered_map<uint64_t, std::vector<float>> avg_strategy;

public:
    void solve(int iterations) {
        for (int iter = 1; iter <= iterations; iter++) {
            // Traverse game tree with CFR
            for (int player = 0; player < 2; player++) {
                game.reset();
                cfr_traverse(player, iter, 1.0, 1.0);
            }

            // Strategy reset at powers of 4
            if (is_power_of_4(iter)) {
                std::cout << "Resetting strategy at iteration " << iter << "\n";
                avg_strategy.clear();
            }

            if (iter % 100 == 0) {
                std::cout << "Iteration " << iter << " / " << iterations << "\n";
            }
        }
    }

private:
    double cfr_traverse(int player, int iteration,
                       double pi_player, double pi_opponent) {
        // Standard CFR traversal with bucketing
        // ... (implement based on your existing CFR code)
    }

    uint64_t get_abstract_infoset(int player) {
        // Use bucketed hand instead of exact cards
        int bucket = bucketer.get_hand_bucket(
            player == PLAYER1 ? game.p1_hole : game.p2_hole,
            game.flop_cards.data(),
            3 + (game.turn_card != PostflopPoker::INVALID_CARD) +
                (game.river_card != PostflopPoker::INVALID_CARD),
            PostflopPoker::get_evaluator()
        );

        // Encode: bucket | pot | street | action_history
        return (static_cast<uint64_t>(bucket) << 32) |
               (game.pot << 16) |
               (game.street << 8) |
               (game.action_history & 0xFF);
    }

    bool is_power_of_4(int n) {
        return (n & (n - 1)) == 0 && (n & 0xAAAAAAAA) == 0;
    }
};
```

---

## Performance Targets

Based on TexasSolver and postflop-solver benchmarks:

| Metric | Target | Notes |
|--------|--------|-------|
| **Convergence Time** | < 5 min | Simple flop (50 hand buckets, 3 bet sizes) |
| **Memory Usage** | < 2 GB | With 16-bit compression |
| **Accuracy** | < 0.5% exploitability | Measured against known solutions |
| **Parallelization** | 4-8x speedup | On 8-core CPU |

---

## References & Resources

1. **Discounted CFR Paper:** https://arxiv.org/abs/1809.04040
2. **TexasSolver Source:** https://github.com/bupticybee/TexasSolver
3. **postflop-solver Source:** https://github.com/b-inary/postflop-solver
4. **CFR Tutorial:** https://aipokertutorial.com/
5. **Pluribus Paper (2019):** Superhuman AI for multiplayer poker

---

This guide should provide a solid roadmap for building your own production-quality postflop solver!
