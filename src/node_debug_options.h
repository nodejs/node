#ifndef SRC_NODE_DEBUG_OPTIONS_H_
#define SRC_NODE_DEBUG_OPTIONS_H_

#include <string>

// Forward declaration to break recursive dependency chain with src/env.h.
namespace node {

class DebugOptions {
 public:
  DebugOptions();
  bool ParseOption(const std::string& option);
  bool inspector_enabled() const {
#if HAVE_INSPECTOR
    return inspector_enabled_;
#else
    return false;
#endif  // HAVE_INSPECTOR
  }
  bool ToolsServerEnabled();
  bool wait_for_connect() const { return wait_connect_; }
  std::string host_name() const { return host_name_; }
  int port() const;
  void set_port(int port) { port_ = port; }

 private:
#if HAVE_INSPECTOR
  bool inspector_enabled_;
#endif  // HAVE_INSPECTOR
  bool wait_connect_;
  bool http_enabled_;
  std::string host_name_;
  int port_;
};

}  // namespace node

#endif  // SRC_NODE_DEBUG_OPTIONS_H_
