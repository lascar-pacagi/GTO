# Neural Network Poker Bots: DeepStack, Pluribus, and Beyond

A comprehensive guide to how modern poker AI uses deep learning instead of traditional CFR.

---

## 🧠 The Revolution: Why Neural Networks?

### Traditional CFR Problems

**Memory Explosion:**
```
Full game tree for No-Limit Hold'em: 10^160+ nodes
Even with abstraction: 10^12+ nodes requiring TB of RAM
Computation time: Days to weeks on clusters
```

**Neural Network Solution:**
- Train a network to **approximate** the game tree
- Store knowledge in **weights** (MB not TB)
- Compute solutions **on-the-fly** during play
- No need to solve entire game offline

---

## 🎯 DeepStack (2017)

**Paper:** "DeepStack: Expert-Level Artificial Intelligence in Heads-Up No-Limit Poker"
**Authors:** Moravčík et al. (University of Alberta)
**Achievement:** First AI to beat professional poker players in heads-up no-limit Hold'em

### Core Innovation: Continual Re-solving

DeepStack doesn't solve the entire game. Instead:

1. **At decision time:** Solve a limited-depth subgame
2. **At depth limit:** Use neural network to estimate values
3. **Repeat** for each decision point

```
Traditional CFR: Solve full game offline → Look up strategy
DeepStack: Solve local subgame online → Use NN for leaf values
```

### The Neural Network: Counterfactual Value Network

**Architecture:**

```python
Input Layer (Poker State):
├─ Hand strength features (7 values)
│  ├─ Hand strength vs uniform random
│  ├─ Hand potential (improving)
│  ├─ Hand potential (being outdrawn)
│  └─ Other hand statistics
├─ Board texture features (5 values)
│  ├─ Connectivity
│  ├─ Flush potential
│  ├─ Straight potential
│  └─ Pair/trips on board
└─ Betting features (3 values)
   ├─ Pot size
   ├─ Stack sizes
   └─ Betting history

Hidden Layers:
├─ Fully connected: 500 neurons (ReLU)
├─ Fully connected: 500 neurons (ReLU)
├─ Fully connected: 500 neurons (ReLU)
└─ Fully connected: 500 neurons (ReLU)

Output Layer:
└─ Counterfactual values for each player (2 values)
```

**What it predicts:** Given a game state at the frontier (e.g., river), what is the expected value if both players play optimally from here?

### Training Process

**Step 1: Generate Training Data**
```cpp
// Solve millions of random poker scenarios offline using CFR
for (int i = 0; i < 10_000_000; i++) {
    PokerState state = generate_random_state();

    // Solve this state to convergence with CFR
    CFRSolver solver;
    solver.solve(state, 1000_iterations);

    // Extract the values
    double value_p1 = solver.get_value(PLAYER1);
    double value_p2 = solver.get_value(PLAYER2);

    // Create training example
    training_data.add({
        features: extract_features(state),
        label: {value_p1, value_p2}
    });
}
```

**Step 2: Train Neural Network**
```python
# Standard supervised learning
model = Sequential([
    Dense(500, activation='relu'),
    Dense(500, activation='relu'),
    Dense(500, activation='relu'),
    Dense(500, activation='relu'),
    Dense(2)  # Output: [value_p1, value_p2]
])

model.compile(optimizer='adam', loss='mse')
model.fit(features, labels, epochs=100)
```

**Key Insight:** The NN learns to generalize from solved positions to unseen ones!

### How DeepStack Plays (Runtime)

```python
def deepstack_decision(poker_state):
    """Make a decision using continual re-solving"""

    # 1. Build a subgame tree (limited depth)
    subgame = build_subgame(
        root=poker_state,
        depth=3,  # e.g., solve 3 streets ahead
        lookahead_depth=4  # Use NN after 4 actions
    )

    # 2. Replace leaf nodes with NN value estimates
    for leaf in subgame.get_leaves():
        if not leaf.is_terminal():
            # Use neural network instead of solving further
            values = neural_network.predict(leaf.state)
            leaf.set_values(values)

    # 3. Solve this subgame with CFR
    solver = CFRSolver(subgame)
    solver.solve(iterations=1000)

    # 4. Return strategy for current state
    strategy = solver.get_strategy(poker_state)
    action = sample_action(strategy)

    return action
```

**Depth Limiting:**
- Solve 2-4 betting rounds lookahead
- Use NN to estimate value beyond that
- Much faster than solving to end of game!

