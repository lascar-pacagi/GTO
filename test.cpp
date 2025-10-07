#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include <random>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <immintrin.h>
#include <omp.h>
#include <atomic>
#include <memory>

// Leduc Poker: 6 cards (2 suits, 3 ranks), 2 players, 2 betting rounds
// Deck: J♠ J♥ Q♠ Q♥ K♠ K♥

constexpr int NUM_CARDS = 6;
constexpr int NUM_RANKS = 3;
constexpr int NUM_ACTIONS = 3;
constexpr int FOLD = 0;
constexpr int CALL = 1;
constexpr int RAISE = 2;

// Lock-free info node using atomics with double-buffering for reads
struct alignas(64) InfoNode {
    // Using atomic floats for lock-free updates
    alignas(16) std::atomic<float> regretSum[NUM_ACTIONS];
    alignas(16) std::atomic<float> strategySum[NUM_ACTIONS];
    
    InfoNode() {
        for (int a = 0; a < NUM_ACTIONS; ++a) {
            regretSum[a].store(0.0f, std::memory_order_relaxed);
            strategySum[a].store(0.0f, std::memory_order_relaxed);
        }
    }
    
    // Prevent copy/move to avoid atomic copy issues
    InfoNode(const InfoNode&) = delete;
    InfoNode& operator=(const InfoNode&) = delete;
    
    // Lock-free strategy computation with snapshot
    inline void getStrategy(float* outStrategy) {
        float regrets[NUM_ACTIONS];
        float normSum = 0.0f;
        
        // Snapshot regrets
        for (int a = 0; a < NUM_ACTIONS; ++a) {
            regrets[a] = regretSum[a].load(std::memory_order_relaxed);
            float positive = regrets[a] > 0.0f ? regrets[a] : 0.0f;
            outStrategy[a] = positive;
            normSum += positive;
        }
        
        if (normSum > 0.0f) {
            float invNorm = 1.0f / normSum;
            for (int a = 0; a < NUM_ACTIONS; ++a) {
                outStrategy[a] *= invNorm;
            }
        } else {
            float uniform = 1.0f / NUM_ACTIONS;
            for (int a = 0; a < NUM_ACTIONS; ++a) {
                outStrategy[a] = uniform;
            }
        }
    }
    
    // Lock-free atomic accumulation
    inline void accumulateRegret(int action, float value) {
        float expected = regretSum[action].load(std::memory_order_relaxed);
        while (!regretSum[action].compare_exchange_weak(
            expected, expected + value,
            std::memory_order_relaxed, std::memory_order_relaxed)) {
            // CAS retry
        }
    }
    
    inline void accumulateStrategy(int action, float value) {
        float expected = strategySum[action].load(std::memory_order_relaxed);
        while (!strategySum[action].compare_exchange_weak(
            expected, expected + value,
            std::memory_order_relaxed, std::memory_order_relaxed)) {
            // CAS retry
        }
    }
    
    // Batch update for better performance
    inline void accumulateRegrets(const std::array<float, NUM_ACTIONS>& regrets) {
        for (int a = 0; a < NUM_ACTIONS; ++a) {
            if (regrets[a] != 0.0f) {
                accumulateRegret(a, regrets[a]);
            }
        }
    }
    
    inline void accumulateStrategies(const std::array<float, NUM_ACTIONS>& strats) {
        for (int a = 0; a < NUM_ACTIONS; ++a) {
            if (strats[a] != 0.0f) {
                accumulateStrategy(a, strats[a]);
            }
        }
    }
    
    inline void getAverageStrategy(float* outStrategy) const {
        float normSum = 0.0f;
        for (int a = 0; a < NUM_ACTIONS; ++a) {
            outStrategy[a] = strategySum[a].load(std::memory_order_relaxed);
            normSum += outStrategy[a];
        }
        
        if (normSum > 0.0f) {
            float invNorm = 1.0f / normSum;
            for (int a = 0; a < NUM_ACTIONS; ++a) {
                outStrategy[a] *= invNorm;
            }
        } else {
            float uniform = 1.0f / NUM_ACTIONS;
            for (int a = 0; a < NUM_ACTIONS; ++a) {
                outStrategy[a] = uniform;
            }
        }
    }
};

