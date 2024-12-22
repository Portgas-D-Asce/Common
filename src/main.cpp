#include <iostream>
#include <string>
#include <vector>

#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"

int main() {
    std::cout << "Hello, World!" << std::endl;
    spdlog::info("Hello, World!");
    std::vector<std::string> ss = {"hello", "world"};
    nlohmann::json j(ss);
    std::cout << j.dump() << std::endl;
    return 0;
}
