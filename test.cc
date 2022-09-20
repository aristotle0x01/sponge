#include <algorithm>
#include <iostream>
#include <map>
#include <string_view>
#include <vector>

void print(std::string_view comment, const auto &data) {
    std::cout << comment;
    for (auto [k, v] : data)
        std::cout << ' ' << k << '(' << v << ')';

    std::cout << '\n';
}

int main() {
    uint32_t r = (189999 ^ 0) >> 32;
    std::cout << "result: " << r << "\n";
}
// g++ -o t test.cc -std=c++17