struct GameState {
    std::array<int8_t, 2> cards;
    int8_t boardCard;
    std::array<int16_t, 2> pot;
    std::string history;
    int8_t round;
    
    GameState() : cards{-1, -1}, boardCard(-1), pot{1, 1}, history(""), round(0) {}
};

// Thread-local storage with pre-allocated capacity
struct ThreadLocalData {
    std::unordered_map<std::string, std::array<float, NUM_ACTIONS>> localRegrets;
    std::unordered_map<std::string, std::array<float, NUM_ACTIONS>> localStrategies;
    
    ThreadLocalData() {
        localRegrets.reserve(1024);
        localStrategies.reserve(1024);
    }
    
    void clear() {
        localRegrets.clear();
        localStrategies.clear();
    }
};

// Sharded node map to reduce contention on map operations
template<size_t NumShards = 64>
class ShardedNodeMap {
private:
    struct Shard {
        std::unordered_map<std::string, std::unique_ptr<InfoNode>> map;
        alignas(64) std::atomic_flag lock = ATOMIC_FLAG_INIT;
        
        Shard() {
            map.reserve(512);
        }
    };
    
    std::array<Shard, NumShards> shards;
    
    inline size_t getShardIndex(const std::string& key) const {
        return std::hash<std::string>{}(key) % NumShards;
    }
    
public:
    InfoNode* getOrCreate(const std::string& key) {
        size_t idx = getShardIndex(key);
        Shard& shard = shards[idx];
        
        // Fast path: try without lock
        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            return it->second.get();
        }
        
        // Slow path: acquire lock and insert
        while (shard.lock.test_and_set(std::memory_order_acquire)) {
            // Spin
        }
        
        auto result = shard.map.try_emplace(key, std::make_unique<InfoNode>());
        InfoNode* node = result.first->second.get();
        
        shard.lock.clear(std::memory_order_release);
        return node;
    }
    
    InfoNode* find(const std::string& key) {
        size_t idx = getShardIndex(key);
        auto it = shards[idx].map.find(key);
        return (it != shards[idx].map.end()) ? it->second.get() : nullptr;
    }
    
    size_t size() const {
        size_t total = 0;
        for (const auto& shard : shards) {
            total += shard.map.size();
        }
        return total;
    }
};

class LeducCFR {
private:
    ShardedNodeMap<64> nodeMap;
    std::atomic<int> iterationCounter{0};
    
    inline int getHandStrength(int card, int board) const {
        if (board == -1) return card / 2;
        if (card / 2 == board / 2) return 100 + card / 2;
        return card / 2;
    }
    
    inline int compareHands(int c0, int c1, int board) const {
        int s0 = getHandStrength(c0, board);
        int s1 = getHandStrength(c1, board);
        return (s0 > s1) - (s0 < s1);
    }
    
    inline void getValidActions(const std::string& history, bool* valid) const {
        valid[FOLD] = valid[CALL] = valid[RAISE] = false;
        
        if (history.empty() || history.back() == '/') {
            valid[CALL] = true;
            valid[RAISE] = true;
        } else if (history.back() == 'r') {
            valid[FOLD] = true;
            valid[CALL] = true;
            valid[RAISE] = true;
        } else if (history.back() == 'c') {
            valid[CALL] = true;
            valid[RAISE] = true;
        }
    }
    
    inline std::string getInfoSet(int card, const std::string& history) const {
        return std::to_string(card) + ":" + history;
    }
    
