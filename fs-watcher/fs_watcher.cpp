#include "fs_watcher.h"

#include <errno.h>

#include <signal.h>
#include <stdio.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <iostream>
#include <algorithm>
#include <cassert>
#include <fmt/core.h>
#include <unordered_map>

#include <boost/process/system.hpp>

Watcher::Watcher() : m_ifd{inotify_init()} {
  if (m_ifd < 0) {
    perror("inotify_init() = ");
  }
}

// void init()

void Watcher::start(size_t n_files, const char *filepaths[],
                    const char* process_s,
                    unsigned int event_mask) {

  EventsHandler events_handler;
  events_handler.set_process_s(process_s);

  // if (m_status == Status::Uninitialized) {
  //     perror("starting uninitialized");
  //     return;
  // }
  add_watchers(n_files, filepaths, event_mask);

  while (n_watchers > 0) {
    check_for_events_ready();
    get_ready_events();

    fmt::print("\nList of events we read:\n");
    for (const auto& e : m_events) {
        fmt::print("{0}\n", e.name);
    }
    fmt::print("\n\n\nCalling events handler");

    int res = events_handler(*this);
    fmt::print("\nResult: {0}", res);
    fflush(stdout);
  }
}

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
    fmt::print(
        "\n\n  Error: eventmask_2_string() is unimplemented for mask {0}\n",
        mask);
    return default_s;
  }
}

int Watcher::extract_entry_event(char *buffer, size_t buf_len,
                                 event_entry_t &event) {
  inotify_event *pevent = reinterpret_cast<inotify_event *>(buffer);

  // Copy the watch descriptor
  const int wd = event.wd = pevent->wd;
  // Copy the event type
  std::string_view event_type_s = eventmask_2_string(pevent->mask);
  event_type_s.copy(&event.type[0], event_type_s.size());
  // Copy the filepath
  std::string_view name = m_wd2name[wd];
  name.copy(&event.name[0], name.size());

  // Compute the inotify_event object's size.
  static const size_t event_size = offsetof(struct inotify_event, name);
  size_t name_len =
      std::distance(&pevent->name[0],
                    std::find(&pevent->name[0], &pevent->name[NAME_MAX], '\0'));

  return event_size + name_len;
}

void Watcher::add_watchers(size_t n_files, const char *filepaths[],
                           unsigned int event_mask)
{
  for (size_t i = 0; i < n_files; ++i)
  {
    int wd = inotify_add_watch(m_ifd, filepaths[i], event_mask);
    if (wd < 0) {
      perror(" ");
      fmt::print("\nError when adding watcher for file {0}", filepaths[i]);
      m_status = Status::Uninitialized;
    }

    auto [it, inserted] = m_wd2name.insert(std::make_pair(wd, std::string(filepaths[i])));
    if (inserted) {
      fmt::print("\n\nAdded watcher for filepath {0} with event mask {1}\n",
                 filepaths[i], EVENT_MASK);
      ++n_watchers;
    }
  }
  size_t count = m_wd2name.size();

  fmt::print("\n\nWatching {0} files:\n", count);

  for (const auto &[wd, name] : m_wd2name) {
    fmt::print("  {0}\n", name);
  }
  if (n_watchers > 0) {
      m_status = Status::WatchersRegistered;
  }
}

bool Watcher::check_for_events_ready() const {
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(m_ifd, &rfds);
  int res = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
  if (res < 0) {
    perror("select() = ");
  }
  if (res > 0) {
    return true;
  }
  return false;
};

void Watcher::get_ready_events() {
  /// Read r events.
  std::array<char, BUF_LEN> buffer;
  buffer.fill('\0');

  fmt::print("\n\nBefore read\n\n");

  int r = read(m_ifd, &buffer[0], BUF_LEN);
  if (r < 0) {
    fmt::print("\n\n  Error while reading from inotify_fd\n");
    perror("read");
  }

  fmt::print("\nJust read {0} bytes from ifd\n", r);

  const size_t old_size = m_events.size();
  size_t size = 0;
  size_t count = 0;

  auto &event = m_events.emplace_back();

  extract_entry_event(buffer.data(), BUF_LEN, event);

  // fmt::print("Read {0} characters into the buffer.\n", r);
  // {
  //     inotify_event& entry = m_events.emplace_back();
  //     ++count;
  //     std::string_view name_sv = get_filepath(entry);
  //     std::string name;
  //     name_sv.copy(name.data(), name_sv.size());
  //     fmt::print("Event Type: {0}, Filepath: {1}\n", name);
  // }

  // Add an entry to our list for each event read.
  for (size_t s = 0; s < r; s +=EVENT_SIZE)
  {
    event_entry_t& entry = m_events.emplace_back();
    ++count;
    //std::string_view name_sv = entry.name;
    //std::string name;
    //name_sv.copy(name.data(), name_sv.size());
    fmt::print("Event Type: {0}, Filepath: {1}\n", entry.type, entry.name);
    fmt::print("Size of event: {0}\n", EVENT_SIZE);
  }

  fmt::print(
      "\n\n\nRead {0} characters and extracted {1} events: filepathis {2}\n\n",
      r, count, event.name);
}

void Watcher::clear_events() { m_events.clear(); }

std::string_view Watcher::get_filepath(const inotify_event &event) const {
  return "";
}

int EventsHandler::operator()(Watcher &watcher) {
  namespace bp = boost::process;
  static auto onModify = [&](const inotify_event &event) {
    fmt::print("Event {0}: FILE {1} MODIFIED\n", count++,
               watcher.get_filepath(event));
  };

  m_events.clear();
  std::swap(m_events, watcher.m_events);

  fmt::print("\n************\n  EventsHandler starting...\n************\n");

  std::for_each(m_events.begin(), m_events.end(), [](const auto &event) {
    fmt::print("Event: {0}\n", event.name);
  });

  fmt::print("\nCalling process {0}\n", m_process_s);
  int result = bp::system(m_process_s);
  return result;
}
