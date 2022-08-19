#include "fs_watcher.h"

#include <errno.h>

#include <signal.h>
#include <stdio.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <algorithm>
#include <cassert>
#include <fmt/core.h>
#include <iostream>
#include <unordered_map>

#include <boost/process/system.hpp>

Watcher::Watcher() : m_ifd{inotify_init()} {
  if (m_ifd < 0) {
    perror("inotify_init() = ");
  }
  m_status = Initialized;
}

void Watcher::start(size_t n_files, const char *filepaths[],
                    const char *process_s, unsigned int event_mask) {

  if (m_status == Status::Uninitialized) {
    throw std::runtime_error("Watcher::start: status is Uninitialized");
  }

  EventsHandler events_handler;
  events_handler.set_process_s(process_s);

  add_watchers(n_files, filepaths, event_mask);

  while (n_watchers > 0) {
    check_for_events_ready();
    get_ready_events();
    fflush(stdout);
    int res = events_handler(*this);
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

void Watcher::extract_entry_event(char *buffer, size_t buf_len,
                                  event_entry_t &event) {
  inotify_event *pevent = reinterpret_cast<inotify_event *>(buffer);
  // Copy the watch descriptor
  event.wd = pevent->wd;
  // Get the string representation of the event type
  event.type = eventmask_2_string(pevent->mask);
  // Get the filepath of the file associated to the watch descriptor
  event.name = m_wd2name[event.wd];
}

void Watcher::add_watchers(size_t n_files, const char *filepaths[],
                           unsigned int event_mask) {
  size_t init_count = m_wd2name.size();

  for (size_t i = 0; i < n_files; ++i) {
    const char *fp = filepaths[i];

    int wd = inotify_add_watch(m_ifd, fp, event_mask);
    if (wd < 0) {
      perror("inotify_add_watch");
      fmt::print("\nWARNING: Failed to add watcher for file {0} with mask {1}",
                 filepaths[i], event_mask);
    }

    auto [it, inserted] =
        m_wd2name.insert(std::make_pair(wd, std::string(filepaths[i])));
    if (inserted) {
      ++n_watchers;
    } else if (m_wd2name.count(wd)) {
      fmt::print("\nWARNING: Watch descriptor {0} for path {1} is already "
                 "associated with path {2}\n",
                 wd, filepaths[i], it->second);
    }
  }

  size_t n_added = m_wd2name.size() - init_count;
  fmt::print("\n*************\nWatching {0} files:\n", n_watchers);
  for (const auto& [wd, fp] : m_wd2name) {
    fmt::print("  - {0}\n", fp);
  }
  fmt::print("************\n");
}

void Watcher::check_for_events_ready() const {
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(m_ifd, &rfds);
  int res = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
  if (res < 0) {
    perror("select() = ");
  }
  if (res > 0) {
    m_status = EventsReadyToRead;
  }
};

void Watcher::get_ready_events() {
  std::array<char, BUF_LEN> buffer;
  buffer.fill('\0');

  int r = read(m_ifd, &buffer[0], BUF_LEN);
  if (r < 0) {
    fmt::print("\nERROR: while reading from inotify_fd\n");
    perror("read");
  }

  size_t size = 0;
  size_t count = 0;

  // NOTE: Since we only watch regular files, the `name`
  // field of of all inotify_event instances does not
  // contribute and the events have constant size.
  int _count = 0;
  while (size < r) {
    auto &event = m_events.emplace_back();
    extract_entry_event(buffer.data(), BUF_LEN, event);
    size += EVENT_SIZE;
    ++_count;

    fmt::print("  New event... TYPE: {0}, FILE: {1}\n", event.type, event.name);
  }
}

std::string_view Watcher::get_filepath(const inotify_event &event) {
  return m_wd2name[event.wd];
}

bool EventsHandler::operator()(Watcher &watcher) {
  namespace bp = boost::process;

  m_events.clear();
  std::swap(m_events, watcher.m_events);

  bool res = true;

  for (const auto &event : m_events) {
    int result = bp::system(m_process_s);
    res &= (result >= 0);
  }

  return res;
}