    // External sampling CFR - only traverse one action per node
    float externalCFR(GameState& state, int player, 
                      std::vector<bool>& deck, ThreadLocalData& tld,
                      std::mt19937& rng) {
        
        const std::string& history = state.history;
        
        // Terminal nodes
        if (history.length() >= 2) {
            char last = history.back();
            char prev = history[history.length() - 2];
            
            if (last == 'f') {
                int fold_player = (history.length() - 1) % 2;
                return fold_player == player ? -state.pot[player] : state.pot[1 - player];
            }
            
            if (last == 'c' && (prev == 'c' || prev == 'r')) {
                if (state.round == 0) {
                    state.round = 1;
                    state.history += '/';
                    
                    // Sample random board card
                    std::vector<int> available;
                    for (int i = 0; i < NUM_CARDS; ++i) {
                        if (deck[i]) available.push_back(i);
                    }
                    
                    std::uniform_int_distribution<int> dist(0, available.size() - 1);
                    int boardIdx = available[dist(rng)];
                    
                    deck[boardIdx] = false;
                    state.boardCard = boardIdx;
                    float value = externalCFR(state, player, deck, tld, rng);
                    deck[boardIdx] = true;
                    state.boardCard = -1;
                    state.history.pop_back();
                    state.round = 0;
                    return value;
                } else {
                    int result = compareHands(state.cards[0], state.cards[1], state.boardCard);
                    return result == 0 ? 0.0f : 
                           (result == 1) == (player == 0) ? (float)state.pot[1 - player] : -(float)state.pot[player];
                }
            }
        }
        
        int curPlayer = history.length() % 2;
        if (!history.empty() && history.back() == '/') {
            curPlayer = 0;
        }
        
        std::string infoSet = getInfoSet(state.cards[curPlayer], history);
        InfoNode* node = nodeMap.getOrCreate(infoSet);
        
        bool validActions[NUM_ACTIONS];
        getValidActions(history, validActions);
        int numValid = validActions[FOLD] + validActions[CALL] + validActions[RAISE];
        
        float strategy[NUM_ACTIONS];
        node->getStrategy(strategy);
        
        // Normalize over valid actions
        float validSum = 0.0f;
        for (int a = 0; a < NUM_ACTIONS; ++a) {
            if (!validActions[a]) strategy[a] = 0.0f;
            validSum += strategy[a];
        }
        if (validSum > 0.0f) {
            float invSum = 1.0f / validSum;
            for (int a = 0; a < NUM_ACTIONS; ++a) {
                strategy[a] *= invSum;
            }
        } else {
            float uniform = 1.0f / numValid;
            for (int a = 0; a < NUM_ACTIONS; ++a) {
                strategy[a] = validActions[a] ? uniform : 0.0f;
            }
        }
        
        // Accumulate strategy
        std::array<float, NUM_ACTIONS> stratAccum;
        for (int a = 0; a < NUM_ACTIONS; ++a) {
            stratAccum[a] = strategy[a];
        }
        
        auto stratIt = tld.localStrategies.find(infoSet);
        if (stratIt != tld.localStrategies.end()) {
            for (int a = 0; a < NUM_ACTIONS; ++a) {
                stratIt->second[a] += stratAccum[a];
            }
        } else {
            tld.localStrategies[infoSet] = stratAccum;
        }
        
        if (curPlayer == player) {
            // Exploring player: compute all action values
            std::array<float, NUM_ACTIONS> actionUtils = {0.0f};
            float nodeUtil = 0.0f;
            
            for (int a = 0; a < NUM_ACTIONS; ++a) {
                if (!validActions[a]) continue;
                
                char action = (a == FOLD) ? 'f' : (a == CALL) ? 'c' : 'r';
                state.history += action;
                
                int oldPot = state.pot[curPlayer];
                if (a == RAISE) {
                    state.pot[curPlayer] += (state.round == 0) ? 2 : 4;
                } else if (a == CALL && !history.empty() && history.back() == 'r') {
                    state.pot[curPlayer] = state.pot[1 - curPlayer];
                }
                
                actionUtils[a] = -externalCFR(state, 1 - player, deck, tld, rng);
                state.pot[curPlayer] = oldPot;
                state.history.pop_back();
                
                nodeUtil += strategy[a] * actionUtils[a];
            }
            
            // Accumulate regrets locally
            std::array<float, NUM_ACTIONS> regretAccum = {0.0f};
            for (int a = 0; a < NUM_ACTIONS; ++a) {
                if (validActions[a]) {
                    regretAccum[a] = actionUtils[a] - nodeUtil;
                }
            }
            
            auto regIt = tld.localRegrets.find(infoSet);
            if (regIt != tld.localRegrets.end()) {
                for (int a = 0; a < NUM_ACTIONS; ++a) {
                    regIt->second[a] += regretAccum[a];
                }
            } else {
                tld.localRegrets[infoSet] = regretAccum;
            }
            
            return nodeUtil;
        } else {
            // Sample action according to strategy
            std::discrete_distribution<int> dist(strategy, strategy + NUM_ACTIONS);
            int action = dist(rng);
            
            char actionChar = (action == FOLD) ? 'f' : (action == CALL) ? 'c' : 'r';
            state.history += actionChar;
            
            int oldPot = state.pot[curPlayer];
            if (action == RAISE) {
                state.pot[curPlayer] += (state.round == 0) ? 2 : 4;
            } else if (action == CALL && !history.empty() && history.back() == 'r') {
                state.pot[curPlayer] = state.pot[1 - curPlayer];
            }
            
            float value = -externalCFR(state, 1 - player, deck, tld, rng);
            
            state.pot[curPlayer] = oldPot;
            state.history.pop_back();
            
            return value;
        }
    }
    
public:
    LeducCFR() {}
    
