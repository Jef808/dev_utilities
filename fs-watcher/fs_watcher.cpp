/**
 * The inofity API uses the following structure to
 * represent an event object.
 *
 * struct inotify_event
 * {
 *   int wd;               // Watch descriptor.
 *   uint32_t mask;        // Watch mask.
 *   uint32_t cookie;      // Cookie to synchronize two events.
 *   uint32_t len;         // Length (including NULs) of name.
 *   char name __flexarr;  // Name.
 * };
 *
 * NOTE: The `name` field is only present if the watched item is
 * a directory and the event is for an item in the directory (other
 * then the directory itself).
 */

//#include "pathutils.h"

#include <errno.h>
//#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
//#include <sys/types.h>

#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
//#include <numeric>
#include <thread>
#include <unordered_map>
#include <vector>

#include <fmt/core.h>

/* The size of an inotify event. */
constexpr auto EVENT_SIZE = sizeof(struct inotify_event);
/* Upper bound for the size of the name field of an event. */
constexpr auto NAME_MAX = 1024;
constexpr auto EVENT_TYPE_MAX = 10;
/* Buffer to hold inotify event objects. */
constexpr auto BUF_LEN = EVENT_SIZE + NAME_MAX + 1;  // With this size, we will always read at least one whole event.
/* The events we are interested in. */
constexpr auto EVENT_MASK = IN_MODIFY | IN_DELETE;

struct queue_entry_event {
    int wd;
    char type[EVENT_TYPE_MAX];
    char name[NAME_MAX];
};

/// The filepaths associated to each inotify watch descriptors.
std::unordered_map<int, std::string> wd_2_name;

/**
 * Wait until a recognized event occurs or a signal
 * interrupt is catched.
 *
 * If the optional parameter #timeout_ms is specified
 * and non-zero, only wait for #timeout_ms milliseconds.
 *
 * @Return the number of ready descriptors
 */
int event_check(int fd, int timeout_ms = 0) noexcept {
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);

  // If no timeout
  if (timeout_ms == 0)
    return select(FD_SETSIZE, &rfds, NULL, NULL, NULL);

  // With timeout
  static timeval timeout;
  timeout.tv_sec = 1000.0 * timeout_ms;
  return select(FD_SETSIZE, &rfds, NULL, NULL, &timeout);
}

/**
 * Process the events in our queue.
 *
 * @Return  -1 if an error occured, otherwise 0.
 */
// int process_inotify_events(std::deque<inotify_event> &event_queue, int fd,
//                            bool *should_keep_running) noexcept {
//   while (should_keep_running && not event_queue.empty()) {
//     if (event_check(fd) > 0) {
//       int r;
//       r = read_events(event_queue, fd);
//       if (r < 0) {
//         break;
//       } else {
//         const auto &event = event_queue.front();
//         handle_event(event);
//       }
//       event_queue.pop_front();
//     }
//   }
//   bool err = should_keep_running && not event_queue.empty();
//   return err * -1;
// }

/**
 * Add watchers for all provided filenames with given event mask.
 *
 * @Param inotify_fd  The file descriptor of an initialized inotify instance.
 * @Param argc  The number of files to watch.
 * @Param argv  The list of filepaths to watch.
 * @Param wds  A table associating a unique inotify watch descriptor to the
 * filepath of each file watched.
 */
void add_watchers(int inotify_fd, int argc, char *argv[],
                  unsigned int event_mask = IN_MODIFY | IN_CREATE) {
  for (size_t i = 1; i < argc; ++i) {
    int wd = inotify_add_watch(inotify_fd, argv[i], event_mask);
    if (wd < 0) {
      fmt::print(
          "Error while adding watcher for filepath {0} with event mask {1}",
          argv[i], event_mask);
      perror(" ");
      continue;
    }
    auto [it, inserted] = wd_2_name.insert(std::make_pair(wd, argv[i]));
    if (inserted) {
      fmt::print("\n\nAdded watcher for filepath {0} with event mask {1}\n", argv[i],
                 EVENT_MASK);
    }
  }
  size_t count = wd_2_name.size();
  fmt::print("\n\nWatching {0} files:\n", count);
  for (const auto& [wd, name] : wd_2_name) {
      fmt::print("  {0}\n", name);
  }
}

/**
 * Initialize inotify. A file descriptor is then provided, which
 * provides access to queries once watchers are added.
 */
inline int open_inotify_fd() noexcept {
  int fd = inotify_init();
  if (fd < 0) {
    perror("inotify_init() = ");
  }
  return fd;
}

/**
 * An #fd_set structure is like an array of file
 * descriptor status.
 *
 * The function #select allows to **multiplex** I/O
 * operations using fd_set structures:
 * At input, the pointer to #fd_set structure specifies
 * which file descriptors to check.
 * Then at output, that fd_set structure contains the information
 * of which file descriptor is ~ready to be operated on~.
 *
 * In our case, we take ~ready to be operated on~ as meaning that
 * a call to read() on the file descriptor will not block.
 */
