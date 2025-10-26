# Postflop Poker Game Tree Size Analysis

## Game Tree Size Estimation

### Without Abstraction

Let's estimate the tree size for a simple postflop scenario:

**Assumptions:**
- 2 players
- Fixed flop (3 cards)
- Range: Each player has R hand combinations (e.g., top 20% hands)
- Actions per betting round: {Check, Bet(1/2 pot), Bet(pot), Raise(pot), All-in} = 5-6 actions
- Streets: Flop, Turn, River

**Calculation:**

1. **Initial Hand Deals:** R₁ × R₂ combinations (removing conflicts)
   - Example: If each range has 200 combos → ~40,000 starting positions

2. **Flop Betting:**
   - Each player can take ~5 actions
   - Average betting sequences: ~10-20 paths per hand

3. **Turn Cards:**
   - 45 remaining cards (52 - 3 flop - 4 hole cards)
   - Multiplies tree by 45

4. **Turn Betting:**
   - Another ~10-20 betting paths

5. **River Cards:**
   - 44 remaining cards
   - Multiplies tree by 44

**Rough Estimate:**
- **Small ranges (50 combos each):** ~50×50 × 15 × 45 × 15 × 44 × 10 ≈ **100 billion nodes**
- **Medium ranges (200 combos each):** ~40,000 × 15 × 45 × 15 × 44 × 10 ≈ **10+ trillion nodes**
- **Large ranges (500+ combos each):** **100+ trillion nodes**

This is computationally intractable without abstraction!

---

## Abstraction Techniques to Reduce Tree Size

### 1. **Card Isomorphism** (Lossless Reduction)

**Concept:** Group hands that are strategically identical.

**Implementation:**
```cpp
// Suit isomorphism: AcKc ≡ AhKh ≡ AdKd ≡ AsKs (preflop)
// On monotone flop (all same suit), the 3 non-dealt suits are isomorphic

struct IsomorphicHands {
    // Map canonical hand to all isomorphic variants
    uint16_t get_canonical_hand(uint8_t card1, uint8_t card2) {
        // Normalize suits to canonical form
        // E.g., always map to clubs-diamonds-hearts-spades order
    }
};
```

**Reduction:** ~75% reduction in hand combinations preflop, ~50% postflop

**Best for:** Initial hand dealing, turn/river card isomorphisms

---

### 2. **Action Abstraction** (Lossy but Controlled)

**Concept:** Limit available bet sizes to a manageable set.

**Implementation:**
```cpp
enum BetSizes {
    CHECK,
    BET_33_POT,    // Instead of arbitrary sizes
    BET_50_POT,
    BET_75_POT,
    BET_POT,
    BET_150_POT,
    ALL_IN
};

// Or use geometric sizing:
// Bet sizes = {pot × r, pot × r², pot × r³, ...} where r ≈ 1.5-2.0
```

**Reduction:** From 100+ possible bet sizes → 3-6 sizes = **~95% reduction** in action space

**Trade-off:** Some exploitability, but minimal if bet sizes are well-chosen

---

### 3. **Card Bucketing / Information Abstraction** (Lossy)

**Concept:** Group similar hands into buckets and play them identically.

#### Method A: Expected Hand Strength (EHS)

```cpp
class HandBucketer {
    static constexpr int NUM_BUCKETS = 50;  // Typical: 50-200 buckets

    // Calculate EHS: E[HS] = P(win) + 0.5×P(tie)
    double calculate_ehs(const uint8_t* hole_cards,
                         const uint8_t* board,
                         int num_board_cards) {
        int wins = 0, ties = 0, total = 0;

        // Monte Carlo simulation
        for (int trial = 0; trial < 10000; trial++) {
            uint8_t opp_hole[2] = sample_opponent_hand();
            uint8_t full_board[5] = complete_board(board, num_board_cards);

            int result = compare_hands(hole_cards, opp_hole, full_board);
            if (result > 0) wins++;
            else if (result == 0) ties++;
            total++;
        }

        return (wins + 0.5 * ties) / total;
    }

    int get_bucket(double ehs) {
        // Map EHS [0,1] to bucket [0, NUM_BUCKETS-1]
        return static_cast<int>(ehs * NUM_BUCKETS);
    }
};
```

**Reduction:** From 1,326 hand combos → 50-200 buckets = **~95% reduction**

#### Method B: K-Means Clustering with Hand Features

```cpp
struct HandFeatures {
    double ehs;              // Expected hand strength
    double ehs2;             // E[HS²] (variance)
    double hand_potential;   // Probability of improving
    double negative_potential; // Probability of being outdrawn

    // Or use hand strength distribution histogram
    std::array<double, 10> strength_histogram;
};

// Cluster hands using k-means on feature vectors
std::vector<int> cluster_hands(const std::vector<HandFeatures>& features,
                                int num_clusters) {
    // Use k-means++ initialization
    // Iterate until convergence
    // Return cluster assignments
}
```

**Reduction:** Similar to EHS, but more accurate

---

### 4. **Imperfect Recall Abstraction** (Aggressive Lossy)

**Concept:** "Forget" previous betting history in certain situations.

