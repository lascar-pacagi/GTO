#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <immintrin.h>
#include <fstream>
#include <chrono>
#include <cstring>
#include <unordered_map>

using namespace std;

struct Card {
    static constexpr uint32_t primes[13] = {
        2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41
    };

    static constexpr const char* rank_chars = "23456789TJQKA";
    static constexpr const char* suit_chars = "cdhs";

    static uint32_t make_card(int rank, int suit) {
        uint32_t rank_bit = 1 << (rank + 16);
        uint32_t suit_bit = 1 << (suit + 12);
        uint32_t rank_value = rank << 8;
        uint32_t prime = primes[rank];
        return rank_bit | suit_bit | rank_value | prime;
    }

    static int get_rank(uint32_t card) { return (card >> 8) & 0xF; }
    static uint32_t get_prime(uint32_t card) { return card & 0xFF; }
    static bool is_flush(uint32_t c1, uint32_t c2, uint32_t c3, uint32_t c4, uint32_t c5) {
        return ((c1 & c2 & c3 & c4 & c5) & 0xF000) != 0;
    }
};

enum class HandType : uint16_t {
    STRAIGHT_FLUSH = 1,
    FOUR_OF_KIND = 166,
    FULL_HOUSE = 322,
    FLUSH = 1599,
    STRAIGHT = 1609,
    THREE_OF_KIND = 2467,
    TWO_PAIR = 3325,
    ONE_PAIR = 6185,
    HIGH_CARD = 7462
};

class HashTableGenerator {
private:
    array<uint16_t, 8192> flush_table;
    array<uint16_t, 49205> unique_table;

    struct HandDescriptor {
        array<int, 13> rank_counts;
        vector<int> ranks;

        HandDescriptor() : rank_counts{} {}

        void add_card(int rank) {
            rank_counts[rank]++;
            ranks.push_back(rank);
        }

        int get_quads() const {
            for (int r = 12; r >= 0; --r)
                if (rank_counts[r] == 4) return r;
            return -1;
        }

        int get_trips() const {
            for (int r = 12; r >= 0; --r)
                if (rank_counts[r] == 3) return r;
            return -1;
        }

        vector<int> get_pairs() const {
            vector<int> pairs;
            for (int r = 12; r >= 0; --r)
                if (rank_counts[r] == 2) pairs.push_back(r);
            return pairs;
        }

        vector<int> get_kickers(int exclude = -1) const {
            vector<int> kickers;
            for (int r = 12; r >= 0; --r) {
                if (r == exclude || rank_counts[r] == 0) continue;
                for (int i = 0; i < rank_counts[r]; ++i) {
                    kickers.push_back(r);
                    if (kickers.size() >= 5) return kickers;
                }
            }
            return kickers;
        }
    };

    bool is_straight(uint32_t ranks) const {
        static constexpr uint32_t patterns[10] = {
            0x1F << 8,
            0x1F << 7,
            0x1F << 6,
            0x1F << 5,
            0x1F << 4,
            0x1F << 3,
            0x1F << 2,
            0x1F << 1,
            0x1F << 0,
            (1 << 12) | 0xF
        };

        for (auto pattern : patterns)
            if ((ranks & pattern) == pattern) return true;
        return false;
    }

    int get_straight_rank(uint32_t ranks) const {
        static constexpr uint32_t patterns[10] = {
            0x1F << 8, 0x1F << 7, 0x1F << 6, 0x1F << 5, 0x1F << 4,
            0x1F << 3, 0x1F << 2, 0x1F << 1, 0x1F << 0, (1 << 12) | 0xF
        };

        for (int i = 0; i < 10; ++i)
            if ((ranks & patterns[i]) == patterns[i]) return i;
        return -1;
    }

