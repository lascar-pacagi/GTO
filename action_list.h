#ifndef ACTION_LIST_H
#define ACTION_LIST_H

template<typename Action, int MAX_NB_ACTIONS>
struct ActionList {
    Action action_list[MAX_NB_ACTIONS], *last;
    explicit ActionList() : last(action_list) {}
    const Action* begin() const { return action_list; }
    const Action* end() const { return last; }
    Action* begin() { return action_list; }
    Action* end() { return last; }
    size_t size() const { return last - action_list; }
    const Action& operator[](size_t i) const { return action_list[i]; }
    Action& operator[](size_t i) { return action_list[i]; }
    Action* data() { return action_list; }
};

#endif