### DeepStack vs Traditional CFR

| Aspect | Traditional CFR | DeepStack |
|--------|----------------|-----------|
| **Computation** | Days offline | Seconds online |
| **Memory** | TB (full game tree) | MB (NN weights) |
| **Adaptability** | Fixed blueprint | Adapts per-hand |
| **Exploitability** | Very low (~0.1%) | Low (~0.5-1%) |
| **Generalization** | Exact solutions | Approximate |

---

## 🤖 Pluribus (2019)

**Paper:** "Superhuman AI for multiplayer poker"
**Authors:** Brown & Sandholm (Carnegie Mellon + Facebook AI)
**Achievement:** First AI to beat pros in 6-player poker

### Key Innovation: Self-Play + Search

Pluribus combines:
1. **Blueprint strategy** (offline training via self-play)
2. **Real-time search** (online adaptation)

### Architecture Overview

```
┌─────────────────────────────────────────┐
│         BLUEPRINT STRATEGY               │
│   (trained via self-play CFR)           │
│                                          │
│  ┌──────────────────────────────────┐  │
│  │  Monte Carlo CFR Self-Play       │  │
│  │  - 12,400 CPU core-hours          │  │
│  │  - 8 days on 64-core server      │  │
│  │  - Abstraction: 6 bet sizes      │  │
│  └──────────────────────────────────┘  │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│        REAL-TIME SEARCH                  │
│   (depth-limited solving per hand)      │
│                                          │
│  ┌──────────────────────────────────┐  │
│  │  Search depth: 4 moves ahead     │  │
│  │  Beyond: use blueprint strategy   │  │
│  │  Modified CFR for search         │  │
│  └──────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

### Pluribus Innovations

**1. Linear CFR (Faster than CFR+)**
```cpp
// Update formula:
strategy_sum[a] += reach_prob * strategy[a] * iteration

// Faster convergence for large games
```

**2. Depth-Limited Search**
- Solve 4 actions ahead
- Use blueprint strategy beyond that
- No neural network needed!

**3. Self-Play Training**
- Play 6 copies of itself
- Iteratively improve via CFR
- No human data needed

**4. Dealing with 6 Players**
- Model opponents as having same strategy
- Reduces complexity from O(5^n) to O(5)
- "Assume all opponents play the same way"

### Pluribus Blueprint Training

```python
class PluribusTrainer:
    def __init__(self):
        self.blueprint_strategy = {}

    def train(self, num_iterations=10_000):
        """Self-play CFR training"""

        for iteration in range(num_iterations):
            # Play game with 6 copies of current agent
            game = SixPlayerPoker()

            for player in range(6):
                # Each player uses current blueprint
                # Updated via CFR
                regrets = self.cfr_traverse(
                    game, player, iteration
                )

                self.update_blueprint(regrets)

            if iteration % 1000 == 0:
                # Evaluate against past versions
                exploitability = self.compute_exploitability()
                print(f"Iter {iteration}: {exploitability}")

    def cfr_traverse(self, game, player, iteration):
        """Modified Monte Carlo CFR"""
        if game.is_terminal():
            return game.payoff(player)

        if game.is_chance():
            # Sample 1 outcome instead of all
            action = game.sample_chance()
            game.play(action)
            value = self.cfr_traverse(game, player, iteration)
            game.undo(action)
            return value

        # Standard CFR for player nodes
        # ... (similar to regular CFR)
```

### Runtime Search

```python
def pluribus_decision(game_state):
    """Make decision using real-time search"""

    # 1. Build search tree (4 moves ahead)
    search_tree = SearchTree(
        root=game_state,
        depth=4,
        use_blueprint_beyond_depth=True
    )

    # 2. Solve with modified CFR (fast variant)
    solver = LinearCFRSolver(search_tree)
    solver.solve(iterations=100)  # Fast!

    # 3. Extract strategy
    strategy = solver.get_strategy(game_state)

    return sample_action(strategy)
