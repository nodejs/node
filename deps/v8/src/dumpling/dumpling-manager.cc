// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/dumpling/dumpling-manager.h"

#include <fstream>

#include "src/dumpling/object-dumping.h"
#include "src/execution/isolate.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/bytecode-array.h"

namespace v8::internal {

namespace {

V8_INLINE void MaybePrint(std::string short_name,
                          std::optional<std::string> maybe_value,
                          std::ofstream& os) {
  if (maybe_value.has_value()) {
    os << short_name << maybe_value.value() << "\n";
  }
}

}  // namespace

void DumplingManager::DoPrint(UnoptimizedJSFrame* frame,
                              Tagged<JSFunction> function, int bytecode_offset,
                              DumpFrameType frame_dump_type,
                              Handle<Object> accumulator) {
  DCHECK(IsDumpingEnabled());

  switch (frame_dump_type) {
    case DumpFrameType::kInterpreterFrame:
      dumpling_os_ << "---I" << '\n';
      break;
    default:
      UNREACHABLE();
  }

  MaybePrint("b:", DumpBytecodeOffset(bytecode_offset), dumpling_os_);

  std::stringstream check_acc;
  DifferentialFuzzingPrint(*accumulator, dumpling_os_);
  MaybePrint("x:", DumpAcc(check_acc.str()), dumpling_os_);

  dumpling_os_ << "\n";
}

std::string DumplingManager::GetDumpOutFilename() const {
  return std::string(v8_flags.dump_out_filename);
}

template <typename T>
std::optional<std::string> DumplingManager::DumpValue(T value, T& last_value) {
  if (value == last_value) {
    return {};
  }
  last_value = value;
  return std::to_string(value);
}

std::optional<std::string> DumplingManager::DumpAcc(std::string acc) {
  if (acc == dumpling_last_frame_.acc) {
    return {};
  }
  dumpling_last_frame_.acc = acc;
  return acc;
}

std::optional<std::string> DumplingManager::DumpBytecodeOffset(
    int bytecode_offset) {
  return DumpValue(bytecode_offset, dumpling_last_frame_.bytecode_offset);
}

DumplingManager::DumplingManager()
    : dumpling_os_(GetDumpOutFilename(), std::ofstream::out) {}

DumplingManager::~DumplingManager() { dumpling_os_.close(); }

bool DumplingManager::AnyDumplingFlagsSet() const {
  return v8_flags.interpreter_dumping;
}

}  // namespace v8::internal
