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

    size_t n_files = filepaths.size();
    const char* _filepaths[n_files];

    std::transform(filepaths.begin(), filepaths.end(), &_filepaths[0], [](const auto& fp){
        return fp.c_str();
    });

    Watcher watcher{};
    watcher.start(n_files, _filepaths, command_s, IN_MODIFY);


    return EXIT_SUCCESS;
}
