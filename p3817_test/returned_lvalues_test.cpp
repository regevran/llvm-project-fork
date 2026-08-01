#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <utility>

int& foo() { static int storage = 0; return storage; }
std::pair<int,int> get_pair() { return {7, 8}; }

int main() {
    // using foo() -- function returning an lvalue reference.
    {
        auto [using foo(), y] = get_pair();
        assert(foo() == 7);
        assert(y == 8);
        printf("using foo(): OK\n");
    }

    // using s[0] -- array subscript.
    {
        int s[2] = {0, 0};
        auto [using s[0], y] = get_pair();
        assert(s[0] == 7);
        assert(y == 8);
        printf("using s[0]: OK\n");
    }

    // using params["kn"] -- map subscript (from the scylladb example in the paper).
    {
        std::map<std::string, std::string> params;
        auto split = [](std::string s) { return std::pair<std::string,std::string>{s, s}; };
        auto [using params["kn"], using params["cf"]] = split("hello");
        assert(params["kn"] == "hello");
        assert(params["cf"] == "hello");
        printf("using params[\"kn\"]: OK\n");
    }

    // using obj.member -- explicit member access on a named object.
    {
        struct Point { int x, y; };
        Point p{0, 0};
        auto [using p.x, y] = get_pair();
        assert(p.x == 7);
        assert(y == 8);
        printf("using p.x: OK\n");
    }

    return 0;
}
