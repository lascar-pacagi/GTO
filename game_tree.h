#ifndef GAME_TREE_H
#define GAME_TREE_H
#include "game.h"
#include "list.h"
#include <vector>
#include <sstream>
#include <cmath>
#include <map>

template<typename Game>
struct GameTree {
    using State   = Game::State;
    using InfoSet = Game::InfoSet;
    using Action  = Game::Action;

    std::map<InfoSet, std::vector<State>> info_set_to_states;
    std::map<State, int> state_to_idx;
    std::vector<InfoSet> info_sets;
    std::vector<Action>  actions;
    std::vector<int> nb_children;
    std::vector<int> start_children_and_actions;
    std::vector<int> children;
    
    GameTree() {
        Game game;
        build_tree(game);
        info_sets.shrink_to_fit();
        actions.shrink_to_fit();
        nb_children.shrink_to_fit();
        start_children_and_actions.shrink_to_fit();
        children.shrink_to_fit();
    }
    int build_tree(Game& game) {
        int root = static_cast<int>(info_sets.size());
        InfoSet info_set = game.get_info_set(game.current_player());
        state_to_idx[game.get_state()] = root;
        info_sets.push_back(info_set);
        start_children_and_actions.push_back(static_cast<int>(children.size()));
        if (game.game_over()) {
            nb_children.push_back(0);
            children.push_back(game.payoff(PLAYER1));
            //std::cout << game.action_history << ':' << game.payoff(PLAYER1) << ',';
            //std::cout << '{' << game.action_history << ',' << game.payoff(PLAYER1) << "},"; 
            actions.push_back(Action{});
            return root;
        }
        if (game.is_chance_player()) {
            List<Action, Game::MAX_NB_CHANCE_ACTIONS> action_list;
            game.actions(action_list);
            List<int, Game::MAX_NB_CHANCE_ACTIONS> proba_list;
            game.probas(proba_list);
            int n = static_cast<int>(action_list.size());
            nb_children.push_back((n << 2) + CHANCE);
            actions.insert(actions.end(), action_list.begin(), action_list.end());
            actions.insert(actions.end(), n, Action{});
            int children_and_proba_index = static_cast<int>(children.size());
            children.insert(children.end(), 2 * n, 0);
            for (int i = 0; i < n; i++) {
                Action a = action_list[i]; 
                game.play(a);
                int idx = build_tree(game);
                game.undo(a);
                children[children_and_proba_index++] = idx;
                children[children_and_proba_index++] = proba_list[i];                
            }
        } else {
            info_set_to_states[info_set].push_back(game.get_state());        
            List<Action, Game::MAX_NB_PLAYER_ACTIONS> action_list;
            game.actions(action_list);
            nb_children.push_back((static_cast<int>(action_list.size()) << 2) + game.current_player());                    
            actions.insert(actions.end(), action_list.begin(), action_list.end());
            int children_index = static_cast<int>(children.size());
            children.insert(children.end(), action_list.size(), 0);
            for (Action a : action_list) {
                game.play(a);
                int idx = build_tree(game);
                game.undo(a);
                children[children_index++] = idx;
            }
        }
        return root;
    }

    size_t nb_nodes() const {
        return info_sets.size();
    }

    int get_state_idx(const State& state) const {
        return state_to_idx.at(state);
    }

    const std::vector<State>& get_states(const InfoSet& info_set) const {
        return info_set_to_states.at(info_set);
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
    template<typename Game>
    void print_tree(std::ostream& os, const GameTree<Game>& tree, int idx = 0, int proba = -1, const std::string& prev_action = "", const std::string& prefix = "") {
        int n = tree.nb_children[idx] >> 2;
        int player = tree.nb_children[idx] & 3;
        //std::cout << "#### " << idx << '\n';
        int start = tree.start_children_and_actions[idx];
        //std::cout << "@@@@ " << start << '\n';        
        if (n == 0) {
            os << prev_action << " (" << tree.children[start] << ')'; 
            return;
        }        
        // if (player == PLAYER1) std::cout << "PLAYER1" << ' ';
        // if (player == PLAYER2) std::cout << "PLAYER2" << ' ';
        // if (player == CHANCE) std::cout << "CHANCE" << ' ';
        //os << tree.info_sets[idx] << ' ';
        size_t w = prev_action.size() + 2;
        if (proba > -1) {
            std::string p = std::to_string(proba);
            w += p.size() + 1;
            os << p << ' ';
        }
        os << prev_action << ' ';
        std::string p = prefix + repeat(" ", w);
        if (n == 1) {
            os << "---";
            print_tree(os, tree, tree.children[start], (player == CHANCE ? tree.children[start + 1] : -1), convert(tree.actions[start]), p + "  ");
        } else {
            os << "-+-";
            print_tree(os, tree, tree.children[start], (player == CHANCE ? tree.children[start + 1] : -1), convert(tree.actions[start]), p + "| ");
            os << '\n';
            os << p;
            for (int i = 1; i < n - 1; i++) {
                os << "|-";
                print_tree(os, tree, (player == CHANCE ? tree.children[start + 2 * i] : tree.children[start + i]), 
                            (player == CHANCE ? tree.children[start + 2 * i + 1] : -1), convert(tree.actions[start + i]), p + "| ");
                os << '\n';
                os << p;
            }
            os << "`-";
            print_tree(os, tree, (player == CHANCE ? tree.children[start + 2 * n - 2] : tree.children[start + n - 1]), 
                        (player == CHANCE ? tree.children[start + 2 * n - 1] : -1), convert(tree.actions[start + n - 1]), p + "  ");
        }
    }
}
template<typename Game>
std::ostream& operator<<(std::ostream& os, const GameTree<Game>& tree) {
    if (tree.nb_nodes() > 0) {
        print_tree(os, tree);
        os << "\n#nodes: " << tree.nb_nodes();
    } else {
        os << "empty";
    }
    return os;
}


#endif
