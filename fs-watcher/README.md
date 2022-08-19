# fs-watcher

A script to watch source files and runs a specified build script on modification. 

## Building the binary

- cmake -S . -B build
- cmake --build build

## Usage

Specify the root directory of the project, then a list of path to the files to watch.
The filepaths must be relative to the root directory.
Optionally, pass pass in the path to an executable that will be run each time one of the watched files is modified.
In case no such executable is passed, `cmake --build build` will be run.

- ./build/fs-watcher/fs_watcher INPUT_FILES.json EXECUTABLE_HANDLER_FILE

### TODOS

- [X] Specify files to watch from auxiliary config file.
- [] Specify files to watch from command line.
- [] Get files to watch automatically from git.
- [] Get files to watch from cmake script.




