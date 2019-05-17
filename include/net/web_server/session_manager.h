#pragma once

#include <set>

namespace net {

struct session {
  virtual ~session() = default;
  virtual void stop() = 0;
  bool is_websocket_{false};
};

struct session_manager {
  session_manager() = default;
  ~session_manager() = default;

  session_manager(session_manager&&) = delete;
  session_manager& operator=(session_manager&&) = delete;
  session_manager(session_manager const&) = delete;
  session_manager& operator=(session_manager const&) = delete;

  void add(session* s) { sessions_.emplace(s); }
  void remove(session* s) { sessions_.erase(s); }

  void shutdown() {
    for (auto const s : sessions_) {
      s->stop();
    }
  }

  std::set<session*> sessions_;
};

}  // namespace net