
#include <cassert>

int main() {

    int ar[1] = {31};
    auto [x] = ar;
    { auto [using x] = ar; }

    return 0;
}