```

---

## 🔥 ReBeL (2020)

**Paper:** "Combining Deep Reinforcement Learning and Search"
**Authors:** Brown et al. (Facebook AI Research)
**Achievement:** Unified framework for imperfect information games

### Key Innovation: Value + Policy Networks

ReBeL learns TWO neural networks:

**1. Value Network**
```python
value_net(state) → expected_payoff
```

**2. Policy Network**
```python
policy_net(state) → probability_distribution_over_actions
```

### ReBeL Algorithm

```python
class ReBeL:
    def __init__(self):
        self.value_net = ValueNetwork()
        self.policy_net = PolicyNetwork()

    def train_iteration(self):
        """Self-play + learning"""

        # 1. Generate self-play data
        trajectories = []
        for game_num in range(1000):
            game = Poker()
            trajectory = self.play_game_with_search(game)
            trajectories.append(trajectory)

        # 2. Train value network
        for state, actual_value in trajectories:
            predicted = self.value_net(state)
            loss = mse(predicted, actual_value)
            self.value_net.update(loss)

        # 3. Train policy network
        for state, search_policy in trajectories:
            predicted = self.policy_net(state)
            loss = kl_divergence(predicted, search_policy)
            self.policy_net.update(loss)

    def play_game_with_search(self, game):
        """Play with depth-limited search using networks"""
        trajectory = []

        while not game.is_terminal():
            # Search depth-limited tree
            search_policy = self.run_search(game.state)
            action = sample(search_policy)

            # Record for training
            trajectory.append((
                state=game.state,
                search_policy=search_policy,
                value=None  # Filled in later
            ))

            game.play(action)

        # Backfill actual values
        final_value = game.payoff()
        for entry in trajectory:
            entry.value = final_value

        return trajectory

    def run_search(self, state):
        """Depth-limited search using value net"""
        # Similar to AlphaZero MCTS
        # Use value_net for leaf evaluation
        # Use policy_net for prior probabilities
        # ... (MCTS implementation)
```

### ReBeL vs Others

| Feature | DeepStack | Pluribus | ReBeL |
|---------|-----------|----------|-------|
| **Offline Training** | CFR solutions | Self-play CFR | RL self-play |
| **Online Search** | CFR + NN values | CFR + blueprint | MCTS + networks |
| **Network Type** | Value only | None (blueprint) | Value + Policy |
| **Data Source** | CFR labels | Self-play | Self-play |
| **Generality** | Poker-specific | Poker-specific | General games |

---

## 🎮 How to Implement Neural Network Poker Bot

### Approach 1: DeepStack Style

```cpp
// Step 1: Generate training data
class TrainingDataGenerator {
    void generate(int num_samples) {
        for (int i = 0; i < num_samples; i++) {
            // Random poker state
            PostflopPoker state = random_postflop_state();

            // Solve with CFR
            CFRSolver solver;
            solver.solve(state, 1000);

            // Extract features and values
            auto features = extract_features(state);
            auto values = solver.get_values();

            save_training_example(features, values);
        }
    }

    std::vector<double> extract_features(const PostflopPoker& state) {
        std::vector<double> features;

        // Hand strength features
        double ehs = calculate_ehs(state);
        features.push_back(ehs);

        // Potential features
        double hp_pos = hand_potential_positive(state);
        double hp_neg = hand_potential_negative(state);
        features.push_back(hp_pos);
        features.push_back(hp_neg);

        // Board texture
        features.push_back(has_flush_draw(state) ? 1.0 : 0.0);
        features.push_back(has_straight_draw(state) ? 1.0 : 0.0);
        features.push_back(is_paired_board(state) ? 1.0 : 0.0);

        // Game state
        features.push_back(state.pot / 100.0);
        features.push_back(state.stack_p1 / 100.0);
        features.push_back(state.stack_p2 / 100.0);

        return features;
    }
};
```

```python
# Step 2: Train neural network
import tensorflow as tf

def build_value_network():
    model = tf.keras.Sequential([
        tf.keras.layers.Dense(512, activation='relu', input_dim=15),
        tf.keras.layers.Dense(512, activation='relu'),
        tf.keras.layers.Dense(256, activation='relu'),
        tf.keras.layers.Dense(128, activation='relu'),
        tf.keras.layers.Dense(2)  # Values for P1, P2
    ])

    model.compile(
        optimizer='adam',
        loss='mse',
        metrics=['mae']
    )

    return model

# Train
model = build_value_network()
model.fit(training_features, training_values,
          epochs=50, batch_size=256,
          validation_split=0.2)

model.save('poker_value_network.h5')
```

```cpp
// Step 3: Use in play
class DeepStackPlayer {
    NeuralNetwork value_net;