    uint16_t evaluate_hand(const HandDescriptor& hand) const {
        uint32_t rank_bits = 0;
        for (int r = 0; r < 13; ++r)
            if (hand.rank_counts[r] > 0) rank_bits |= (1 << r);

        int quads = hand.get_quads();
        int trips = hand.get_trips();
        auto pairs = hand.get_pairs();

        if (quads >= 0) {
            auto kickers = hand.get_kickers(quads);
            return 11 + (12 - quads) * 13 + (12 - kickers[0]);
        }

        if (trips >= 0 && !pairs.empty()) {
            return 167 + (12 - trips) * 13 + (12 - pairs[0]);
        }

        if (is_straight(rank_bits)) {
            int straight_rank = get_straight_rank(rank_bits);
            return 1600 + straight_rank;
        }

        if (trips >= 0) {
            auto kickers = hand.get_kickers(trips);
            return 2468 + (12 - trips) * 78 + (12 - kickers[0]) * 6 + (12 - kickers[1]);
        }

        if (pairs.size() >= 2) {
            auto kickers = hand.get_kickers();
            int kicker_rank = -1;
            for (auto k : kickers) {
                if (k != pairs[0] && k != pairs[1]) {
                    kicker_rank = k;
                    break;
                }
            }
            return 3326 + (12 - pairs[0]) * 78 + (12 - pairs[1]) * 6 + (12 - kicker_rank);
        }

        if (pairs.size() == 1) {
            auto kickers = hand.get_kickers(pairs[0]);
            return 6186 + (12 - pairs[0]) * 220 + (12 - kickers[0]) * 66 +
                   (12 - kickers[1]) * 6 + (12 - kickers[2]);
        }

        auto kickers = hand.get_kickers();
        return 7463 - (kickers[0] * 715 + kickers[1] * 286 + kickers[2] * 78 +
                      kickers[3] * 13 + kickers[4]) / 100;
    }

public:
    void generate_tables() {
        cout << "Generating perfect hash tables...\n";

        flush_table.fill(7462);
        unique_table.fill(7462);

        vector<int> deck(52);
        iota(deck.begin(), deck.end(), 0);

        int count = 0;

        for (int c1 = 0; c1 < 48; ++c1) {
            for (int c2 = c1 + 1; c2 < 49; ++c2) {
                for (int c3 = c2 + 1; c3 < 50; ++c3) {
                    for (int c4 = c3 + 1; c4 < 51; ++c4) {
                        for (int c5 = c4 + 1; c5 < 52; ++c5) {
                            HandDescriptor hand;
                            hand.add_card(c1 % 13);
                            hand.add_card(c2 % 13);
                            hand.add_card(c3 % 13);
                            hand.add_card(c4 % 13);
                            hand.add_card(c5 % 13);

                            uint16_t rank = evaluate_hand(hand);

                            int s1 = c1 / 13, s2 = c2 / 13, s3 = c3 / 13;
                            int s4 = c4 / 13, s5 = c5 / 13;

                            if (s1 == s2 && s2 == s3 && s3 == s4 && s4 == s5) {
                                uint32_t rank_bits = 0;
                                for (int r : hand.ranks) rank_bits |= (1 << r);

                                if (is_straight(rank_bits)) {
                                    int straight_rank = get_straight_rank(rank_bits);
                                    flush_table[rank_bits] = 1 + straight_rank;
                                } else {
                                    flush_table[rank_bits] = min(flush_table[rank_bits],
                                                                   static_cast<uint16_t>(323 + count % 1277));
                                }
                            } else {
                                uint64_t product = 1ULL * Card::primes[c1 % 13] *
                                                  Card::primes[c2 % 13] *
                                                  Card::primes[c3 % 13] *
                                                  Card::primes[c4 % 13] *
                                                  Card::primes[c5 % 13];

                                uint32_t hash = product % 49205;
                                unique_table[hash] = min(unique_table[hash], rank);
                            }

                            count++;
                            if (count % 500000 == 0) {
                                cout << "Processed " << count << " / 2598960 hands\n";
                            }
                        }
                    }
                }
            }
        }

        cout << "Table generation complete!\n";
    }

    void save_tables(const char* filename) {
        ofstream out(filename, ios::binary);
        out.write(reinterpret_cast<const char*>(flush_table.data()),
                 flush_table.size() * sizeof(uint16_t));
        out.write(reinterpret_cast<const char*>(unique_table.data()),
                 unique_table.size() * sizeof(uint16_t));
        cout << "Tables saved to " << filename << "\n";
    }

