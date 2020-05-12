// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/assembler-inl.h"
#include "src/debug/debug-interface.h"
#include "src/execution/frames-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/utils/utils.h"
#include "src/wasm/wasm-objects-inl.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

debug::Location TranslateLocation(WasmRunnerBase* runner,
                                  const debug::Location& loc) {
  // Convert locations from {func_index, offset_in_func} to
  // {0, offset_in_module}.
  int func_index = loc.GetLineNumber();
  int func_offset = runner->builder().GetFunctionAt(func_index)->code.offset();
  int offset = loc.GetColumnNumber() + func_offset;
  return {0, offset};
}

void CheckLocations(
    WasmRunnerBase* runner, NativeModule* native_module, debug::Location start,
    debug::Location end,
    std::initializer_list<debug::Location> expected_locations_init) {
  std::vector<debug::BreakLocation> locations;
  std::vector<debug::Location> expected_locations;
  for (auto loc : expected_locations_init) {
    expected_locations.push_back(TranslateLocation(runner, loc));
  }

  bool success = WasmScript::GetPossibleBreakpoints(
      native_module, TranslateLocation(runner, start),
      TranslateLocation(runner, end), &locations);
  CHECK(success);

  printf("got %d locations: ", static_cast<int>(locations.size()));
  for (size_t i = 0, e = locations.size(); i != e; ++i) {
    printf("%s<%d,%d>", i == 0 ? "" : ", ", locations[i].GetLineNumber(),
           locations[i].GetColumnNumber());
  }
  printf("\n");

  CHECK_EQ(expected_locations.size(), locations.size());
  for (size_t i = 0, e = locations.size(); i != e; ++i) {
    CHECK_EQ(expected_locations[i].GetLineNumber(),
             locations[i].GetLineNumber());
    CHECK_EQ(expected_locations[i].GetColumnNumber(),
             locations[i].GetColumnNumber());
  }
}

void CheckLocationsFail(WasmRunnerBase* runner, NativeModule* native_module,
                        debug::Location start, debug::Location end) {
  std::vector<debug::BreakLocation> locations;
  bool success = WasmScript::GetPossibleBreakpoints(
      native_module, TranslateLocation(runner, start),
      TranslateLocation(runner, end), &locations);
  CHECK(!success);
}

class BreakHandler : public debug::DebugDelegate {
 public:
  enum Action {
    Continue = StepAction::LastStepAction + 1,
    StepNext = StepAction::StepNext,
    StepIn = StepAction::StepIn,
    StepOut = StepAction::StepOut
  };
  struct BreakPoint {
    int position;
    Action action;
    std::function<void(void)> pre_action;
    BreakPoint(int position, Action action)
        : position(position), action(action), pre_action([]() {}) {}
    BreakPoint(int position, Action action,
               std::function<void(void)> pre_action)
        : position(position), action(action), pre_action(pre_action) {}
  };

  explicit BreakHandler(Isolate* isolate,
                        std::initializer_list<BreakPoint> expected_breaks)
      : isolate_(isolate), expected_breaks_(expected_breaks) {
    v8::debug::SetDebugDelegate(reinterpret_cast<v8::Isolate*>(isolate_), this);
  }
  ~BreakHandler() override {
    // Check that all expected breakpoints have been hit.
    CHECK_EQ(count_, expected_breaks_.size());
    v8::debug::SetDebugDelegate(reinterpret_cast<v8::Isolate*>(isolate_),
                                nullptr);
  }

  int count() const { return count_; }

 private:
  Isolate* isolate_;
  int count_ = 0;
  std::vector<BreakPoint> expected_breaks_;

  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             const std::vector<int>&) override {
    printf("Break #%d\n", count_);
    CHECK_GT(expected_breaks_.size(), count_);

    // Check the current position.
    StackTraceFrameIterator frame_it(isolate_);
    auto summ = FrameSummary::GetTop(frame_it.frame()).AsWasmInterpreted();
    CHECK_EQ(expected_breaks_[count_].position, summ.byte_offset());

    expected_breaks_[count_].pre_action();
    Action next_action = expected_breaks_[count_].action;
    switch (next_action) {
      case Continue:
        break;
      case StepNext:
      case StepIn:
      case StepOut:
        isolate_->debug()->PrepareStep(static_cast<StepAction>(next_action));
        break;
      default:
        UNREACHABLE();
    }
    ++count_;
  }
};

