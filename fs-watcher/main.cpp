#include "file_loader.h"
#include "fs_watcher.h"

#include <iostream>

#include <getopt.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <fmt/core.h>


using namespace std;


int main(int argc, char *argv[]) {

    if (argc < 2)
    {
        fmt::print("Invalid usage.");
        return EXIT_FAILURE;
    }
    if (argc < 3)
    {
        fmt::print("No command provided for the event handler.");
        return EXIT_FAILURE;
    }

    const char* command_s = argv[2];
    std::vector<std::string> filepaths{};

    load_json(argv[1], filepaths);
    fmt::print("\n\nThe vector of strings:\n");
    for (const auto& s : filepaths) {
        fmt::print("\n{0}\n", s);
    }

    size_t n_files = filepaths.size();
    const char* _filepaths[n_files];

    std::transform(filepaths.begin(), filepaths.end(), &_filepaths[0], [](const auto& fp){
        return fp.c_str();
    });

    std::cout << "\n\nbefore add_watchers\n\n" << std::endl;

    fmt::print("The _filepaths list:\n");
    for (const auto* fp : _filepaths) {
        fmt::print("{0}\n", fp);
    }

    Watcher watcher{};

    fmt::print("\ncalling add_watchers\n");

    watcher.start(n_files, _filepaths, command_s, IN_MODIFY);

    return 0;
}