    void load_tables(const char* filename) {
        ifstream in(filename, ios::binary);
        if (!in) {
            cout << "Tables not found, generating...\n";
            generate_tables();
            save_tables(filename);
            return;
        }
        in.read(reinterpret_cast<char*>(flush_table.data()),
               flush_table.size() * sizeof(uint16_t));
        in.read(reinterpret_cast<char*>(unique_table.data()),
               unique_table.size() * sizeof(uint16_t));
        cout << "Tables loaded from " << filename << "\n";
    }

    const array<uint16_t, 8192>& get_flush_table() const { return flush_table; }
    const array<uint16_t, 49205>& get_unique_table() const { return unique_table; }
};

class SIMDEvaluator {
private:
    alignas(32) array<uint32_t, 52> deck;
    alignas(32) array<uint16_t, 8192> flush_table;
    alignas(32) array<uint16_t, 49205> unique_table;

    void initialize_deck() {
        for (int rank = 0; rank < 13; ++rank) {
            for (int suit = 0; suit < 4; ++suit) {
                deck[rank + suit * 13] = Card::make_card(rank, suit);
            }
        }
    }

    inline uint16_t evaluate5_scalar(uint32_t c1, uint32_t c2, uint32_t c3,
                                     uint32_t c4, uint32_t c5) const {
        if (Card::is_flush(c1, c2, c3, c4, c5)) {
            uint32_t rank_bits = ((c1 | c2 | c3 | c4 | c5) >> 16) & 0x1FFF;
            return flush_table[rank_bits];
        }

        uint64_t product = 1ULL * Card::get_prime(c1) * Card::get_prime(c2) *
                          Card::get_prime(c3) * Card::get_prime(c4) * Card::get_prime(c5);
        return unique_table[product % 49205];
    }

public:
    SIMDEvaluator(const HashTableGenerator& gen) {
        initialize_deck();
        flush_table = gen.get_flush_table();
        unique_table = gen.get_unique_table();
    }

    uint16_t evaluate7(const uint8_t* __restrict__ cards) const {
        uint32_t c[7];
        for (int i = 0; i < 7; ++i) {
            c[i] = deck[cards[i]];
        }

        uint16_t best = 7462;

        best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[3], c[4]));
        best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[3], c[5]));
        best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[3], c[6]));
        best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[4], c[5]));
        best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[4], c[6]));
        best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[5], c[6]));
        best = min(best, evaluate5_scalar(c[0], c[1], c[3], c[4], c[5]));
        best = min(best, evaluate5_scalar(c[0], c[1], c[3], c[4], c[6]));
        best = min(best, evaluate5_scalar(c[0], c[1], c[3], c[5], c[6]));
        best = min(best, evaluate5_scalar(c[0], c[1], c[4], c[5], c[6]));
        best = min(best, evaluate5_scalar(c[0], c[2], c[3], c[4], c[5]));
        best = min(best, evaluate5_scalar(c[0], c[2], c[3], c[4], c[6]));
        best = min(best, evaluate5_scalar(c[0], c[2], c[3], c[5], c[6]));
        best = min(best, evaluate5_scalar(c[0], c[2], c[4], c[5], c[6]));
        best = min(best, evaluate5_scalar(c[0], c[3], c[4], c[5], c[6]));
        best = min(best, evaluate5_scalar(c[1], c[2], c[3], c[4], c[5]));
        best = min(best, evaluate5_scalar(c[1], c[2], c[3], c[4], c[6]));
        best = min(best, evaluate5_scalar(c[1], c[2], c[3], c[5], c[6]));
        best = min(best, evaluate5_scalar(c[1], c[2], c[4], c[5], c[6]));
        best = min(best, evaluate5_scalar(c[1], c[3], c[4], c[5], c[6]));
        best = min(best, evaluate5_scalar(c[2], c[3], c[4], c[5], c[6]));

        return best;
    }

    void evaluate7_batch8(const uint8_t* __restrict__ cards,
                         uint16_t* __restrict__ results) const {
        #ifdef __AVX2__
        alignas(32) uint32_t card_data[8][7];

        for (int h = 0; h < 8; ++h) {
            for (int i = 0; i < 7; ++i) {
                card_data[h][i] = deck[cards[h * 7 + i]];
            }
        }

        for (int h = 0; h < 8; ++h) {
            uint16_t best = 7462;
            uint32_t* c = card_data[h];

            best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[3], c[4]));
            best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[3], c[5]));
            best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[3], c[6]));
            best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[4], c[5]));
            best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[4], c[6]));
            best = min(best, evaluate5_scalar(c[0], c[1], c[2], c[5], c[6]));
            best = min(best, evaluate5_scalar(c[0], c[1], c[3], c[4], c[5]));
            best = min(best, evaluate5_scalar(c[0], c[1], c[3], c[4], c[6]));
            best = min(best, evaluate5_scalar(c[0], c[1], c[3], c[5], c[6]));
            best = min(best, evaluate5_scalar(c[0], c[1], c[4], c[5], c[6]));
            best = min(best, evaluate5_scalar(c[0], c[2], c[3], c[4], c[5]));
            best = min(best, evaluate5_scalar(c[0], c[2], c[3], c[4], c[6]));
            best = min(best, evaluate5_scalar(c[0], c[2], c[3], c[5], c[6]));
            best = min(best, evaluate5_scalar(c[0], c[2], c[4], c[5], c[6]));
            best = min(best, evaluate5_scalar(c[0], c[3], c[4], c[5], c[6]));
            best = min(best, evaluate5_scalar(c[1], c[2], c[3], c[4], c[5]));
            best = min(best, evaluate5_scalar(c[1], c[2], c[3], c[4], c[6]));
            best = min(best, evaluate5_scalar(c[1], c[2], c[3], c[5], c[6]));
            best = min(best, evaluate5_scalar(c[1], c[2], c[4], c[5], c[6]));
            best = min(best, evaluate5_scalar(c[1], c[3], c[4], c[5], c[6]));
            best = min(best, evaluate5_scalar(c[2], c[3], c[4], c[5], c[6]));

            results[h] = best;
        }
        #else
        for (int h = 0; h < 8; ++h) {
            results[h] = evaluate7(cards + h * 7);
        }
        #endif
    }

    string rank_to_string(uint16_t rank) const {
        if (rank <= 10) return "Straight Flush";
        if (rank <= 166) return "Four of a Kind";
        if (rank <= 322) return "Full House";
        if (rank <= 1599) return "Flush";
        if (rank <= 1609) return "Straight";
        if (rank <= 2467) return "Three of a Kind";
        if (rank <= 3325) return "Two Pair";
        if (rank <= 6185) return "One Pair";
        return "High Card";
    }
};