Handle<BreakPoint> SetBreakpoint(WasmRunnerBase* runner, int function_index,
                                 int byte_offset,
                                 int expected_set_byte_offset = -1) {
  runner->TierDown();
  int func_offset =
      runner->builder().GetFunctionAt(function_index)->code.offset();
  int code_offset = func_offset + byte_offset;
  if (expected_set_byte_offset == -1) expected_set_byte_offset = byte_offset;
  Handle<WasmInstanceObject> instance = runner->builder().instance_object();
  Handle<Script> script(instance->module_object().script(),
                        runner->main_isolate());
  static int break_index = 0;
  Handle<BreakPoint> break_point =
      runner->main_isolate()->factory()->NewBreakPoint(
          break_index++, runner->main_isolate()->factory()->empty_string());
  CHECK(WasmScript::SetBreakPoint(script, &code_offset, break_point));
  int set_byte_offset = code_offset - func_offset;
  CHECK_EQ(expected_set_byte_offset, set_byte_offset);
  // Also set breakpoint on the debug info of the instance directly, since the
  // instance chain is not setup properly in tests.
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);
  WasmDebugInfo::SetBreakpoint(debug_info, function_index, set_byte_offset);

  return break_point;
}

void ClearBreakpoint(WasmRunnerBase* runner, int function_index,
                     int byte_offset, Handle<BreakPoint> break_point) {
  int func_offset =
      runner->builder().GetFunctionAt(function_index)->code.offset();
  int code_offset = func_offset + byte_offset;
  Handle<WasmInstanceObject> instance = runner->builder().instance_object();
  Handle<Script> script(instance->module_object().script(),
                        runner->main_isolate());
  CHECK(WasmScript::ClearBreakPoint(script, code_offset, break_point));
  // Also clear breakpoint on the debug info of the instance directly, since the
  // instance chain is not setup properly in tests.
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);
  WasmDebugInfo::ClearBreakpoint(debug_info, function_index, byte_offset);
}

// Wrapper with operator<<.
struct WasmValWrapper {
  WasmValue val;

  bool operator==(const WasmValWrapper& other) const {
    return val == other.val;
  }
};

// Only needed in debug builds. Avoid unused warning otherwise.
#ifdef DEBUG
std::ostream& operator<<(std::ostream& out, const WasmValWrapper& wrapper) {
  switch (wrapper.val.type().kind()) {
    case ValueType::kI32:
      out << "i32: " << wrapper.val.to<int32_t>();
      break;
    case ValueType::kI64:
      out << "i64: " << wrapper.val.to<int64_t>();
      break;
    case ValueType::kF32:
      out << "f32: " << wrapper.val.to<float>();
      break;
    case ValueType::kF64:
      out << "f64: " << wrapper.val.to<double>();
      break;
    default:
      UNIMPLEMENTED();
  }
  return out;
}
#endif

class CollectValuesBreakHandler : public debug::DebugDelegate {
 public:
  struct BreakpointValues {
    std::vector<WasmValue> locals;
    std::vector<WasmValue> stack;
  };

  explicit CollectValuesBreakHandler(
      Isolate* isolate, std::initializer_list<BreakpointValues> expected_values)
      : isolate_(isolate), expected_values_(expected_values) {
    v8::debug::SetDebugDelegate(reinterpret_cast<v8::Isolate*>(isolate_), this);
  }
  ~CollectValuesBreakHandler() override {
    v8::debug::SetDebugDelegate(reinterpret_cast<v8::Isolate*>(isolate_),
                                nullptr);
  }

 private:
  Isolate* isolate_;
  int count_ = 0;
  std::vector<BreakpointValues> expected_values_;

  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             const std::vector<int>&) override {
    printf("Break #%d\n", count_);
    CHECK_GT(expected_values_.size(), count_);
    auto& expected = expected_values_[count_];
    ++count_;

    HandleScope handles(isolate_);

    StackTraceFrameIterator frame_it(isolate_);
    auto summ = FrameSummary::GetTop(frame_it.frame()).AsWasmInterpreted();
    Handle<WasmInstanceObject> instance = summ.wasm_instance();

    auto frame =
        instance->debug_info().GetInterpretedFrame(frame_it.frame()->fp(), 0);
    CHECK_EQ(expected.locals.size(), frame->GetLocalCount());
    for (int i = 0; i < frame->GetLocalCount(); ++i) {
      CHECK_EQ(WasmValWrapper{expected.locals[i]},
               WasmValWrapper{frame->GetLocalValue(i)});
    }

    CHECK_EQ(expected.stack.size(), frame->GetStackHeight());
    for (int i = 0; i < frame->GetStackHeight(); ++i) {
      CHECK_EQ(WasmValWrapper{expected.stack[i]},
               WasmValWrapper{frame->GetStackValue(i)});
    }

    isolate_->debug()->PrepareStep(StepAction::StepIn);
  }
};

