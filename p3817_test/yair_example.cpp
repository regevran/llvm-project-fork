// solution to test1 "using" keyword
// https://godbolt.org/z/WavsEec68

#include <map>
#include <print>
#include <string>

void map_example() {
    std::string k = "Hello";
    std::string v = "World";
    std::map<std::string, std::string> m {{k, v}};
    auto [p, inserted] = m.try_emplace(k, v) ;
    if (!inserted) {
       // Fixing the error
        k = k + "!";
        v = "New " + v;
        auto [using p, using inserted] = m.try_emplace(k, v) ;
        if (!inserted) {
            std::println("fixed '{}' not inserted?", k);
        }
        else {
             auto [using k, using v] = *p;
             std::println("fixed inserted '{}'='{}'", k, v);
        }
    }
}
/*
// Return default on error
int to_int(const std::string& s);
// Throw on error
int to_int(const std::string& s);
// Return code on error.
rc to_int(const std::string& s, int& result);
int to_int(const std::string& s, rc& status);
// Return nullptr on error
std::unique_ptr<int> to_int(const std::string& s);
int* to_int(const std::string& s);
*/
enum class to_int_rc {success , invalid_argument, out_of_range};
std::pair<int, to_int_rc> to_int(const std::string& s)
{
    try {
        int num = std::stoi(s);
        return {num, to_int_rc::success};
    } catch (const std::invalid_argument&) {
        return {0, to_int_rc::invalid_argument};
    } catch (const std::out_of_range&) {
        return {0, to_int_rc::out_of_range};
    } 
} 

void to_int_example()
{
    auto [num, rc] = to_int("42");
    std::println("rc={} num={}", static_cast<int>(rc), num);
    auto [using num, using rc] = to_int("Forty Two");
    std::println("rc={} num={}", static_cast<int>(rc), num);
    auto [using num, using rc] = to_int("42424242424242424242");
    std::println("rc={} num={}", static_cast<int>(rc), num);
}

int main() {
    map_example();
    to_int_example();
    return 0;
}

