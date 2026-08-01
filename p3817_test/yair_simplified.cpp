
#include <cassert>

int main() {
    int ar[2] = {31, 32};
    auto [y, x] = ar;
    { auto [using x, f] = ar; }

    return 0;
}