// Special template to explicitly cast to WasmValue.
template <typename Arg>
WasmValue MakeWasmVal(Arg arg) {
  return WasmValue(arg);
}
// Translate long to i64 (ambiguous otherwise).
template <>
WasmValue MakeWasmVal(long arg) {  // NOLINT: allow long parameter
  return WasmValue(static_cast<int64_t>(arg));
}

template <typename... Args>
std::vector<WasmValue> wasmVec(Args... args) {
  std::array<WasmValue, sizeof...(args)> arr{{MakeWasmVal(args)...}};
  return std::vector<WasmValue>{arr.begin(), arr.end()};
}

int GetIntReturnValue(MaybeHandle<Object> retval) {
  CHECK(!retval.is_null());
  int result;
  CHECK(retval.ToHandleChecked()->ToInt32(&result));
  return result;
}

}  // namespace

WASM_COMPILED_EXEC_TEST(WasmCollectPossibleBreakpoints) {
  WasmRunner<int> runner(execution_tier);

  BUILD(runner, WASM_NOP, WASM_I32_ADD(WASM_ZERO, WASM_ONE));

  WasmInstanceObject instance = *runner.builder().instance_object();
  NativeModule* native_module = instance.module_object().native_module();

  std::vector<debug::Location> locations;
  // Check all locations for function 0.
  CheckLocations(&runner, native_module, {0, 0}, {0, 10},
                 {{0, 1}, {0, 2}, {0, 4}, {0, 6}, {0, 7}});
  // Check a range ending at an instruction.
  CheckLocations(&runner, native_module, {0, 2}, {0, 4}, {{0, 2}});
  // Check a range ending one behind an instruction.
  CheckLocations(&runner, native_module, {0, 2}, {0, 5}, {{0, 2}, {0, 4}});
  // Check a range starting at an instruction.
  CheckLocations(&runner, native_module, {0, 7}, {0, 8}, {{0, 7}});
  // Check from an instruction to beginning of next function.
  CheckLocations(&runner, native_module, {0, 7}, {0, 10}, {{0, 7}});
  // Check from end of one function (no valid instruction position) to beginning
  // of next function. Must be empty, but not fail.
  CheckLocations(&runner, native_module, {0, 8}, {0, 10}, {});
  // Check from one after the end of the function. Must fail.
  CheckLocationsFail(&runner, native_module, {0, 9}, {0, 10});
}

