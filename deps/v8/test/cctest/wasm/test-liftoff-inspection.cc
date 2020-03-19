// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/wasm-debug.h"
#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

class LiftoffCompileEnvironment {
 public:
  LiftoffCompileEnvironment()
      : isolate_(CcTest::InitIsolateOnce()),
        handle_scope_(isolate_),
        zone_(isolate_->allocator(), ZONE_NAME),
        module_builder_(&zone_, nullptr, ExecutionTier::kLiftoff,
                        kRuntimeExceptionSupport, kNoLowerSimd) {
    // Add a table of length 1, for indirect calls.
    module_builder_.AddIndirectFunctionTable(nullptr, 1);
  }

  struct TestFunction {
    OwnedVector<uint8_t> body_bytes;
    WasmFunction* function;
    FunctionBody body;
  };

  void CheckDeterministicCompilation(
      std::initializer_list<ValueType> return_types,
      std::initializer_list<ValueType> param_types,
      std::initializer_list<uint8_t> raw_function_bytes) {
    auto test_func = AddFunction(return_types, param_types, raw_function_bytes);

    // Now compile the function with Liftoff two times.
    CompilationEnv env = module_builder_.CreateCompilationEnv();
    WasmFeatures detected1;
    WasmFeatures detected2;
    WasmCompilationResult result1 = ExecuteLiftoffCompilation(
        isolate_->allocator(), &env, test_func.body,
        test_func.function->func_index, isolate_->counters(), &detected1);
    WasmCompilationResult result2 = ExecuteLiftoffCompilation(
        isolate_->allocator(), &env, test_func.body,
        test_func.function->func_index, isolate_->counters(), &detected2);

    CHECK(result1.succeeded());
    CHECK(result2.succeeded());

    // Check that the generated code matches.
    auto code1 =
        VectorOf(result1.code_desc.buffer, result1.code_desc.instr_size);
    auto code2 =
        VectorOf(result2.code_desc.buffer, result2.code_desc.instr_size);
    CHECK_EQ(code1, code2);
    CHECK_EQ(detected1, detected2);
  }

  std::unique_ptr<DebugSideTable> GenerateDebugSideTable(
      std::initializer_list<ValueType> return_types,
      std::initializer_list<ValueType> param_types,
      std::initializer_list<uint8_t> raw_function_bytes,
      std::vector<int> breakpoints = {}) {
    auto test_func = AddFunction(return_types, param_types, raw_function_bytes);

    CompilationEnv env = module_builder_.CreateCompilationEnv(
        breakpoints.empty() ? TestingModuleBuilder::kNoDebug
                            : TestingModuleBuilder::kDebug);
    WasmFeatures detected;
    std::unique_ptr<DebugSideTable> debug_side_table_via_compilation;
    ExecuteLiftoffCompilation(
        CcTest::i_isolate()->allocator(), &env, test_func.body, 0, nullptr,
        &detected, VectorOf(breakpoints), &debug_side_table_via_compilation);

    // If there are no breakpoint, then {ExecuteLiftoffCompilation} should
    // provide the same debug side table.
    if (breakpoints.empty()) {
      std::unique_ptr<DebugSideTable> debug_side_table =
          GenerateLiftoffDebugSideTable(CcTest::i_isolate()->allocator(), &env,
                                        test_func.body);
      CheckTableEquals(*debug_side_table, *debug_side_table_via_compilation);
    }

    return debug_side_table_via_compilation;
  }

 private:
  static void CheckTableEquals(const DebugSideTable& a,
                               const DebugSideTable& b) {
    CHECK_EQ(a.num_locals(), b.num_locals());
    CHECK(std::equal(a.entries().begin(), a.entries().end(),
                     b.entries().begin(), b.entries().end(),
                     &CheckEntryEquals));
  }

  static bool CheckEntryEquals(const DebugSideTable::Entry& a,
                               const DebugSideTable::Entry& b) {
    CHECK_EQ(a.pc_offset(), b.pc_offset());
    CHECK(std::equal(a.values().begin(), a.values().end(), b.values().begin(),
                     b.values().end(), &CheckValueEquals));
    return true;
  }