    Action decide(const PostflopPoker& state) {
        // Build subgame
        SubGame subgame(state, depth=3);

        // Replace leaves with NN values
        for (auto& leaf : subgame.get_leaves()) {
            auto features = extract_features(leaf);
            auto values = value_net.predict(features);
            leaf.set_terminal_values(values);
        }

        // Solve subgame
        CFRSolver solver(subgame);
        solver.solve(1000);

        // Get strategy
        return solver.sample_action(state);
    }
};
```

### Approach 2: Pluribus Style (No NN!)

```cpp
class PluribusPlayer {
    BlueprintStrategy blueprint;  // Trained offline

    Action decide(const PostflopPoker& state) {
        // Depth-limited search
        SearchTree tree(state, depth=4);

        // Use blueprint beyond depth
        tree.set_default_strategy(blueprint);

        // Solve with Linear CFR
        LinearCFRSolver solver(tree);
        solver.solve(100);  // Fast!

        return solver.sample_action(state);
    }
};
```

---

## 📊 Performance Comparison

### DeepStack Results (2017)

- **Opponents:** 33 professional poker players
- **Hands:** 44,852 hands per player
- **Result:** Win rate of 492 ± 232 mbb/hand
  - mbb = milli big blinds per hand
  - 492 mbb/hand = crushing victory
- **Confidence:** 99.998% certainty of being better

### Pluribus Results (2019)

- **Opponents:** 15 top professionals (6-max)
- **Hands:** 10,000 hands
- **Result:** 5 bb/100 win rate per player
  - In 6-player, beating field by 5bb/100 is exceptional
- **Equivalent:** $1,000/hour at typical stakes

---

## 🔮 Future Directions

### 1. Multimodal Networks
```python
# Combine visual + symbolic reasoning
class MultimodalPokerNet:
    vision_encoder = ConvNet()  # Process table image
    symbolic_encoder = MLP()     # Process game state
    fusion_layer = Attention()   # Combine both
    value_head = Dense(1)        # Output value
```

### 2. Opponent Modeling
```python
# Learn opponent-specific strategies
class OpponentAdaptiveBot:
    def __init__(self):
        self.opponent_models = {}  # One per opponent

    def observe(self, opponent_id, action, state):
        # Update belief about opponent's strategy
        self.opponent_models[opponent_id].update(state, action)

    def exploit(self, opponent_id, state):
        # Compute exploitative strategy
        opp_strategy = self.opponent_models[opponent_id].predict(state)
        return compute_best_response(opp_strategy)
```

### 3. Transfer Learning
```python
# Pre-train on simpler games, transfer to poker
base_model = train_on_leduc_holdem()
poker_model = fine_tune_on_texas_holdem(base_model)
```

### 4. Meta-Learning
```python
# Learn to quickly adapt to new opponents
class MetaPokerBot:
    def quick_adapt(self, opponent_samples, num_steps=10):
        # MAML-style meta-learning
        # Adapt in 10 hands instead of 1000!
        pass
```

---

## 💡 Key Takeaways

### Why Neural Networks Work for Poker

1. **Generalization** - Learn patterns across millions of positions
2. **Compression** - Store knowledge in weights, not game trees
3. **Speed** - Fast forward pass vs. solving CFR from scratch
4. **Scalability** - Can handle larger games than pure CFR

### When to Use What

| Approach | Best For | Drawbacks |
|----------|----------|-----------|
| **Pure CFR** | Small games, exact solutions | Memory, computation |
| **DeepStack** | 1v1, deep stacks, NL | Training data generation |
| **Pluribus** | Multiplayer, tournament | 6-player assumption |
| **ReBeL** | General research, new games | Complex training |

### Practical Implementation

For your GTO solver:

```cpp
// Phase 1: Pure CFR (what you have)
DCFRPostflopSolver solver;
solver.solve(game, 10000);

// Phase 2: Add NN approximation (future)
TrainingDataGenerator gen;
gen.generate_from_cfr_solutions(100000);

NeuralNetwork value_net = train_network(gen.data);

DeepStackSolver ds_solver(value_net);
ds_solver.solve_with_continual_resolving(game);
```

---

## 📚 Further Reading

1. **DeepStack Paper** (2017) - Moravčík et al.
2. **Pluribus Paper** (2019) - Brown & Sandholm, Science
3. **ReBeL Paper** (2020) - Brown et al., ICML
4. **AlphaZero** (2017) - Silver et al. (transfer ideas to poker)
5. **Libratus** (2017) - Brown & Sandholm (predecessor to Pluribus)

---

**The future of poker AI is neural!** 🧠🃏🤖
