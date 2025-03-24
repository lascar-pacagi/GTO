#ifndef ACTION_LIST_H
#define ACTION_LIST_H

template<typename Action>
struct ActionList {
    Action actionList[MAX_NB_MOVES], *last;
    explicit ActionList() : last(actionList) {}
    const Action* begin() const { return actionList; }
    const Action* end() const { return last; }
    Action* begin() { return actionList; }
    Action* end() { return last; }
    size_t size() const { return last - actionList; }
    const Action& operator[](size_t i) const { return actionList[i]; }
    Action& operator[](size_t i) { return actionList[i]; }
    Action* data() { return actionList; }
};

#endif
