// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/deoptimization-data.h"

#include <iomanip>

#include "src/deoptimizer/translated-state.h"
#include "src/objects/code.h"
#include "src/objects/deoptimization-data-inl.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

Handle<DeoptimizationData> DeoptimizationData::New(Isolate* isolate,
                                                   int deopt_entry_count,
                                                   AllocationType allocation) {
  return Handle<DeoptimizationData>::cast(isolate->factory()->NewFixedArray(
      LengthFor(deopt_entry_count), allocation));
}

Handle<DeoptimizationData> DeoptimizationData::Empty(Isolate* isolate) {
  return Handle<DeoptimizationData>::cast(
      isolate->factory()->empty_fixed_array());
}

SharedFunctionInfo DeoptimizationData::GetInlinedFunction(int index) {
  if (index == -1) {
    return SharedFunctionInfo::cast(SharedFunctionInfo());
  } else {
    return SharedFunctionInfo::cast(LiteralArray().get(index));
  }
}

#ifdef ENABLE_DISASSEMBLER

namespace {
void print_pc(std::ostream& os, int pc) {
  if (pc == -1) {
    os << "NA";
  } else {
    os << std::hex << pc << std::dec;
  }
}
}  // namespace

void DeoptimizationData::DeoptimizationDataPrint(std::ostream& os) {
  if (length() == 0) {
    os << "Deoptimization Input Data invalidated by lazy deoptimization\n";
    return;
  }

  int const inlined_function_count = InlinedFunctionCount().value();
  os << "Inlined functions (count = " << inlined_function_count << ")\n";
  for (int id = 0; id < inlined_function_count; ++id) {
    Object info = LiteralArray().get(id);
    os << " " << Brief(SharedFunctionInfo::cast(info)) << "\n";
  }
  os << "\n";
  int deopt_count = DeoptCount();
  os << "Deoptimization Input Data (deopt points = " << deopt_count << ")\n";
  if (0 != deopt_count) {
#ifdef DEBUG
    os << " index  bytecode-offset  node-id    pc";
#else   // DEBUG
    os << " index  bytecode-offset    pc";
#endif  // DEBUG
    if (v8_flags.print_code_verbose) os << "  commands";
    os << "\n";
  }
  for (int i = 0; i < deopt_count; i++) {
    os << std::setw(6) << i << "  " << std::setw(15)
       << GetBytecodeOffset(i).ToInt() << "  "
#ifdef DEBUG
       << std::setw(7) << NodeId(i).value() << "  "
#endif  // DEBUG
       << std::setw(4);
    print_pc(os, Pc(i).value());
    os << std::setw(2);

    if (!v8_flags.print_code_verbose) {
      os << "\n";
      continue;
    }

    TranslationArrayPrintSingleFrame(os, TranslationByteArray(),
                                     TranslationIndex(i).value(),
                                     LiteralArray());
  }
}

#endif  // ENABLE_DISASSEMBLER

}  // namespace internal
}  // namespace v8