  static bool CheckValueEquals(const DebugSideTable::Entry::Value& a,
                               const DebugSideTable::Entry::Value& b) {
    CHECK_EQ(a.type, b.type);
    CHECK_EQ(a.kind, b.kind);
    switch (a.kind) {
      case DebugSideTable::Entry::kConstant:
        CHECK_EQ(a.i32_const, b.i32_const);
        break;
      case DebugSideTable::Entry::kRegister:
        CHECK_EQ(a.reg_code, b.reg_code);
        break;
      case DebugSideTable::Entry::kStack:
        CHECK_EQ(a.stack_offset, b.stack_offset);
        break;
    }
    return true;
  }

  OwnedVector<uint8_t> GenerateFunctionBody(
      std::initializer_list<uint8_t> raw_function_bytes) {
    // Build the function bytes by prepending the locals decl and appending an
    // "end" opcode.
    OwnedVector<uint8_t> function_bytes =
        OwnedVector<uint8_t>::New(raw_function_bytes.size() + 2);
    function_bytes[0] = WASM_NO_LOCALS;
    std::copy(raw_function_bytes.begin(), raw_function_bytes.end(),
              &function_bytes[1]);
    function_bytes[raw_function_bytes.size() + 1] = WASM_END;
    return function_bytes;
  }

  FunctionSig* AddSig(std::initializer_list<ValueType> return_types,
                      std::initializer_list<ValueType> param_types) {
    ValueType* storage =
        zone_.NewArray<ValueType>(return_types.size() + param_types.size());
    std::copy(return_types.begin(), return_types.end(), storage);
    std::copy(param_types.begin(), param_types.end(),
              storage + return_types.size());
    FunctionSig* sig = new (&zone_)
        FunctionSig{return_types.size(), param_types.size(), storage};
    module_builder_.AddSignature(sig);
    return sig;
  }

  TestFunction AddFunction(std::initializer_list<ValueType> return_types,
                           std::initializer_list<ValueType> param_types,
                           std::initializer_list<uint8_t> raw_function_bytes) {
    OwnedVector<uint8_t> function_bytes =
        GenerateFunctionBody(raw_function_bytes);
    FunctionSig* sig = AddSig(return_types, param_types);
    int func_index =
        module_builder_.AddFunction(sig, "f", TestingModuleBuilder::kWasm);
    WasmFunction* function = module_builder_.GetFunctionAt(func_index);
    function->code = {module_builder_.AddBytes(function_bytes.as_vector()),
                      static_cast<uint32_t>(function_bytes.size())};
    FunctionBody body{function->sig, 0, function_bytes.begin(),
                      function_bytes.end()};
    return {std::move(function_bytes), function, body};
  }

  Isolate* isolate_;
  HandleScope handle_scope_;
  Zone zone_;
  TestingModuleBuilder module_builder_;
};

struct DebugSideTableEntry {
  std::vector<DebugSideTable::Entry::Value> values;

  // Construct via vector or implicitly via initializer list.
  explicit DebugSideTableEntry(std::vector<DebugSideTable::Entry::Value> values)
      : values(std::move(values)) {}
  DebugSideTableEntry(
      std::initializer_list<DebugSideTable::Entry::Value> values)
      : values(values) {}

  bool operator==(const DebugSideTableEntry& other) const {
    if (values.size() != other.values.size()) return false;
    for (size_t i = 0; i < values.size(); ++i) {
      if (values[i].type != other.values[i].type) return false;
      if (values[i].kind != other.values[i].kind) return false;
      // Stack offsets and register codes are platform dependent, so only check
      // constants here.
      if (values[i].kind == DebugSideTable::Entry::kConstant &&
          values[i].i32_const != other.values[i].i32_const) {
        return false;
      }
    }
    return true;
  }
};