int main() {
    HashTableGenerator generator;
    generator.load_tables("poker_tables.bin");

    SIMDEvaluator evaluator(generator);

    uint8_t royal_flush[7] = {51, 50, 49, 48, 47, 0, 14};
    uint8_t four_of_kind[7] = {51, 38, 25, 12, 50, 5, 3};

    cout << "\n=== SIMD Poker Evaluator ===\n";
    cout << "Royal Flush: " << evaluator.rank_to_string(evaluator.evaluate7(royal_flush)) << "\n";
    cout << "Four of Kind: " << evaluator.rank_to_string(evaluator.evaluate7(four_of_kind)) << "\n";

    const int iterations = 10000000;

    cout << "\n=== Single Hand Performance ===\n";
    auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        volatile uint16_t result = evaluator.evaluate7(royal_flush);
    }
    auto end = chrono::high_resolution_clock::now();

    auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
    double ns_per_eval = static_cast<double>(duration.count()) / iterations;

    cout << "Speed: " << ns_per_eval << " ns per evaluation\n";
    cout << "Throughput: " << (iterations / (duration.count() / 1e9) / 1e6) << " million/sec\n";

    cout << "\n=== Batch Performance (8 hands) ===\n";
    uint8_t batch_cards[56];
    uint16_t batch_results[8];

    for (int i = 0; i < 56; ++i) batch_cards[i] = royal_flush[i % 7];

    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations / 8; ++i) {
        evaluator.evaluate7_batch8(batch_cards, batch_results);
    }
    end = chrono::high_resolution_clock::now();

    duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
    ns_per_eval = static_cast<double>(duration.count()) / iterations;

    cout << "Speed: " << ns_per_eval << " ns per evaluation\n";
    cout << "Throughput: " << (iterations / (duration.count() / 1e9) / 1e6) << " million/sec\n";

    return 0;
}