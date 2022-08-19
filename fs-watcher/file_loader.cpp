#include "file_loader.h"

#include <cstddef>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <fmt/core.h>
#include <json.hpp>

void load_json(const char* fp, std::vector<std::string>& out) {
    using namespace nlohmann;

    std::ifstream ifs { fp };

    if (not ifs) {
        fmt::print("Failed to open file");
        throw std::runtime_error("Nope");
    }

    json j = json::parse(ifs);

    std::cout << j.dump(4) << std::endl;

    auto remove_quotes = [](std::string_view s) {
        return s.substr(1, s.size()-2);
    };

    auto output_it = std::back_inserter(out);
    //const char* list[j.size()];

    fmt::print("\n\nList of files:\n");
    for (const auto& s : j) {
        fmt::print("\n{0}\n", s);
        output_it = s;
    }

    fmt::print("Leaving load_json\n");
}