bool events_ready(int inotify_fd) {
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(inotify_fd, &rfds);
  int res = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
  if (res < 0) {
    perror("select() = ");
  }
  return res > 0;
};

std::string_view eventmask_2_string(unsigned int mask) {
  static constexpr const char *in_modify_s = "IN_MODIFY";
  static constexpr const char *in_delete_s = "IN_DELETE";
  static constexpr const char *default_s = "";
  switch (mask) {
  case IN_MODIFY:
    return in_modify_s;
  case IN_DELETE:
    return in_delete_s;
  default:
    fmt::print("\n\n  Error: eventmask_2_string() is unimplemented for mask {0}\n",
               mask);
    return default_s;
  }
}

/**
 * From a pointer to the memory buffer storing the data of a inotify_event object,
 * extract the inotify watch descriptor and the corresponding filepath of the
 * watched file.
 *
 * @Return  the number of bytes the origin inotify_event object occupies.
 */
int extract_entry_event(char* buffer, size_t buf_len, queue_entry_event& event) {
    inotify_event* pevent = reinterpret_cast<inotify_event*>(buffer);

    // Copy the watch descriptor
    const int wd = event.wd = pevent->wd;
    // Copy the event type
    std::string_view event_type_s = eventmask_2_string(pevent->mask);
    event_type_s.copy(&event.type[0], event_type_s.size());
    // Copy the filepath
    std::string_view name = wd_2_name[wd];
    name.copy(&event.name[0], name.size());

    // Compute the inotify_event object's size.
    static const size_t event_size = offsetof(struct inotify_event, name);
    size_t name_len = std::distance(&pevent->name[0],
                                    std::find(&pevent->name[0], &pevent->name[NAME_MAX], '\0'));

    return event_size + name_len;
}

int get_events(int inotify_fd, char* buf, size_t buf_len, std::vector<queue_entry_event>& event_entries) {
    /// Read r events.
    int r = read(inotify_fd, buf, buf_len);
    if (r < 0) {
        fmt::print("\n\n  Error while reading from inotify_fd\n");
        perror("read");
        return r;
    }

    const size_t old_size = event_entries.size();
    size_t size = 0;

    fmt::print("Read {0} characters into the buffer.\n", r);

    /// Add an entry to our list for each event read.
    while (size < r) {
        auto& entry = event_entries.emplace_back();
        const size_t event_size = extract_entry_event(buf, buf_len, entry);
        size += event_size;

        fmt::print("Event Type: {0}, Filepath: {1}\n", entry.type, entry.name);
        fmt::print("Size of event: {0}\n", event_size);
    }
    const size_t count = event_entries.size() - old_size;

    fmt::print("\n\n\nRead {0} characters and extracted {1} events\n", r, count);
    return count;
}

void handle_dirty(bool* is_dirty) {
    if (*is_dirty) {
        fmt::print("\n\n  Handling a dirty flag!\n");
        *is_dirty = false;
    }
}

int main(int argc, char *argv[]) {

  /// Get the filepaths that need to be watched.
  constexpr size_t MAX_FILES_WATCHED = FD_SETSIZE;
  if (argc > MAX_FILES_WATCHED) {
    fmt::print(
        "\n\n   Error: Number of files to watch should not exceed {0}\n  (was {1})\n",
        MAX_FILES_WATCHED, argc);
    return EXIT_FAILURE;
  }

  /// Get the inotify file descriptor.
  int inotify_fd = open_inotify_fd();
  if (not inotify_fd) {
    fmt::print("\n\n  Error: Failed to initialize inotify.\n");
    return EXIT_FAILURE;
  }

  /// Add the watchers.
  add_watchers(inotify_fd, argc, argv, EVENT_MASK);
  if (wd_2_name.empty()) {
    fmt::print("\n\n  No watcher has succesfully been registered.\n");
    return EXIT_FAILURE;
  }

  /// Wait indefinitely, only waking if a watcher reports
  /// occurrence of a registered events of if a signal
  /// interrupt is caught.
  bool should_continue = true;
  std::vector<queue_entry_event> events;
  events.reserve(wd_2_name.size());

  char event_buffer[BUF_LEN];

  while (should_continue && not wd_2_name.empty()) {

    events.clear();
    bool dirty_flag = false;

    // When the events are ready to be collected, read
    // the inotify file descriptor and record which events
    // occured.
    if (events_ready(inotify_fd)) {
      int events_length = get_events(inotify_fd, event_buffer, BUF_LEN, events);
      if (events_length > 0) {
          dirty_flag = true;
      }

      handle_dirty(&dirty_flag);
    }
  }
  /// Finish up any queued task and dispose of everything.
  fmt::print("\n  Terminating...");

  return EXIT_SUCCESS;
}
