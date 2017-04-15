// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_INTERPRETER_H_
#define V8_WASM_INTERPRETER_H_

#include "src/wasm/wasm-opcodes.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace base {
class AccountingAllocator;
}

namespace internal {
namespace wasm {

// forward declarations.
struct ModuleBytesEnv;
struct WasmFunction;
class WasmInterpreterInternals;

typedef size_t pc_t;
typedef size_t sp_t;
typedef int32_t pcdiff_t;
typedef uint32_t spdiff_t;

const pc_t kInvalidPc = 0x80000000;

typedef ZoneMap<pc_t, pcdiff_t> ControlTransferMap;

// Macro for defining union members.
#define FOREACH_UNION_MEMBER(V) \
  V(i32, kWasmI32, int32_t)     \
  V(u32, kWasmI32, uint32_t)    \
  V(i64, kWasmI64, int64_t)     \
  V(u64, kWasmI64, uint64_t)    \
  V(f32, kWasmF32, float)       \
  V(f64, kWasmF64, double)

// Representation of values within the interpreter.
struct WasmVal {
  ValueType type;
  union {
#define DECLARE_FIELD(field, localtype, ctype) ctype field;
    FOREACH_UNION_MEMBER(DECLARE_FIELD)
#undef DECLARE_FIELD
  } val;

  WasmVal() : type(kWasmStmt) {}

#define DECLARE_CONSTRUCTOR(field, localtype, ctype) \
  explicit WasmVal(ctype v) : type(localtype) { val.field = v; }
  FOREACH_UNION_MEMBER(DECLARE_CONSTRUCTOR)
#undef DECLARE_CONSTRUCTOR

  template <typename T>
  inline T to() {
    UNREACHABLE();
  }

  template <typename T>
  inline T to_unchecked() {
    UNREACHABLE();
  }
};

#define DECLARE_CAST(field, localtype, ctype) \
  template <>                                 \
  inline ctype WasmVal::to_unchecked() {      \
    return val.field;                         \
  }                                           \
  template <>                                 \
  inline ctype WasmVal::to() {                \
    CHECK_EQ(localtype, type);                \
    return val.field;                         \
  }
FOREACH_UNION_MEMBER(DECLARE_CAST)
#undef DECLARE_CAST

// Representation of frames within the interpreter.
class WasmFrame {
 public:
  const WasmFunction* function() const { return function_; }
  int pc() const { return pc_; }

 private:
  friend class WasmInterpreter;

  WasmFrame(const WasmFunction* function, int pc, int fp, int sp)
      : function_(function), pc_(pc), fp_(fp), sp_(sp) {}

  const WasmFunction* function_;
  int pc_;
  int fp_;
  int sp_;
};

// An interpreter capable of executing WASM.
class V8_EXPORT_PRIVATE WasmInterpreter {
 public:
  // State machine for a Thread:
  //                       +---------------Run()-----------+
  //                       V                               |
  // STOPPED ---Run()-->  RUNNING  ------Pause()-----+-> PAUSED  <------+
  //                       | | |                    /      |            |
  //                       | | +---- Breakpoint ---+       +-- Step() --+
  //                       | |
  //                       | +------------ Trap --------------> TRAPPED
  //                       +------------- Finish -------------> FINISHED
  enum State { STOPPED, RUNNING, PAUSED, FINISHED, TRAPPED };

  // Representation of a thread in the interpreter.
  class Thread {
   public:
    // Execution control.
    virtual State state() = 0;
    virtual void PushFrame(const WasmFunction* function, WasmVal* args) = 0;
    virtual State Run() = 0;
    virtual State Step() = 0;
    virtual void Pause() = 0;
    virtual void Reset() = 0;
    virtual ~Thread() {}

    // Stack inspection and modification.
    virtual pc_t GetBreakpointPc() = 0;
    virtual int GetFrameCount() = 0;
    virtual const WasmFrame* GetFrame(int index) = 0;
    virtual WasmFrame* GetMutableFrame(int index) = 0;
    virtual WasmVal GetReturnValue(int index = 0) = 0;
    // Returns true if the thread executed an instruction which may produce
    // nondeterministic results, e.g. float div, float sqrt, and float mul,
    // where the sign bit of a NaN is nondeterministic.
    virtual bool PossibleNondeterminism() = 0;

    // Thread-specific breakpoints.
    bool SetBreakpoint(const WasmFunction* function, int pc, bool enabled);
    bool GetBreakpoint(const WasmFunction* function, int pc);
  };

  WasmInterpreter(const ModuleBytesEnv& env, AccountingAllocator* allocator);
  ~WasmInterpreter();

  //==========================================================================
  // Execution controls.
  //==========================================================================
  void Run();
  void Pause();

  // Set a breakpoint at {pc} in {function} to be {enabled}. Returns the
  // previous state of the breakpoint at {pc}.
  bool SetBreakpoint(const WasmFunction* function, pc_t pc, bool enabled);

  // Gets the current state of the breakpoint at {function}.
  bool GetBreakpoint(const WasmFunction* function, pc_t pc);

  // Enable or disable tracing for {function}. Return the previous state.
  bool SetTracing(const WasmFunction* function, bool enabled);

  //==========================================================================
  // Thread iteration and inspection.
  //==========================================================================
  int GetThreadCount();
  Thread* GetThread(int id);

  //==========================================================================
  // Stack frame inspection.
  //==========================================================================
  WasmVal GetLocalVal(const WasmFrame* frame, int index);
  WasmVal GetExprVal(const WasmFrame* frame, int pc);
  void SetLocalVal(WasmFrame* frame, int index, WasmVal val);
  void SetExprVal(WasmFrame* frame, int pc, WasmVal val);

  //==========================================================================
  // Memory access.
  //==========================================================================
  size_t GetMemorySize();
  WasmVal ReadMemory(size_t offset);
  void WriteMemory(size_t offset, WasmVal val);

  //==========================================================================
  // Testing functionality.
  //==========================================================================
  // Manually adds a function to this interpreter, returning the index of the
  // function.
  int AddFunctionForTesting(const WasmFunction* function);
  // Manually adds code to the interpreter for the given function.
  bool SetFunctionCodeForTesting(const WasmFunction* function,
                                 const byte* start, const byte* end);

  // Computes the control transfers for the given bytecode. Used internally in
  // the interpreter, but exposed for testing.
  static ControlTransferMap ComputeControlTransfersForTesting(Zone* zone,
                                                              const byte* start,
                                                              const byte* end);

 private:
  Zone zone_;
  WasmInterpreterInternals* internals_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_INTERPRETER_H_
