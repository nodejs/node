// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DUMPLING_DUMPLING_MANAGER_H_
#define V8_DUMPLING_DUMPLING_MANAGER_H_

#include <fstream>

#include "src/execution/frames.h"
#include "src/interpreter/bytecodes.h"

namespace v8::internal {

typedef enum DumpFrameType {
  kInterpreterFrame = 0,
} DumpFrameType;

class DumplingManager {
 public:
  DumplingManager();
  ~DumplingManager();

  void SetIsolateDumpDisabled() { isolate_dumping_disabled_ = true; }

  bool IsIsolateDumpDisabled() const { return isolate_dumping_disabled_; }

  bool IsDumpingEnabled() const {
    return !IsIsolateDumpDisabled() && AnyDumplingFlagsSet();
  }

  void DoPrint(UnoptimizedJSFrame* frame, Tagged<JSFunction> function,
               int bytecode_offset, DumpFrameType frame_dump_type,
               Handle<Object> accumulator);

 private:
  bool AnyDumplingFlagsSet() const;

  std::string GetDumpOutFilename() const;

  template <typename T>
  std::optional<std::string> DumpValue(T value, T& last_value);

  std::optional<std::string> DumpBytecodeOffset(int bytecode_offset);

  std::optional<std::string> DumpAcc(std::string acc);

  bool isolate_dumping_disabled_ = false;
  struct DumplingLastFrame {
    int bytecode_offset;
    std::string acc;
    int arg_count;
    std::vector<std::string> args;
    int reg_count;
    std::vector<std::string> regs;
    int function_id;
  };

  // initialize struct and reserve 64 elements in args
  DumplingLastFrame dumpling_last_frame_ = {-1, "invalid_acc",
                                            -1, std::vector<std::string>(64),
                                            -1, std::vector<std::string>(128),
                                            -1};

  std::ofstream dumpling_os_;
};

}  // namespace v8::internal

#endif  // V8_DUMPLING_DUMPLING_MANAGER_H_