// Debug builds will print the vector of DebugSideTableEntry.
#ifdef DEBUG
std::ostream& operator<<(std::ostream& out, const DebugSideTableEntry& entry) {
  out << "{";
  const char* comma = "";
  for (auto& v : entry.values) {
    out << comma << ValueTypes::TypeName(v.type) << " ";
    switch (v.kind) {
      case DebugSideTable::Entry::kConstant:
        out << "const:" << v.i32_const;
        break;
      case DebugSideTable::Entry::kRegister:
        out << "reg";
        break;
      case DebugSideTable::Entry::kStack:
        out << "stack";
        break;
    }
    comma = ", ";
  }
  return out << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const std::vector<DebugSideTableEntry>& entries) {
  return out << PrintCollection(entries);
}
#endif  // DEBUG

// Named constructors to make the tests more readable.
DebugSideTable::Entry::Value Constant(ValueType type, int32_t constant) {
  DebugSideTable::Entry::Value value;
  value.type = type;
  value.kind = DebugSideTable::Entry::kConstant;
  value.i32_const = constant;
  return value;
}
DebugSideTable::Entry::Value Register(ValueType type) {
  DebugSideTable::Entry::Value value;
  value.type = type;
  value.kind = DebugSideTable::Entry::kRegister;
  return value;
}
DebugSideTable::Entry::Value Stack(ValueType type) {
  DebugSideTable::Entry::Value value;
  value.type = type;
  value.kind = DebugSideTable::Entry::kStack;
  return value;
}

void CheckDebugSideTable(std::vector<DebugSideTableEntry> expected_entries,
                         const wasm::DebugSideTable* debug_side_table) {
  std::vector<DebugSideTableEntry> entries;
  for (auto& entry : debug_side_table->entries()) {
    auto values = entry.values();
    entries.push_back(
        DebugSideTableEntry{std::vector<DebugSideTable::Entry::Value>{
            values.begin(), values.end()}});
  }
  CHECK_EQ(expected_entries, entries);
}

}  // namespace

TEST(Liftoff_deterministic_simple) {
  LiftoffCompileEnvironment env;
  env.CheckDeterministicCompilation(
      {kWasmI32}, {kWasmI32, kWasmI32},
      {WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))});
}

TEST(Liftoff_deterministic_call) {
  LiftoffCompileEnvironment env;
  env.CheckDeterministicCompilation(
      {kWasmI32}, {kWasmI32},
      {WASM_I32_ADD(WASM_CALL_FUNCTION(0, WASM_GET_LOCAL(0)),
                    WASM_GET_LOCAL(0))});
}

TEST(Liftoff_deterministic_indirect_call) {
  LiftoffCompileEnvironment env;
  env.CheckDeterministicCompilation(
      {kWasmI32}, {kWasmI32},
      {WASM_I32_ADD(WASM_CALL_INDIRECT(0, WASM_GET_LOCAL(0), WASM_I32V_1(47)),
                    WASM_GET_LOCAL(0))});
}

TEST(Liftoff_deterministic_loop) {
  LiftoffCompileEnvironment env;
  env.CheckDeterministicCompilation(
      {kWasmI32}, {kWasmI32},
      {WASM_LOOP(WASM_BR_IF(0, WASM_GET_LOCAL(0))), WASM_GET_LOCAL(0)});
}

TEST(Liftoff_deterministic_trap) {
  LiftoffCompileEnvironment env;
  env.CheckDeterministicCompilation(
      {kWasmI32}, {kWasmI32, kWasmI32},
      {WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))});
}

TEST(Liftoff_debug_side_table_simple) {
  LiftoffCompileEnvironment env;
  auto debug_side_table = env.GenerateDebugSideTable(
      {kWasmI32}, {kWasmI32, kWasmI32},
      {WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))});
  CheckDebugSideTable(
      {
          // OOL stack check, locals spilled, stack empty.
          {Stack(kWasmI32), Stack(kWasmI32)},
      },
      debug_side_table.get());
}

TEST(Liftoff_debug_side_table_call) {
  LiftoffCompileEnvironment env;
  auto debug_side_table = env.GenerateDebugSideTable(
      {kWasmI32}, {kWasmI32},
      {WASM_I32_ADD(WASM_CALL_FUNCTION(0, WASM_GET_LOCAL(0)),
                    WASM_GET_LOCAL(0))});
  CheckDebugSideTable(
      {
          // call, local spilled, stack empty.
          {Stack(kWasmI32)},
          // OOL stack check, local spilled, stack empty.
          {Stack(kWasmI32)},
      },
      debug_side_table.get());
}

