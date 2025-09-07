#ifndef GAME_TREE_H
#define GAME_TREE_H
#include "game.h"
#include "list.h"
#include <vector>
#include <type_traits>
#include <sstream>
#include <limits>

template<auto V>
struct integer_for_value {
    using type = decltype([] {
        if constexpr (V >= 0) {
            if constexpr (V <= std::numeric_limits<uint8_t>::max()) {
                return uint8_t{};
            } else if constexpr (V <= std::numeric_limits<uint16_t>::max()) {
                return uint16_t{};
            } else if constexpr (V <= std::numeric_limits<uint32_t>::max()) {
                return uint32_t{};
            } else {
                static_assert(V <= std::numeric_limits<uint64_t>::max(),
                              "Value is too large for a 64-bit unsigned integer.");
                return uint64_t{};
            }
        } else {
            if constexpr (V >= std::numeric_limits<int8_t>::min()) {
                return int8_t{};
            } else if constexpr (V >= std::numeric_limits<int16_t>::min()) {
                return int16_t{};
            } else if constexpr (V >= std::numeric_limits<int32_t>::min()) {
                return int32_t{};
            } else {
                static_assert(V >= std::numeric_limits<int64_t>::min(),
                              "Value is too small for a 64-bit signed integer.");
                return int64_t{};
            }
        }
    }());
};

template<auto V>
using integer_for_value_t = typename integer_for_value<V>::type;

#define DEFINE_HAS_TYPE(TRAIT_NAME, TYPE_NAME)                                     \
    template <typename T, typename = void>                                         \
    struct TRAIT_NAME : std::false_type {};                                        \
                                                                                   \
    template <typename T>                                                          \
    struct TRAIT_NAME<T, std::void_t<typename T::TYPE_NAME>> : std::true_type {};  \
                                                                                   \
    template <typename T>                                                          \
    inline constexpr bool TRAIT_NAME##_v = TRAIT_NAME<T>::value

DEFINE_HAS_TYPE(has_Proba, Proba);

template<typename Game, typename IntNb = integer_for_value_t<Game::MAX_NB_ACTIONS>, typename IntIdx = uint32_t>
struct GameTree {
    using State   = Game::State;
    using InfoSet = Game::InfoSet;
    using Action  = Game::Action;

    static_assert(Game::MAX_NB_ACTIONS < sizeof(IntNb) * 8,
                    "IntNb is not large enough to represent the maximum number of actions + 1");
    static_assert(sizeof(decltype(std::declval<Game>().payoff(PLAYER1))) <= sizeof(IntIdx),
                    "The return type of Game::payoff() is larger than IntIdx.");

    std::vector<InfoSet> info_sets;
    std::vector<Action>  actions;
    std::vector<IntNb>   nb_children;
    std::vector<IntIdx>  start_children_and_actions;
    std::vector<IntIdx>  children;
    
    GameTree() {
        Game game;
        build_tree(game);
        info_sets.shrink_to_fit();
        actions.shrink_to_fit();
        nb_children.shrink_to_fit();
        start_children_and_actions.shrink_to_fit();
        children.shrink_to_fit();
    }

    IntIdx build_tree(Game& game) {
        IntIdx root = static_cast<IntIdx>(info_sets.size());
        info_sets.push_back(game.get_info_set(game.current_player()));
        start_children_and_actions.push_back(static_cast<IntIdx>(children.size()));
        if (game.game_over()) {
            nb_children.push_back(0);
            using IntIdxSigned = std::make_signed_t<IntIdx>;
            children.push_back(static_cast<IntIdx>(static_cast<IntIdxSigned>(game.payoff(PLAYER1))));
            actions.push_back(Action{});
            return root;
        }
        List<Action, Game::MAX_NB_ACTIONS> action_list;
        game.actions(action_list);
        actions.insert(actions.end(), action_list.begin(), action_list.end());
        nb_children.push_back(static_cast<IntNb>(action_list.size()));        
        IntIdx children_index = static_cast<IntIdx>(children.size());
        children.insert(children.end(), action_list.size(), IntIdx{});
        for (const Action a : action_list) {
            game.play(a);
            IntIdx idx = build_tree(game);
            game.undo(a);
            children[children_index++] = idx;    
        }
        return root;
    }

    size_t nb_nodes() const {
        return info_sets.size();
    }
};

namespace {
    std::string repeat(const std::string& s, size_t n) {
        std::stringstream ss;
        for (size_t i = 0; i < n; i++) {
            ss << s;
        }
        return ss.str();
    }
    template<typename T>
    std::string convert(const T& x) {
        std::stringstream ss;
        ss << x;
        return ss.str();
    }
    template<typename Game, typename IntNb = uint8_t, typename IntIdx = uint32_t>
    void print_tree(std::ostream& os, const GameTree<Game, IntNb, IntIdx>& tree, IntIdx idx = 0, const std::string& prev_action = "", const std::string& prefix = "") {
        IntNb n = tree.nb_children[idx];
        IntIdx start = tree.start_children_and_actions[idx];
        if (n == 0) {
            os << prev_action << " (" << static_cast<std::make_signed_t<IntIdx>>(tree.children[start]) << ')'; 
            return;
        }
        //std::string info_set = convert(tree.info_sets[idx]);
        //os << "[" << prev_action << "] ";// << info_set;
        os << prev_action;// << info_set;
        //size_t w = /*info_set.size() +*/ prev_action.size() + 3;
        size_t w = prev_action.size();
        std::string p = prefix + repeat(" ", w + 1);             
        if (n == 1) {
            os << "---";
            print_tree(os, tree, tree.children[start], convert(tree.actions[start]), p + "  ");
        } else {
            os << "-+-";
            print_tree(os, tree, tree.children[start], convert(tree.actions[start]), p + "| ");
            os << '\n';
            os << p;
            for (IntNb i = 1; i < n - 1; i++) {
                os << "|-";
                print_tree(os, tree, tree.children[start + i], convert(tree.actions[start + i]), p + "| ");
                os << '\n';
                os << p;
            }
            os << "`-";
            print_tree(os, tree, tree.children[start + n - 1], convert(tree.actions[start + n - 1]), p + "  ");
        }        
    }
}
template<typename Game, typename IntNb = uint8_t, typename IntIdx = uint32_t>
std::ostream& operator<<(std::ostream& os, const GameTree<Game, IntNb, IntIdx>& tree) {
    if (tree.nb_nodes() > 0) {
        print_tree(os, tree);
        os << "\n#nodes: " << tree.nb_nodes();
    } else {
        os << "empty";
    }
    return os;
}


#endif
