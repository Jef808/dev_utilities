#ifndef FS_WATCHER_H_
#define FS_WATCHER_H_

#include <sys/inotify.h>

#include <array>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

class Watcher;

/* Upper bound for the size of the name field of an event. */
static constexpr auto NAME_MAX = 1024;
/* Upper bound for the size of the event type names. */
static constexpr auto EVENT_TYPE_MAX = 10;
/* Structure to help representing events as strings. */
struct event_entry_t {
  int wd;
  std::string_view type;
  std::string_view name;
};

class EventsHandler {

public:
  EventsHandler() = default;

  /**
   * Handle a sequence of file modification events.
   */
  bool operator()(Watcher &watcher);

  void set_process_s(const char *process) { m_process_s = process; }

private:
  friend class Watcher;

  const char *m_process_s;
  std::vector<event_entry_t> m_events;
  int count{0};
};

class Watcher {

  /* Size of a inotify_event structure. */
  //static constexpr auto EVENT_SIZE = sizeof(struct inotify_event);
  static const auto EVENT_SIZE = offsetof(struct inotify_event, name);
  /* The size of the entry structures we store. */
  static const auto EVENT_ENTRY_SIZE = sizeof(struct event_entry_t);
  /* Lower bound for the total size of an inotify_event instance. */
  static constexpr auto BUF_LEN = EVENT_SIZE + NAME_MAX + 1;
  /* We will only track file modification events. */
  static constexpr auto EVENT_MASK = IN_MODIFY;

public:
  /**
   * Initialize inotify. A file descriptor is then provided, which
   * provides access to queries once watchers are added.
   *
   * @Return  The inotify file descriptor.
   */
  Watcher();

  /**
   * Main loop
   */
  void start(size_t n_files, const char *filepaths[], const char *process_s,
             unsigned int event_mask);

  /**
   * Add watchers for all filenames in the argv array.
   *
   * @Param inotify_fd  The file descriptor of an initialized inotify instance.
   * @Param argc  The number of files to watch.
   * @Param argv  The list of filepaths to watch.
   * @Param wds  A table associating a unique inotify watch descriptor to the
   * filepath of each file watched.
   * @Param event_mask  Mask to control which types of events will be reported.
   * See the `notify.h` header for the possible list of flags.
   */
  void add_watchers(size_t n_files, const char *filepaths[],
                    unsigned int event_mask);

  /**
   * Check if the inotify file descriptor is ready to be read
   * without blocking.
   *
   * See the man pages for #pselect() and #fd_set.
   */
  void check_for_events_ready() const;

  /**
   * Read from the inotify file descriptor and store
   * the events found in the #m_events container.
   */
  void get_ready_events();

  void extract_entry_event(char *buffer, size_t buf_len, event_entry_t &event);

  /**
   * Get the filepath associated to an event.
   */
  std::string_view get_filepath(const inotify_event &event);

  //void set_inactive(std::string_view fp);

private:
  enum Status {
    Uninitialized = 0,
    Initialized = 1,
    Running = 2,
    EventsReadyToRead = 4,
  };

  friend class EventsHandler;

  /* inotify file descriptor */
  int m_ifd;
  /* Enum flags to keep track of the watcher's status. */
  mutable Status m_status{Status::Uninitialized};
  /* number of watchers currently working */
  size_t n_watchers{0};

  /* list of unprocessed events. */
  std::vector<event_entry_t> m_events;
  /* watch descriptor to filepath map */
  std::unordered_map<int, std::string> m_wd2name;
};


#endif // FS_WATCHER_H_
