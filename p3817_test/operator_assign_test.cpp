// Verifies that `using`-marked bindings go through the real assignment
// operator (move for `auto`, copy for `auto&`), rather than a raw bitwise
// store, for both the aggregate/array decomposition path and the tuple-like
// (std::pair/std::tuple, via a holding var) decomposition path.

#include <cassert>
#include <cstdio>
#include <utility>

struct Tracker {
    int value = 0;
    int copy_assign_calls = 0;
    int move_assign_calls = 0;

    Tracker() = default;
    Tracker(int v) : value(v) {}
    Tracker(const Tracker&) = default;
    Tracker(Tracker&&) = default;

    Tracker& operator=(const Tracker& o) {
        value = o.value;
        copy_assign_calls++;
        return *this;
    }
    Tracker& operator=(Tracker&& o) {
        value = o.value;
        move_assign_calls++;
        return *this;
    }
};

struct Pair { Tracker a; Tracker b; };

Pair make_aggregate_pair() { return Pair{Tracker(10), Tracker(20)}; }
std::pair<Tracker, Tracker> make_tuple_like_pair() {
    return {Tracker(10), Tracker(20)};
}

int main() {
    // Aggregate/member decomposition, auto -> should MOVE.
    {
        Tracker t;
        Pair p = make_aggregate_pair();
        { auto [using t, y] = p; }
        assert(t.value == 10);
        assert(t.move_assign_calls == 1);
        assert(t.copy_assign_calls == 0);
        printf("aggregate/auto move: OK\n");
    }

    // Aggregate/member decomposition, auto& -> should COPY.
    {
        Tracker t;
        Pair p = make_aggregate_pair();
        { auto& [using t, y] = p; }
        assert(t.value == 10);
        assert(t.copy_assign_calls == 1);
        assert(t.move_assign_calls == 0);
        printf("aggregate/auto& copy: OK\n");
    }

    // Tuple-like (std::pair) decomposition, auto -> should MOVE.
    {
        Tracker t;
        { auto [using t, y] = make_tuple_like_pair(); }
        assert(t.value == 10);
        assert(t.move_assign_calls == 1);
        assert(t.copy_assign_calls == 0);
        printf("tuple-like/auto move: OK\n");
    }

    // Tuple-like (std::pair) decomposition, auto& -> should COPY.
    {
        Tracker t;
        std::pair<Tracker, Tracker> p = make_tuple_like_pair();
        { auto& [using t, y] = p; }
        assert(t.value == 10);
        assert(t.copy_assign_calls == 1);
        assert(t.move_assign_calls == 0);
        printf("tuple-like/auto& copy: OK\n");
    }

    return 0;
}
