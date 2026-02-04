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
  kSparkplugFrame = 1,
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
               Handle<BytecodeArray> bytecode_array,
               Handle<Object> accumulator);

  // We need to clean files and caches.
  void PrepareForNextREPRLCycle();

 private:
  bool AnyDumplingFlagsSet() const;

  std::string GetDumpOutFilename() const;

  std::string GetDumpPositionsFilename() const;

  template <typename T>
  std::optional<std::string> DumpValuePlain(T value, T& last_value);

  template <typename T>
  std::optional<std::string> DumpValue(T value, T& last_value);

  std::optional<std::string> DumpBytecodeOffset(int bytecode_offset);

  std::optional<std::string> DumpFunctionId(int function_id);

  std::optional<std::string> DumpArgCount(int arg_count);

  std::optional<std::string> DumpRegCount(int reg_count);

  std::optional<std::string> DumpArg(unsigned int index, std::string arg);

  std::optional<std::string> DumpReg(unsigned int index, std::string reg);

  std::optional<std::string> DumpAcc(std::string acc);

  void RecordDumpPosition(int function_id, int bytecode_offset);

  // We write dump positions to file on dumpling manager destruction because we
  // deduplicate dump positions in a map to not spam the file contents with
  // many dupes.
  void WriteDumpPositionsToFile();

  void LoadDumpPositionsFromFile();

  void ResetLastFrame();

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

  DumplingLastFrame dumpling_last_frame_;

  std::ofstream dumpling_os_;

  std::unordered_map<int, std::unordered_set<int> > dump_positions_;
};

}  // namespace v8::internal

#endif  // V8_DUMPLING_DUMPLING_MANAGER_H_
