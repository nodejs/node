// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_INTERPRETER_H_
#define V8_WASM_WASM_INTERPRETER_H_

#include <memory>

#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-value.h"
#include "src/zone/zone-containers.h"

namespace v8 {

namespace internal {
class WasmInstanceObject;

namespace wasm {

// Forward declarations.
struct ModuleWireBytes;
struct WasmFunction;
struct WasmModule;
class WasmInterpreterInternals;

using pc_t = size_t;
using sp_t = size_t;
using pcdiff_t = int32_t;
using spdiff_t = uint32_t;

struct ControlTransferEntry {
  // Distance from the instruction to the label to jump to (forward, but can be
  // negative).
  pcdiff_t pc_diff;
  // Delta by which to decrease the stack height.
  spdiff_t sp_diff;
  // Arity of the block we jump to.
  uint32_t target_arity;
};

struct CatchControlTransferEntry : public ControlTransferEntry {
  int exception_index;
  int target_control_index;
};

struct ControlTransferMap {
  explicit ControlTransferMap(Zone* zone) : map(zone), catch_map(zone) {}

  ZoneMap<pc_t, ControlTransferEntry> map;
  ZoneMap<pc_t, ZoneVector<CatchControlTransferEntry>> catch_map;
};

// An interpreter capable of executing WebAssembly.
class WasmInterpreter {
 public:
  WasmInterpreter(const WasmInterpreter&) = delete;
  WasmInterpreter& operator=(const WasmInterpreter&) = delete;

  // State machine for the interpreter:
  //    +----------------------------------------------------------+
  //    |                    +--------Run()/Step()---------+       |
  //    V                    V                             |       |
  // STOPPED ---Run()-->  RUNNING  ------Pause()-----+-> PAUSED <--+
  //    ^                 | | | |                   /
  //    +--- Exception ---+ | | +--- Breakpoint ---+
  //                        | +---------- Trap --------------> TRAPPED
  //                        +----------- Finish -------------> FINISHED
  enum State { STOPPED, RUNNING, PAUSED, FINISHED, TRAPPED };

  enum ExceptionHandlingResult { HANDLED, UNWOUND };

  WasmInterpreter(Isolate* isolate, const WasmModule* module,
                  const ModuleWireBytes& wire_bytes,
                  Handle<WasmInstanceObject> instance);

  ~WasmInterpreter();

  //==========================================================================
  // Execution controls.
  //==========================================================================
  State state() const;
  void InitFrame(const WasmFunction* function, WasmValue* args);
  // Pass -1 as num_steps to run till completion, pause or breakpoint.
  State Run(int num_steps = -1);
  State Step() { return Run(1); }
  void Pause();
  void Reset();

  // Stack inspection and modification.
  WasmValue GetReturnValue(int index = 0) const;
  TrapReason GetTrapReason() const;

  // Returns true if the thread executed an instruction which may produce
  // nondeterministic results, e.g. float div, float sqrt, and float mul,
  // where the sign bit of a NaN is nondeterministic.
  bool PossibleNondeterminism() const;

  // Returns the number of calls / function frames executed on this thread.
  uint64_t NumInterpretedCalls() const;

  //==========================================================================
  // Testing functionality.
  //==========================================================================
  // Manually adds a function to this interpreter. The func_index of the
  // function must match the current number of functions.
  void AddFunctionForTesting(const WasmFunction* function);
  // Manually adds code to the interpreter for the given function.
  void SetFunctionCodeForTesting(const WasmFunction* function,
                                 const byte* start, const byte* end);

  // Computes the control transfers for the given bytecode. Used internally in
  // the interpreter, but exposed for testing.
  static ControlTransferMap ComputeControlTransfersForTesting(
      Zone* zone, const WasmModule* module, const byte* start, const byte* end);

 private:
  Zone zone_;
  std::unique_ptr<WasmInterpreterInternals> internals_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_INTERPRETER_H_
