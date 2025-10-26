#ifndef HAND_BUCKETING_H
#define HAND_BUCKETING_H

#include "postflop_poker.h"
#include <vector>
#include <set>
#include <random>
#include <cmath>

/*
 * Hand Bucketing for Poker Abstraction
 *
 * Reduces the number of distinct hands by grouping similar hands into buckets.
 * This is essential for making postflop solving computationally tractable.
 *
 * Example: Instead of 1326 possible hand combinations, we use 50-200 buckets.
 */

class HandBucketer {
public:
    static constexpr int DEFAULT_NUM_BUCKETS = 50;
    static constexpr int DEFAULT_MONTE_CARLO_TRIALS = 1000;

    HandBucketer(int num_buckets = DEFAULT_NUM_BUCKETS,
                 int mc_trials = DEFAULT_MONTE_CARLO_TRIALS)
        : num_buckets(num_buckets), mc_trials(mc_trials) {}

    // Calculate Expected Hand Strength (EHS)
    // EHS = P(win) + 0.5 * P(tie)
    double calculate_ehs(const uint8_t* hole_cards,
                        const uint8_t* board,
                        int num_board_cards,
                        PostflopHandEvaluator& evaluator) const {

        if (num_board_cards < 3) return 0.5;  // No board yet

        // Collect used cards
        std::set<uint8_t> used;
        used.insert(hole_cards[0]);
        used.insert(hole_cards[1]);
        for (int i = 0; i < num_board_cards; i++) {
            used.insert(board[i]);
        }

        // Build opponent hand combinations
        std::vector<std::pair<uint8_t, uint8_t>> opp_hands;
        for (int c1 = 0; c1 < 52; c1++) {
            if (used.count(c1)) continue;
            for (int c2 = c1 + 1; c2 < 52; c2++) {
                if (used.count(c2)) continue;
                opp_hands.emplace_back(c1, c2);
            }
        }

        if (opp_hands.empty()) return 0.5;

        // Monte Carlo sampling if too many combinations
        int trials = std::min(mc_trials, static_cast<int>(opp_hands.size()));
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(0, opp_hands.size() - 1);

        int wins = 0, ties = 0;

        for (int trial = 0; trial < trials; trial++) {
            // Sample opponent hand
            auto [opp_c1, opp_c2] = opp_hands[dist(rng)];

            // If we don't have river yet, need to rollout
            if (num_board_cards < 5) {
                double trial_score = rollout_hand(hole_cards, {opp_c1, opp_c2},
                                                   board, num_board_cards, evaluator);
                wins += trial_score;
                ties += (trial_score == 0.5 ? 1 : 0);
            } else {
                // Full board - direct evaluation
                int result = evaluate_hands(hole_cards, {opp_c1, opp_c2},
                                            board, 5, evaluator);
                if (result > 0) wins++;
                else if (result == 0) ties++;
            }
        }

        return (static_cast<double>(wins) + 0.5 * ties) / trials;
    }

    // Get bucket index for a hand (0 to num_buckets-1)
    int get_bucket(double ehs) const {
        int bucket = static_cast<int>(ehs * num_buckets);
        return std::min(bucket, num_buckets - 1);
    }

    // Get bucket for a specific hand
    int get_hand_bucket(const uint8_t* hole_cards,
                       const uint8_t* board,
                       int num_board_cards,
                       PostflopHandEvaluator& evaluator) const {
        double ehs = calculate_ehs(hole_cards, board, num_board_cards, evaluator);
        return get_bucket(ehs);
    }

    // Get bucket ranges (for analysis)
    std::vector<std::pair<double, double>> get_bucket_ranges() const {
        std::vector<std::pair<double, double>> ranges;
        for (int i = 0; i < num_buckets; i++) {
            double low = static_cast<double>(i) / num_buckets;
            double high = static_cast<double>(i + 1) / num_buckets;
            ranges.emplace_back(low, high);
        }
        return ranges;
    }

private:
    int num_buckets;
    int mc_trials;