**Example:**
```cpp
InfoSet get_abstracted_infoset(int player) const {
    // Instead of tracking full betting history:
    // Only remember: pot size, position, current street

    InfoSet info = 0;
    info |= static_cast<uint64_t>(get_hand_bucket(player)) << 0;
    info |= static_cast<uint64_t>(get_pot_bucket()) << 8;  // Bucket pot sizes too
    info |= static_cast<uint64_t>(street) << 16;

    // Forget exact betting sequence!
    return info;
}

int get_pot_bucket() const {
    // Coarse-grain pot sizes into 5-10 buckets
    if (pot < starting_pot * 1.5) return 0;
    if (pot < starting_pot * 2.5) return 1;
    // ...
}
```

**Reduction:** Can reduce tree by **10-100x** depending on granularity

**Trade-off:** Higher exploitability, but often necessary for large games

---

### 5. **Public State Bucketing** (For Turn/River)

**Concept:** Group similar board textures together.

```cpp
struct BoardTexture {
    int num_suits;          // 1=monotone, 2=two-tone, etc.
    bool has_pair;
    bool has_straight_draw;
    bool has_flush_draw;
    int high_card_rank;

    // Or use neural network to embed board → vector
};

// Only solve for representative boards from each bucket
```

**Reduction:** From 22,100 flops → ~200-1,000 representative flops = **~95% reduction**

---

## Hybrid Strategy: Multi-Level Abstraction

Combine techniques for maximum efficiency:

```cpp
struct AbstractedPostflopGame {
    // Level 1: Card isomorphism (lossless)
    IsomorphismMapper iso_mapper;

    // Level 2: Action abstraction (5-6 bet sizes)
    static constexpr std::array<double, 5> BET_SIZES = {0.33, 0.5, 0.75, 1.0, 1.5};

    // Level 3: Hand bucketing (50-200 buckets)
    HandBucketer bucketer;

    // Level 4: Public state abstraction
    BoardBucketer board_bucketer;

    // Combined tree size:
    // ~500 board buckets × 50 hand buckets P1 × 50 P2
    // × 5 actions × 45 turn cards × 5 actions × 44 river cards
    // ≈ 100 million nodes (vs 10+ trillion!)
};
```

---

## Modern Solver Approaches

### 1. **Disk-Based Storage** (Pluribus, Libratus)
- Store game tree on disk, load chunks as needed
- Use compression (1 byte per strategy entry)
- Can solve games with 10¹²+ nodes over days/weeks

### 2. **Solving on Subgames** (Safe Subgame Solving)
- Solve full game with coarse abstraction
- When playing, re-solve local subgames with finer abstraction
- Used in Libratus to defeat human pros

### 3. **Neural Network Approximation** (DeepStack, ReBeL)
- Train neural network to approximate values
- Use network as heuristic instead of solving to leaf nodes
- Reduces memory requirements drastically

### 4. **Monte Carlo CFR (MCCFR)**
- Sample subset of actions/chance nodes each iteration
- Converges slower but uses less memory
- Already implemented in your codebase!

---

## Practical Implementation Recommendations

### For Your Postflop Solver:

**Phase 1: Start Small**
```cpp
// Very limited abstraction for testing:
- 2-3 hand buckets per range (strong/medium/weak)
- 3 bet sizes (0.5×pot, 1×pot, all-in)
- Fixed flop, only solve turn + river
- Tree size: ~10 million nodes (solvable!)
```

**Phase 2: Add Bucketing**
```cpp
// 20-50 hand buckets using EHS
// 4-5 bet sizes
// 100-500 representative flops
// Tree size: ~100 million - 1 billion nodes
```

**Phase 3: Full Solver**
```cpp
// 100-200 hand buckets with potential-aware abstraction
// 5-6 bet sizes with geometric progression
// All flops with isomorphism
// Disk-based storage
// Tree size: 10-100 billion nodes (requires cluster/days)
```

---

## Key References

1. **Slumbot** (GitHub: ericgjackson/slumbot2019) - Open source with abstractions
2. **Postflop-solver** (GitHub: b-inary/postflop-solver) - Rust implementation
3. **"Solving Imperfect Information Games Using Decomposition"** - Brown et al.
4. **"DeepStack: Expert-Level AI in Heads-Up No-Limit Poker"** - Moravčík et al.
5. **"Superhuman AI for multiplayer poker"** (Pluribus) - Brown & Sandholm

---

## Example Code Structure

```cpp
// Add to postflop_poker.h:

class AbstractedPostflopGame {
    PostflopPoker base_game;
    HandBucketer bucketer;

    // Abstracted state uses buckets instead of exact cards
    using AbstractState = uint64_t;  // bucket_p1 | bucket_p2 | board | history

    int get_hand_bucket(const uint8_t* hole_cards,
                       const uint8_t* board,
                       int num_board) const {
        double ehs = bucketer.calculate_ehs(hole_cards, board, num_board);
        return bucketer.get_bucket(ehs);
    }

    AbstractState get_abstract_state() const {
        int bucket_p1 = get_hand_bucket(base_game.p1_hole, ...);
        int bucket_p2 = get_hand_bucket(base_game.p2_hole, ...);
        // Encode with buckets instead of exact cards
    }
};
```

This should give you a solid foundation for tackling the massive tree size!
