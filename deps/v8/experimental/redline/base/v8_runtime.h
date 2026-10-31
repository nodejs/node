// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_BASE_V8_RUNTIME_H_
#define V8_EXPERIMENTAL_REDLINE_BASE_V8_RUNTIME_H_

#include <memory>
#include <string>
#include <string_view>

namespace v8 {
class Isolate;
class ArrayBuffer;
}  // namespace v8

namespace redline {

// Initializes the V8 engine linked into this binary (platform, ICU, startup
// snapshot). Idempotent; `flags` (e.g. "--allow-natives-syntax") are only
// applied by the first call because V8 freezes its flags afterwards.
// Returns false with a message if initialization fails.
bool InitializeV8(const std::string& flags, std::string* error);

// An isolate that only compiles scripts, to ask the real V8 parser whether
// it accepts a piece of source text. One instance per thread.
class SyntaxChecker {
 public:
  SyntaxChecker();
  ~SyntaxChecker();
  SyntaxChecker(const SyntaxChecker&) = delete;
  SyntaxChecker& operator=(const SyntaxChecker&) = delete;

  // Compiles `source` as a classic script (no execution). Returns true if V8
  // accepts it; otherwise `message` receives V8's SyntaxError message with
  // line and column.
  bool Accepts(std::string_view source, std::string* message);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_BASE_V8_RUNTIME_H_