    // Rollout hand to river and average results
    double rollout_hand(const uint8_t* my_hole,
                       std::pair<uint8_t, uint8_t> opp_hole,
                       const uint8_t* board,
                       int num_board_cards,
                       PostflopHandEvaluator& evaluator) const {

        // Collect used cards
        std::set<uint8_t> used;
        used.insert(my_hole[0]);
        used.insert(my_hole[1]);
        used.insert(opp_hole.first);
        used.insert(opp_hole.second);
        for (int i = 0; i < num_board_cards; i++) {
            used.insert(board[i]);
        }

        // Get remaining cards
        std::vector<uint8_t> remaining;
        for (int c = 0; c < 52; c++) {
            if (!used.count(c)) remaining.push_back(c);
        }

        // Sample random river completions
        int rollouts = std::min(100, static_cast<int>(remaining.size()));
        std::mt19937 rng(my_hole[0] * 53 + my_hole[1]);

        double total_score = 0.0;

        for (int r = 0; r < rollouts; r++) {
            uint8_t full_board[5];
            for (int i = 0; i < num_board_cards; i++) {
                full_board[i] = board[i];
            }

            // Sample remaining board cards
            std::shuffle(remaining.begin(), remaining.end(), rng);
            for (int i = num_board_cards; i < 5; i++) {
                full_board[i] = remaining[i - num_board_cards];
            }

            int result = evaluate_hands(my_hole, opp_hole, full_board, 5, evaluator);
            if (result > 0) total_score += 1.0;
            else if (result == 0) total_score += 0.5;
        }

        return total_score / rollouts;
    }

    // Evaluate two hands against each other
    // Returns: 1 if my_hole wins, -1 if opp_hole wins, 0 if tie
    int evaluate_hands(const uint8_t* my_hole,
                      std::pair<uint8_t, uint8_t> opp_hole,
                      const uint8_t* board,
                      int num_board_cards,
                      PostflopHandEvaluator& evaluator) const {

        uint8_t my_cards[7], opp_cards[7];

        // Build 7-card hands (or 5/6 if not full board)
        for (int i = 0; i < num_board_cards; i++) {
            my_cards[i] = opp_cards[i] = board[i];
        }
        my_cards[num_board_cards] = my_hole[0];
        my_cards[num_board_cards + 1] = my_hole[1];
        opp_cards[num_board_cards] = opp_hole.first;
        opp_cards[num_board_cards + 1] = opp_hole.second;

        int total = num_board_cards + 2;

        uint16_t my_rank, opp_rank;

        if (total == 7) {
            my_rank = evaluator.evaluate7(my_cards);
            opp_rank = evaluator.evaluate7(opp_cards);
        } else if (total >= 5) {
            my_rank = evaluator.evaluate5(my_cards);
            opp_rank = evaluator.evaluate5(opp_cards);
        } else {
            return 0;  // Not enough cards
        }

        // Lower rank is better in the evaluator
        if (my_rank < opp_rank) return 1;
        if (opp_rank < my_rank) return -1;
        return 0;
    }
};

/*
 * Advanced Bucketing: K-Means Clustering
 *
 * For more sophisticated abstraction, group hands based on multiple features:
 * - Expected Hand Strength (EHS)
 * - Hand Potential (HP)
 * - Hand Strength Distribution
 */

struct HandFeatures {
    double ehs;                          // Expected hand strength
    double hand_potential_positive;      // P(improving to best hand)
    double hand_potential_negative;      // P(being outdrawn)
    std::array<double, 10> strength_dist; // Distribution of hand strengths

    // Distance metric for clustering
    double distance(const HandFeatures& other) const {
        double d = 0.0;
        d += std::pow(ehs - other.ehs, 2);
        d += std::pow(hand_potential_positive - other.hand_potential_positive, 2);
        d += std::pow(hand_potential_negative - other.hand_potential_negative, 2);

        for (int i = 0; i < 10; i++) {
            d += 0.1 * std::pow(strength_dist[i] - other.strength_dist[i], 2);
        }

        return std::sqrt(d);
    }
};

class KMeansHandBucketer {
public:
    KMeansHandBucketer(int num_buckets) : num_buckets(num_buckets) {}

    // Train k-means clusters on sample hands
    void train(const std::vector<HandFeatures>& samples) {
        // K-means++ initialization
        centroids.clear();
        centroids.resize(num_buckets);

        // TODO: Implement k-means clustering
        // 1. Initialize centroids using k-means++
        // 2. Assign samples to nearest centroid
        // 3. Update centroids to mean of assigned samples
        // 4. Repeat until convergence
    }

    int get_bucket(const HandFeatures& features) const {
        double min_dist = 1e9;
        int best_bucket = 0;

        for (int i = 0; i < num_buckets; i++) {
            double dist = features.distance(centroids[i]);
            if (dist < min_dist) {
                min_dist = dist;
                best_bucket = i;
            }
        }

        return best_bucket;
    }

private:
    int num_buckets;
    std::vector<HandFeatures> centroids;
};

#endif // HAND_BUCKETING_H