TEST(Liftoff_debug_side_table_call_const) {
  LiftoffCompileEnvironment env;
  constexpr int kConst = 13;
  auto debug_side_table = env.GenerateDebugSideTable(
      {kWasmI32}, {kWasmI32},
      {WASM_SET_LOCAL(0, WASM_I32V_1(kConst)),
       WASM_I32_ADD(WASM_CALL_FUNCTION(0, WASM_GET_LOCAL(0)),
                    WASM_GET_LOCAL(0))});
  CheckDebugSideTable(
      {
          // call, local is kConst.
          {Constant(kWasmI32, kConst)},
          // OOL stack check, local spilled.
          {Stack(kWasmI32)},
      },
      debug_side_table.get());
}

TEST(Liftoff_debug_side_table_indirect_call) {
  LiftoffCompileEnvironment env;
  constexpr int kConst = 47;
  auto debug_side_table = env.GenerateDebugSideTable(
      {kWasmI32}, {kWasmI32},
      {WASM_I32_ADD(WASM_CALL_INDIRECT(0, WASM_I32V_1(47), WASM_GET_LOCAL(0)),
                    WASM_GET_LOCAL(0))});
  CheckDebugSideTable(
      {
          // indirect call, local spilled, stack empty.
          {Stack(kWasmI32)},
          // OOL stack check, local spilled, stack empty.
          {Stack(kWasmI32)},
          // OOL trap (invalid index), local spilled, stack has {kConst}.
          {Stack(kWasmI32), Constant(kWasmI32, kConst)},
          // OOL trap (sig mismatch), local spilled, stack has {kConst}.
          {Stack(kWasmI32), Constant(kWasmI32, kConst)},
      },
      debug_side_table.get());
}

TEST(Liftoff_debug_side_table_loop) {
  LiftoffCompileEnvironment env;
  constexpr int kConst = 42;
  auto debug_side_table = env.GenerateDebugSideTable(
      {kWasmI32}, {kWasmI32},
      {WASM_I32V_1(kConst), WASM_LOOP(WASM_BR_IF(0, WASM_GET_LOCAL(0)))});
  CheckDebugSideTable(
      {
          // OOL stack check, local spilled, stack empty.
          {Stack(kWasmI32)},
          // OOL loop stack check, local spilled, stack has {kConst}.
          {Stack(kWasmI32), Constant(kWasmI32, kConst)},
      },
      debug_side_table.get());
}

TEST(Liftoff_debug_side_table_trap) {
  LiftoffCompileEnvironment env;
  auto debug_side_table = env.GenerateDebugSideTable(
      {kWasmI32}, {kWasmI32, kWasmI32},
      {WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))});
  CheckDebugSideTable(
      {
          // OOL stack check, local spilled, stack empty.
          {Stack(kWasmI32), Stack(kWasmI32)},
          // OOL trap (div by zero), locals spilled, stack empty.
          {Stack(kWasmI32), Stack(kWasmI32)},
          // OOL trap (result unrepresentable), locals spilled, stack empty.
          {Stack(kWasmI32), Stack(kWasmI32)},
      },
      debug_side_table.get());
}

TEST(Liftoff_breakpoint_simple) {
  LiftoffCompileEnvironment env;
  // Set two breakpoints. At both locations, values are live in registers.
  auto debug_side_table = env.GenerateDebugSideTable(
      {kWasmI32}, {kWasmI32, kWasmI32},
      {WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))},
      {
          1,  // break at beginning of function (first local.get)
          5   // break at i32.add
      });
  CheckDebugSideTable(
      {
          // First break point, locals in registers.
          {Register(kWasmI32), Register(kWasmI32)},
          // Second break point, locals and two stack values in registers.
          {Register(kWasmI32), Register(kWasmI32), Register(kWasmI32),
           Register(kWasmI32)},
          // OOL stack check, locals spilled, stack empty.
          {Stack(kWasmI32), Stack(kWasmI32)},
      },
      debug_side_table.get());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