WASM_COMPILED_EXEC_TEST(WasmSimpleBreak) {
  WasmRunner<int> runner(execution_tier);
  Isolate* isolate = runner.main_isolate();

  BUILD(runner, WASM_NOP, WASM_I32_ADD(WASM_I32V_1(11), WASM_I32V_1(3)));

  Handle<JSFunction> main_fun_wrapper =
      runner.builder().WrapCode(runner.function_index());
  SetBreakpoint(&runner, runner.function_index(), 4, 4);

  BreakHandler count_breaks(isolate, {{4, BreakHandler::Continue}});

  Handle<Object> global(isolate->context().global_object(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, main_fun_wrapper, global, 0, nullptr);
  CHECK_EQ(14, GetIntReturnValue(retval));
}

WASM_COMPILED_EXEC_TEST(WasmNonBreakablePosition) {
  WasmRunner<int> runner(execution_tier);
  Isolate* isolate = runner.main_isolate();

  BUILD(runner, WASM_RETURN1(WASM_I32V_2(1024)));

  Handle<JSFunction> main_fun_wrapper =
      runner.builder().WrapCode(runner.function_index());
  SetBreakpoint(&runner, runner.function_index(), 2, 4);

  BreakHandler count_breaks(isolate, {{4, BreakHandler::Continue}});

  Handle<Object> global(isolate->context().global_object(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, main_fun_wrapper, global, 0, nullptr);
  CHECK_EQ(1024, GetIntReturnValue(retval));
}

WASM_COMPILED_EXEC_TEST(WasmSimpleStepping) {
  WasmRunner<int> runner(execution_tier);
  BUILD(runner, WASM_I32_ADD(WASM_I32V_1(11), WASM_I32V_1(3)));

  Isolate* isolate = runner.main_isolate();
  Handle<JSFunction> main_fun_wrapper =
      runner.builder().WrapCode(runner.function_index());

  // Set breakpoint at the first I32Const.
  SetBreakpoint(&runner, runner.function_index(), 1, 1);

  BreakHandler count_breaks(isolate,
                            {
                                {1, BreakHandler::StepNext},  // I32Const
                                {3, BreakHandler::StepNext},  // I32Const
                                {5, BreakHandler::Continue}   // I32Add
                            });

  Handle<Object> global(isolate->context().global_object(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, main_fun_wrapper, global, 0, nullptr);
  CHECK_EQ(14, GetIntReturnValue(retval));
}

WASM_COMPILED_EXEC_TEST(WasmStepInAndOut) {
  WasmRunner<int, int> runner(execution_tier);
  WasmFunctionCompiler& f2 = runner.NewFunction<void>();
  f2.AllocateLocal(kWasmI32);

  // Call f2 via indirect call, because a direct call requires f2 to exist when
  // we compile main, but we need to compile main first so that the order of
  // functions in the code section matches the function indexes.

  // return arg0
  BUILD(runner, WASM_RETURN1(WASM_GET_LOCAL(0)));
  // for (int i = 0; i < 10; ++i) { f2(i); }
  BUILD(f2, WASM_LOOP(
                WASM_BR_IF(0, WASM_BINOP(kExprI32GeU, WASM_GET_LOCAL(0),
                                         WASM_I32V_1(10))),
                WASM_SET_LOCAL(
                    0, WASM_BINOP(kExprI32Sub, WASM_GET_LOCAL(0), WASM_ONE)),
                WASM_CALL_FUNCTION(runner.function_index(), WASM_GET_LOCAL(0)),
                WASM_DROP, WASM_BR(1)));

  Isolate* isolate = runner.main_isolate();
  Handle<JSFunction> main_fun_wrapper =
      runner.builder().WrapCode(f2.function_index());

  // Set first breakpoint on the LocalGet (offset 19) before the Call.
  SetBreakpoint(&runner, f2.function_index(), 19, 19);

  BreakHandler count_breaks(isolate,
                            {
                                {19, BreakHandler::StepIn},   // LocalGet
                                {21, BreakHandler::StepIn},   // Call
                                {1, BreakHandler::StepOut},   // in f2
                                {23, BreakHandler::Continue}  // After Call
                            });

  Handle<Object> global(isolate->context().global_object(), isolate);
  CHECK(!Execution::Call(isolate, main_fun_wrapper, global, 0, nullptr)
             .is_null());
}

WASM_COMPILED_EXEC_TEST(WasmGetLocalsAndStack) {
  WasmRunner<void, int> runner(execution_tier);
  runner.AllocateLocal(kWasmI64);
  runner.AllocateLocal(kWasmF32);
  runner.AllocateLocal(kWasmF64);

  BUILD(runner,
        // set [1] to 17
        WASM_SET_LOCAL(1, WASM_I64V_1(17)),
        // set [2] to <arg0> = 7
        WASM_SET_LOCAL(2, WASM_F32_SCONVERT_I32(WASM_GET_LOCAL(0))),
        // set [3] to <arg1>/2 = 8.5
        WASM_SET_LOCAL(3, WASM_F64_DIV(WASM_F64_SCONVERT_I64(WASM_GET_LOCAL(1)),
                                       WASM_F64(2))));

  Isolate* isolate = runner.main_isolate();
  Handle<JSFunction> main_fun_wrapper =
      runner.builder().WrapCode(runner.function_index());

  // Set breakpoint at the first instruction (7 bytes for local decls: num
  // entries + 3x<count, type>).
  SetBreakpoint(&runner, runner.function_index(), 7, 7);

  CollectValuesBreakHandler break_handler(
      isolate,
      {
          // params + locals          stack
          {wasmVec(7, 0L, 0.f, 0.), wasmVec()},          // 0: i64.const[17]
          {wasmVec(7, 0L, 0.f, 0.), wasmVec(17L)},       // 1: set_local[1]
          {wasmVec(7, 17L, 0.f, 0.), wasmVec()},         // 2: get_local[0]
          {wasmVec(7, 17L, 0.f, 0.), wasmVec(7)},        // 3: f32.convert_s
          {wasmVec(7, 17L, 0.f, 0.), wasmVec(7.f)},      // 4: set_local[2]
          {wasmVec(7, 17L, 7.f, 0.), wasmVec()},         // 5: get_local[1]
          {wasmVec(7, 17L, 7.f, 0.), wasmVec(17L)},      // 6: f64.convert_s
          {wasmVec(7, 17L, 7.f, 0.), wasmVec(17.)},      // 7: f64.const[2]
          {wasmVec(7, 17L, 7.f, 0.), wasmVec(17., 2.)},  // 8: f64.div
          {wasmVec(7, 17L, 7.f, 0.), wasmVec(8.5)},      // 9: set_local[3]
          {wasmVec(7, 17L, 7.f, 8.5), wasmVec()},        // 10: end
      });

  Handle<Object> global(isolate->context().global_object(), isolate);
  Handle<Object> args[]{handle(Smi::FromInt(7), isolate)};
  CHECK(!Execution::Call(isolate, main_fun_wrapper, global, 1, args).is_null());
}

WASM_COMPILED_EXEC_TEST(WasmRemoveBreakPoint) {
  WasmRunner<int> runner(execution_tier);
  Isolate* isolate = runner.main_isolate();

  BUILD(runner, WASM_NOP, WASM_NOP, WASM_NOP, WASM_NOP, WASM_NOP,
        WASM_I32V_1(14));

  Handle<JSFunction> main_fun_wrapper =
      runner.builder().WrapCode(runner.function_index());

  SetBreakpoint(&runner, runner.function_index(), 1, 1);
  SetBreakpoint(&runner, runner.function_index(), 2, 2);
  Handle<BreakPoint> to_delete =
      SetBreakpoint(&runner, runner.function_index(), 3, 3);
  SetBreakpoint(&runner, runner.function_index(), 4, 4);

  BreakHandler count_breaks(isolate, {{1, BreakHandler::Continue},
                                      {2, BreakHandler::Continue,
                                       [&runner, &to_delete]() {
                                         ClearBreakpoint(
                                             &runner, runner.function_index(),
                                             3, to_delete);
                                       }},
                                      {4, BreakHandler::Continue}});

  Handle<Object> global(isolate->context().global_object(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, main_fun_wrapper, global, 0, nullptr);
  CHECK_EQ(14, GetIntReturnValue(retval));
}

WASM_COMPILED_EXEC_TEST(WasmRemoveLastBreakPoint) {
  WasmRunner<int> runner(execution_tier);
  Isolate* isolate = runner.main_isolate();

  BUILD(runner, WASM_NOP, WASM_NOP, WASM_NOP, WASM_NOP, WASM_NOP,
        WASM_I32V_1(14));

  Handle<JSFunction> main_fun_wrapper =
      runner.builder().WrapCode(runner.function_index());

  SetBreakpoint(&runner, runner.function_index(), 1, 1);
  SetBreakpoint(&runner, runner.function_index(), 2, 2);
  Handle<BreakPoint> to_delete =
      SetBreakpoint(&runner, runner.function_index(), 3, 3);

  BreakHandler count_breaks(
      isolate, {{1, BreakHandler::Continue},
                {2, BreakHandler::Continue, [&runner, &to_delete]() {
                   ClearBreakpoint(&runner, runner.function_index(), 3,
                                   to_delete);
                 }}});

  Handle<Object> global(isolate->context().global_object(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, main_fun_wrapper, global, 0, nullptr);
  CHECK_EQ(14, GetIntReturnValue(retval));
}

WASM_COMPILED_EXEC_TEST(WasmRemoveAllBreakPoint) {
  WasmRunner<int> runner(execution_tier);
  Isolate* isolate = runner.main_isolate();

  BUILD(runner, WASM_NOP, WASM_NOP, WASM_NOP, WASM_NOP, WASM_NOP,
        WASM_I32V_1(14));

  Handle<JSFunction> main_fun_wrapper =
      runner.builder().WrapCode(runner.function_index());

  Handle<BreakPoint> bp1 =
      SetBreakpoint(&runner, runner.function_index(), 1, 1);
  Handle<BreakPoint> bp2 =
      SetBreakpoint(&runner, runner.function_index(), 2, 2);
  Handle<BreakPoint> bp3 =
      SetBreakpoint(&runner, runner.function_index(), 3, 3);

  BreakHandler count_breaks(
      isolate, {{1, BreakHandler::Continue, [&runner, &bp1, &bp2, &bp3]() {
                   ClearBreakpoint(&runner, runner.function_index(), 1, bp1);
                   ClearBreakpoint(&runner, runner.function_index(), 3, bp3);
                   ClearBreakpoint(&runner, runner.function_index(), 2, bp2);
                 }}});

  Handle<Object> global(isolate->context().global_object(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, main_fun_wrapper, global, 0, nullptr);
  CHECK_EQ(14, GetIntReturnValue(retval));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
