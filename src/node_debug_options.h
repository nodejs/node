#ifndef SRC_NODE_DEBUG_OPTIONS_H_
#define SRC_NODE_DEBUG_OPTIONS_H_

#include <string>

// Forward declaration to break recursive dependency chain with src/env.h.
namespace node {

enum class DebugAgentType {
  kNone,
  kDebugger,
#if HAVE_INSPECTOR
  kInspector
#endif  // HAVE_INSPECTOR
};

class DebugOptions {
 public:
  DebugOptions();
  bool ParseOption(const std::string& option);
  bool debugger_enabled() const {
    return debugger_enabled_ && !inspector_enabled();
  }
  bool inspector_enabled() const {
#if HAVE_INSPECTOR
    return inspector_enabled_;
#else
    return false;
#endif  // HAVE_INSPECTOR
  }
  void EnableDebugAgent(DebugAgentType type);
  bool ToolsServerEnabled();
  bool wait_for_connect() const { return wait_connect_; }
  std::string host_name() const { return host_name_; }
  int port() const;
  void set_port(int port) { port_ = port; }

 private:
  bool debugger_enabled_;
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
