#include <cassert>

struct Pair { int a, b; };
Pair get() { return {7, 8}; }

int main() {
    int a = 0;
    int b = 100;  // outer b, deliberately different value, deliberately different scope
    {
        auto [using a, b] = get();  // a: reused+assigned. b: a NEW local (shadows outer b).
        assert(a == 7);
        assert(b == 8);
    }
    // If `a, b` had been swallowed as one comma-expression using-target, this
    // whole binding would need exactly 1 name for a 2-element Pair -- a
    // compile error, not a silent wrong-value bug. But that's a parser-level
    // argument. This is the semantic argument: even granting it compiles,
    // these two checks prove it's really two independent bindings, not one.
    assert(a == 7);    // a's mutation is the same outer object -- persists.
    assert(b == 100);  // outer b is untouched -- the inner b was a distinct,
                        // newly-declared variable, not a reuse of outer b.
    return 0;
}
