#ifndef LIST_H
#define LIST_H

template<typename T, int MAX_NB_ELEMENTS>
struct List {
    T list[MAX_NB_ELEMENTS], *last;
    explicit List() : last(list) {}
    const T* begin() const { return list; }
    const T* end() const { return last; }
    T* begin() { return list; }
    T* end() { return last; }
    size_t size() const { return last - list; }
    const T& operator[](size_t i) const { return list[i]; }
    T& operator[](size_t i) { return list[i]; }
    T* data() { return list; }
};

#endif
