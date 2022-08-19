#include "file_loader.h"

#include <fstream>
#include <stdexcept>
#include <string>

#include <json.hpp>

void load_json(const char* fp, std::vector<std::string>& out) {
    using namespace nlohmann;

    std::ifstream ifs { fp };

    if (not ifs) {
        throw std::runtime_error("Failed to open JSON file");
    }

    json j = json::parse(ifs);

    std::copy(j.begin(), j.end(), std::back_inserter(out));
}
