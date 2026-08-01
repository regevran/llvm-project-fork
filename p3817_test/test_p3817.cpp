
#include <print>
#include <cassert>

int main() {
    int x = 0;
    int arr[3] = {42, 43, 44};

    // Testing P3817: assuming the syntax allows binding to existing x
    // with the keyword using
    auto [t, using x, z] = arr; 

    assert(t==42);
    assert(x==43);
    assert(z==44);

    return x;
}
