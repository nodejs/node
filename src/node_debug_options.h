#ifndef SRC_NODE_DEBUG_OPTIONS_H_
#define SRC_NODE_DEBUG_OPTIONS_H_

#include <string>

// Forward declaration to break recursive dependency chain with src/env.h.
namespace node {

class DebugOptions {
 public:
  DebugOptions();
  bool ParseOption(const char* argv0, const std::string& option);
  bool inspector_enabled() const { return inspector_enabled_; }
  bool deprecated_invocation() const {
    return deprecated_debug_ &&
      inspector_enabled_ &&
      break_first_line_;
  }
  bool invalid_invocation() const {
    return deprecated_debug_ && !inspector_enabled_;
  }
  bool wait_for_connect() const { return break_first_line_; }
  std::string host_name() const { return host_name_; }
  void set_host_name(std::string host_name) { host_name_ = host_name; }
  int port() const;
  void set_port(int port) { port_ = port; }

 private:
  bool inspector_enabled_;
  bool deprecated_debug_;
  bool break_first_line_;
  std::string host_name_;
  int port_;
};

}  // namespace node

#endif  // SRC_NODE_DEBUG_OPTIONS_H_
