// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_LS_GLOBALS_H_
#define V8_TORQUE_LS_GLOBALS_H_

#include <fstream>
#include "src/torque/contextual.h"

namespace v8 {
namespace internal {
namespace torque {

// When the language server is run by VS code, stdout can not be seen, as it is
// used as the communication channel. For debugging purposes a simple
// Log class is added, that allows writing diagnostics to a file configurable
// via command line flag.
class Logger : public ContextualClass<Logger> {
 public:
  Logger() : enabled_(false) {}
  ~Logger() {
    if (enabled_) logfile_.close();
  }

  static void Enable(std::string path) {
    Get().enabled_ = true;
    Get().logfile_.open(path);
  }

  template <class... Args>
  static void Log(Args&&... args) {
    if (Enabled()) {
      USE((Stream() << std::forward<Args>(args))...);
      Flush();
    }
  }

 private:
  static bool Enabled() { return Get().enabled_; }
  static std::ofstream& Stream() {
    CHECK(Get().enabled_);
    return Get().logfile_;
  }
  static void Flush() { Get().logfile_.flush(); }

 private:
  bool enabled_;
  std::ofstream logfile_;
};

DECLARE_CONTEXTUAL_VARIABLE(TorqueFileList, std::vector<std::string>);

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_LS_GLOBALS_H_
