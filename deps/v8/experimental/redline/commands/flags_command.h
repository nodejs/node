// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_COMMANDS_FLAGS_COMMAND_H_
#define V8_EXPERIMENTAL_REDLINE_COMMANDS_FLAGS_COMMAND_H_

#include <string>
#include <vector>

namespace redline {

class Flags;

// Draws random `d8` flag lines for `redline flags` and the selftest.
bool DrawD8FlagLines(const Flags& flags, std::vector<std::string>* lines,
                     std::string* error);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_COMMANDS_FLAGS_COMMAND_H_