    void train(int iterations) {
        std::cout << "Training External Sampling CFR for " << iterations << " iterations...\n";
        std::cout << "Using " << omp_get_max_threads() << " threads\n";
        
        #pragma omp parallel
        {
            int threadId = omp_get_thread_num();
            std::mt19937 localRng(std::random_device{}() + threadId * 12345);
            ThreadLocalData tld;
            
            // Each thread processes independent iterations
            #pragma omp for schedule(static, 100)
            for (int iter = 0; iter < iterations; ++iter) {
                tld.clear();
                
                std::vector<bool> deck(NUM_CARDS, true);
                GameState state;
                
                // Deal cards
                std::vector<int> cards;
                for (int i = 0; i < NUM_CARDS; ++i) cards.push_back(i);
                std::shuffle(cards.begin(), cards.end(), localRng);
                
                state.cards[0] = cards[0];
                state.cards[1] = cards[1];
                deck[cards[0]] = false;
                deck[cards[1]] = false;
                
                // Alternate which player we update
                int updatePlayer = iter % 2;
                externalCFR(state, updatePlayer, deck, tld, localRng);
                
                // Batch update global nodes (lock-free atomics)
                for (auto& [infoSet, regrets] : tld.localRegrets) {
                    InfoNode* node = nodeMap.getOrCreate(infoSet);
                    node->accumulateRegrets(regrets);
                }
                
                for (auto& [infoSet, strats] : tld.localStrategies) {
                    InfoNode* node = nodeMap.getOrCreate(infoSet);
                    node->accumulateStrategies(strats);
                }
                
                // Progress reporting
                if (threadId == 0) {
                    int completed = iterationCounter.fetch_add(1, std::memory_order_relaxed);
                    if ((completed + 1) % 10000 == 0) {
                        std::cout << "Completed " << (completed + 1) << " iterations\n";
                    }
                }
            }
        }
        
        std::cout << "Training complete! Info sets: " << nodeMap.size() << "\n";
    }
    
    void printStrategy(const std::string& infoSet) {
        InfoNode* node = nodeMap.find(infoSet);
        if (!node) {
            std::cout << "Info set not found: " << infoSet << "\n";
            return;
        }
        
        float strategy[NUM_ACTIONS];
        node->getAverageStrategy(strategy);
        
        std::cout << "Strategy for " << infoSet << ":\n";
        std::cout << "  Fold:  " << strategy[FOLD] << "\n";
        std::cout << "  Call:  " << strategy[CALL] << "\n";
        std::cout << "  Raise: " << strategy[RAISE] << "\n";
    }
    
    size_t getNodeCount() const { return nodeMap.size(); }
};

int main() {
    LeducCFR cfr;
    
    cfr.train(1000000);
    
    std::cout << "\n=== Example Strategies ===\n";
    cfr.printStrategy("0:");
    cfr.printStrategy("2:r");
    cfr.printStrategy("4:cr");
    
    return 0;
}