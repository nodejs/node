// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/execution.h"
#include "src/handles.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/interpreter.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-feedback-vector.h"

namespace v8 {
namespace internal {
namespace interpreter {


static MaybeHandle<Object> CallInterpreter(Isolate* isolate,
                                           Handle<JSFunction> function) {
  return Execution::Call(isolate, function,
                         isolate->factory()->undefined_value(), 0, nullptr);
}


template <class... A>
static MaybeHandle<Object> CallInterpreter(Isolate* isolate,
                                           Handle<JSFunction> function,
                                           A... args) {
  Handle<Object> argv[] = { args... };
  return Execution::Call(isolate, function,
                         isolate->factory()->undefined_value(), sizeof...(args),
                         argv);
}


template <class... A>
class InterpreterCallable {
 public:
  InterpreterCallable(Isolate* isolate, Handle<JSFunction> function)
      : isolate_(isolate), function_(function) {}
  virtual ~InterpreterCallable() {}

  MaybeHandle<Object> operator()(A... args) {
    return CallInterpreter(isolate_, function_, args...);
  }

 private:
  Isolate* isolate_;
  Handle<JSFunction> function_;
};


static const char* kFunctionName = "f";


class InterpreterTester {
 public:
  InterpreterTester(Isolate* isolate, const char* source,
                    MaybeHandle<BytecodeArray> bytecode,
                    MaybeHandle<TypeFeedbackVector> feedback_vector,
                    const char* filter)
      : isolate_(isolate),
        source_(source),
        bytecode_(bytecode),
        feedback_vector_(feedback_vector) {
    i::FLAG_ignition = true;
    i::FLAG_ignition_fake_try_catch = true;
    i::FLAG_ignition_fallback_on_eval_and_catch = false;
    i::FLAG_always_opt = false;
    // Set ignition filter flag via SetFlagsFromString to avoid double-free
    // (or potential leak with StrDup() based on ownership confusion).
    ScopedVector<char> ignition_filter(64);
    SNPrintF(ignition_filter, "--ignition-filter=%s", filter);
    FlagList::SetFlagsFromString(ignition_filter.start(),
                                 ignition_filter.length());
    // Ensure handler table is generated.
    isolate->interpreter()->Initialize();
  }

  InterpreterTester(Isolate* isolate, Handle<BytecodeArray> bytecode,
                    MaybeHandle<TypeFeedbackVector> feedback_vector =
                        MaybeHandle<TypeFeedbackVector>(),
                    const char* filter = kFunctionName)
      : InterpreterTester(isolate, nullptr, bytecode, feedback_vector, filter) {
  }


  InterpreterTester(Isolate* isolate, const char* source,
                    const char* filter = kFunctionName)
      : InterpreterTester(isolate, source, MaybeHandle<BytecodeArray>(),
                          MaybeHandle<TypeFeedbackVector>(), filter) {}

  virtual ~InterpreterTester() {}

  template <class... A>
  InterpreterCallable<A...> GetCallable() {
    return InterpreterCallable<A...>(isolate_, GetBytecodeFunction<A...>());
  }

  static Handle<Object> NewObject(const char* script) {
    return v8::Utils::OpenHandle(*CompileRun(script));
  }

  static Handle<String> GetName(Isolate* isolate, const char* name) {
    Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(name);
    return isolate->factory()->string_table()->LookupString(isolate, result);
  }

  static std::string SourceForBody(const char* body) {
    return "function " + function_name() + "() {\n" + std::string(body) + "\n}";
  }

  static std::string function_name() {
    return std::string(kFunctionName);
  }

 private:
  Isolate* isolate_;
  const char* source_;
  MaybeHandle<BytecodeArray> bytecode_;
  MaybeHandle<TypeFeedbackVector> feedback_vector_;

  template <class... A>
  Handle<JSFunction> GetBytecodeFunction() {
    Handle<JSFunction> function;
    if (source_) {
      CompileRun(source_);
      v8::Local<v8::Context> context =
          v8::Isolate::GetCurrent()->GetCurrentContext();
      Local<Function> api_function =
          Local<Function>::Cast(CcTest::global()
                                    ->Get(context, v8_str(kFunctionName))
                                    .ToLocalChecked());
      function = Handle<JSFunction>::cast(v8::Utils::OpenHandle(*api_function));
    } else {
      int arg_count = sizeof...(A);
      std::string source("(function " + function_name() + "(");
      for (int i = 0; i < arg_count; i++) {
        source += i == 0 ? "a" : ", a";
      }
      source += "){})";
      function = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
          *v8::Local<v8::Function>::Cast(CompileRun(source.c_str()))));
      function->ReplaceCode(
          *isolate_->builtins()->InterpreterEntryTrampoline());
    }

    if (!bytecode_.is_null()) {
      function->shared()->set_function_data(*bytecode_.ToHandleChecked());
    }
    if (!feedback_vector_.is_null()) {
      function->shared()->set_feedback_vector(
          *feedback_vector_.ToHandleChecked());
    }
    return function;
  }

  DISALLOW_COPY_AND_ASSIGN(InterpreterTester);
};


TEST(InterpreterReturn) {
  HandleAndZoneScope handles;
  Handle<Object> undefined_value =
      handles.main_isolate()->factory()->undefined_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(undefined_value));
}


TEST(InterpreterLoadUndefined) {
  HandleAndZoneScope handles;
  Handle<Object> undefined_value =
      handles.main_isolate()->factory()->undefined_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadUndefined().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(undefined_value));
}


TEST(InterpreterLoadNull) {
  HandleAndZoneScope handles;
  Handle<Object> null_value = handles.main_isolate()->factory()->null_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadNull().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(null_value));
}


TEST(InterpreterLoadTheHole) {
  HandleAndZoneScope handles;
  Handle<Object> the_hole_value =
      handles.main_isolate()->factory()->the_hole_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadTheHole().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(the_hole_value));
}


TEST(InterpreterLoadTrue) {
  HandleAndZoneScope handles;
  Handle<Object> true_value = handles.main_isolate()->factory()->true_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadTrue().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(true_value));
}


TEST(InterpreterLoadFalse) {
  HandleAndZoneScope handles;
  Handle<Object> false_value = handles.main_isolate()->factory()->false_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadFalse().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(false_value));
}


TEST(InterpreterLoadLiteral) {
  HandleAndZoneScope handles;
  i::Factory* factory = handles.main_isolate()->factory();

  // Small Smis.
  for (int i = -128; i < 128; i++) {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(0);
    builder.set_context_count(0);
    builder.set_parameter_count(1);
    builder.LoadLiteral(Smi::FromInt(i)).Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(i));
  }

  // Large Smis.
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(0);
    builder.set_context_count(0);
    builder.set_parameter_count(1);
    builder.LoadLiteral(Smi::FromInt(0x12345678)).Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(0x12345678));
  }

  // Heap numbers.
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(0);
    builder.set_context_count(0);
    builder.set_parameter_count(1);
    builder.LoadLiteral(factory->NewHeapNumber(-2.1e19)).Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK_EQ(i::HeapNumber::cast(*return_val)->value(), -2.1e19);
  }

  // Strings.
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(0);
    builder.set_context_count(0);
    builder.set_parameter_count(1);
    Handle<i::String> string = factory->NewStringFromAsciiChecked("String");
    builder.LoadLiteral(string).Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK(i::String::cast(*return_val)->Equals(*string));
  }
}


TEST(InterpreterLoadStoreRegisters) {
  HandleAndZoneScope handles;
  Handle<Object> true_value = handles.main_isolate()->factory()->true_value();
  for (int i = 0; i <= kMaxInt8; i++) {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(i + 1);
    builder.set_context_count(0);
    builder.set_parameter_count(1);
    Register reg(i);
    builder.LoadTrue()
        .StoreAccumulatorInRegister(reg)
        .LoadFalse()
        .LoadAccumulatorWithRegister(reg)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK(return_val.is_identical_to(true_value));
  }
}


TEST(InterpreterExchangeRegisters) {
  for (int locals_count = 2; locals_count < 300; locals_count += 126) {
    HandleAndZoneScope handles;
    for (int exchanges = 1; exchanges < 4; exchanges++) {
      BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
      builder.set_locals_count(locals_count);
      builder.set_context_count(0);
      builder.set_parameter_count(0);

      Register r0(0);
      Register r1(locals_count - 1);
      builder.LoadTrue();
      builder.StoreAccumulatorInRegister(r0);
      builder.ExchangeRegisters(r0, r1);
      builder.LoadFalse();
      builder.StoreAccumulatorInRegister(r0);

      bool expected = false;
      for (int i = 0; i < exchanges; i++) {
        builder.ExchangeRegisters(r0, r1);
        expected = !expected;
      }
      builder.LoadAccumulatorWithRegister(r0);
      builder.Return();
      Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
      InterpreterTester tester(handles.main_isolate(), bytecode_array);
      auto callable = tester.GetCallable<>();
      Handle<Object> return_val = callable().ToHandleChecked();
      Handle<Object> expected_val =
          handles.main_isolate()->factory()->ToBoolean(expected);
      CHECK(return_val.is_identical_to(expected_val));
    }
  }
}


TEST(InterpreterExchangeRegistersWithParameter) {
  for (int locals_count = 2; locals_count < 300; locals_count += 126) {
    HandleAndZoneScope handles;
    for (int exchanges = 1; exchanges < 4; exchanges++) {
      BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
      builder.set_locals_count(locals_count);
      builder.set_context_count(0);
      builder.set_parameter_count(3);

      Register r0 = Register::FromParameterIndex(2, 3);
      Register r1(locals_count - 1);
      builder.LoadTrue();
      builder.StoreAccumulatorInRegister(r0);
      builder.ExchangeRegisters(r0, r1);
      builder.LoadFalse();
      builder.StoreAccumulatorInRegister(r0);

      bool expected = false;
      for (int i = 0; i < exchanges; i++) {
        builder.ExchangeRegisters(r0, r1);
        expected = !expected;
      }
      builder.LoadAccumulatorWithRegister(r0);
      builder.Return();
      Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
      InterpreterTester tester(handles.main_isolate(), bytecode_array);
      auto callable = tester.GetCallable<>();
      Handle<Object> return_val = callable().ToHandleChecked();
      Handle<Object> expected_val =
          handles.main_isolate()->factory()->ToBoolean(expected);
      CHECK(return_val.is_identical_to(expected_val));
    }
  }
}


TEST(InterpreterExchangeWideRegisters) {
  for (int locals_count = 3; locals_count < 300; locals_count += 126) {
    HandleAndZoneScope handles;
    for (int exchanges = 0; exchanges < 7; exchanges++) {
      BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
      builder.set_locals_count(locals_count);
      builder.set_context_count(0);
      builder.set_parameter_count(0);

      Register r0(0);
      Register r1(locals_count - 1);
      Register r2(locals_count - 2);
      builder.LoadLiteral(Smi::FromInt(200));
      builder.StoreAccumulatorInRegister(r0);
      builder.ExchangeRegisters(r0, r1);
      builder.LoadLiteral(Smi::FromInt(100));
      builder.StoreAccumulatorInRegister(r0);
      builder.ExchangeRegisters(r0, r2);
      builder.LoadLiteral(Smi::FromInt(0));
      builder.StoreAccumulatorInRegister(r0);
      for (int i = 0; i < exchanges; i++) {
        builder.ExchangeRegisters(r1, r2);
        builder.ExchangeRegisters(r0, r1);
      }
      builder.LoadAccumulatorWithRegister(r0);
      builder.Return();
      Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
      InterpreterTester tester(handles.main_isolate(), bytecode_array);
      auto callable = tester.GetCallable<>();
      Handle<Object> return_val = callable().ToHandleChecked();
      Handle<Object> expected_val =
          handles.main_isolate()->factory()->NewNumberFromInt(100 *
                                                              (exchanges % 3));
      CHECK(return_val.is_identical_to(expected_val));
    }
  }
}


static const Token::Value kShiftOperators[] = {
    Token::Value::SHL, Token::Value::SAR, Token::Value::SHR};


static const Token::Value kArithmeticOperators[] = {
    Token::Value::BIT_OR, Token::Value::BIT_XOR, Token::Value::BIT_AND,
    Token::Value::SHL,    Token::Value::SAR,     Token::Value::SHR,
    Token::Value::ADD,    Token::Value::SUB,     Token::Value::MUL,
    Token::Value::DIV,    Token::Value::MOD};


static double BinaryOpC(Token::Value op, double lhs, double rhs) {
  switch (op) {
    case Token::Value::ADD:
      return lhs + rhs;
    case Token::Value::SUB:
      return lhs - rhs;
    case Token::Value::MUL:
      return lhs * rhs;
    case Token::Value::DIV:
      return lhs / rhs;
    case Token::Value::MOD:
      return std::fmod(lhs, rhs);
    case Token::Value::BIT_OR:
      return (v8::internal::DoubleToInt32(lhs) |
              v8::internal::DoubleToInt32(rhs));
    case Token::Value::BIT_XOR:
      return (v8::internal::DoubleToInt32(lhs) ^
              v8::internal::DoubleToInt32(rhs));
    case Token::Value::BIT_AND:
      return (v8::internal::DoubleToInt32(lhs) &
              v8::internal::DoubleToInt32(rhs));
    case Token::Value::SHL: {
      int32_t val = v8::internal::DoubleToInt32(lhs);
      uint32_t count = v8::internal::DoubleToUint32(rhs) & 0x1F;
      int32_t result = val << count;
      return result;
    }
    case Token::Value::SAR: {
      int32_t val = v8::internal::DoubleToInt32(lhs);
      uint32_t count = v8::internal::DoubleToUint32(rhs) & 0x1F;
      int32_t result = val >> count;
      return result;
    }
    case Token::Value::SHR: {
      uint32_t val = v8::internal::DoubleToUint32(lhs);
      uint32_t count = v8::internal::DoubleToUint32(rhs) & 0x1F;
      uint32_t result = val >> count;
      return result;
    }
    default:
      UNREACHABLE();
      return std::numeric_limits<double>::min();
  }
}


TEST(InterpreterShiftOpsSmi) {
  int lhs_inputs[] = {0, -17, -182, 1073741823, -1};
  int rhs_inputs[] = {5, 2, 1, -1, -2, 0, 31, 32, -32, 64, 37};
  for (size_t l = 0; l < arraysize(lhs_inputs); l++) {
    for (size_t r = 0; r < arraysize(rhs_inputs); r++) {
      for (size_t o = 0; o < arraysize(kShiftOperators); o++) {
        HandleAndZoneScope handles;
        i::Factory* factory = handles.main_isolate()->factory();
        BytecodeArrayBuilder builder(handles.main_isolate(),
                                     handles.main_zone());
        builder.set_locals_count(1);
        builder.set_context_count(0);
        builder.set_parameter_count(1);
        Register reg(0);
        int lhs = lhs_inputs[l];
        int rhs = rhs_inputs[r];
        builder.LoadLiteral(Smi::FromInt(lhs))
            .StoreAccumulatorInRegister(reg)
            .LoadLiteral(Smi::FromInt(rhs))
            .BinaryOperation(kShiftOperators[o], reg, Strength::WEAK)
            .Return();
        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

        InterpreterTester tester(handles.main_isolate(), bytecode_array);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        Handle<Object> expected_value =
            factory->NewNumber(BinaryOpC(kShiftOperators[o], lhs, rhs));
        CHECK(return_value->SameValue(*expected_value));
      }
    }
  }
}


TEST(InterpreterBinaryOpsSmi) {
  int lhs_inputs[] = {3266, 1024, 0, -17, -18000};
  int rhs_inputs[] = {3266, 5, 4, 3, 2, 1, -1, -2};
  for (size_t l = 0; l < arraysize(lhs_inputs); l++) {
    for (size_t r = 0; r < arraysize(rhs_inputs); r++) {
      for (size_t o = 0; o < arraysize(kArithmeticOperators); o++) {
        HandleAndZoneScope handles;
        i::Factory* factory = handles.main_isolate()->factory();
        BytecodeArrayBuilder builder(handles.main_isolate(),
                                     handles.main_zone());
        builder.set_locals_count(1);
        builder.set_context_count(0);
        builder.set_parameter_count(1);
        Register reg(0);
        int lhs = lhs_inputs[l];
        int rhs = rhs_inputs[r];
        builder.LoadLiteral(Smi::FromInt(lhs))
            .StoreAccumulatorInRegister(reg)
            .LoadLiteral(Smi::FromInt(rhs))
            .BinaryOperation(kArithmeticOperators[o], reg, Strength::WEAK)
            .Return();
        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

        InterpreterTester tester(handles.main_isolate(), bytecode_array);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        Handle<Object> expected_value =
            factory->NewNumber(BinaryOpC(kArithmeticOperators[o], lhs, rhs));
        CHECK(return_value->SameValue(*expected_value));
      }
    }
  }
}


TEST(InterpreterBinaryOpsHeapNumber) {
  double lhs_inputs[] = {3266.101, 1024.12, 0.01, -17.99, -18000.833, 9.1e17};
  double rhs_inputs[] = {3266.101, 5.999, 4.778, 3.331,  2.643,
                         1.1,      -1.8,  -2.9,  8.3e-27};
  for (size_t l = 0; l < arraysize(lhs_inputs); l++) {
    for (size_t r = 0; r < arraysize(rhs_inputs); r++) {
      for (size_t o = 0; o < arraysize(kArithmeticOperators); o++) {
        HandleAndZoneScope handles;
        i::Factory* factory = handles.main_isolate()->factory();
        BytecodeArrayBuilder builder(handles.main_isolate(),
                                     handles.main_zone());
        builder.set_locals_count(1);
        builder.set_context_count(0);
        builder.set_parameter_count(1);
        Register reg(0);
        double lhs = lhs_inputs[l];
        double rhs = rhs_inputs[r];
        builder.LoadLiteral(factory->NewNumber(lhs))
            .StoreAccumulatorInRegister(reg)
            .LoadLiteral(factory->NewNumber(rhs))
            .BinaryOperation(kArithmeticOperators[o], reg, Strength::WEAK)
            .Return();
        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

        InterpreterTester tester(handles.main_isolate(), bytecode_array);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        Handle<Object> expected_value =
            factory->NewNumber(BinaryOpC(kArithmeticOperators[o], lhs, rhs));
        CHECK(return_value->SameValue(*expected_value));
      }
    }
  }
}


TEST(InterpreterStringAdd) {
  HandleAndZoneScope handles;
  i::Factory* factory = handles.main_isolate()->factory();

  struct TestCase {
    Handle<Object> lhs;
    Handle<Object> rhs;
    Handle<Object> expected_value;
  } test_cases[] = {
      {factory->NewStringFromStaticChars("a"),
       factory->NewStringFromStaticChars("b"),
       factory->NewStringFromStaticChars("ab")},
      {factory->NewStringFromStaticChars("aaaaaa"),
       factory->NewStringFromStaticChars("b"),
       factory->NewStringFromStaticChars("aaaaaab")},
      {factory->NewStringFromStaticChars("aaa"),
       factory->NewStringFromStaticChars("bbbbb"),
       factory->NewStringFromStaticChars("aaabbbbb")},
      {factory->NewStringFromStaticChars(""),
       factory->NewStringFromStaticChars("b"),
       factory->NewStringFromStaticChars("b")},
      {factory->NewStringFromStaticChars("a"),
       factory->NewStringFromStaticChars(""),
       factory->NewStringFromStaticChars("a")},
      {factory->NewStringFromStaticChars("1.11"), factory->NewHeapNumber(2.5),
       factory->NewStringFromStaticChars("1.112.5")},
      {factory->NewStringFromStaticChars("-1.11"), factory->NewHeapNumber(2.56),
       factory->NewStringFromStaticChars("-1.112.56")},
      {factory->NewStringFromStaticChars(""), factory->NewHeapNumber(2.5),
       factory->NewStringFromStaticChars("2.5")},
  };

  for (size_t i = 0; i < arraysize(test_cases); i++) {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(1);
    builder.set_context_count(0);
    builder.set_parameter_count(1);
    Register reg(0);
    builder.LoadLiteral(test_cases[i].lhs)
        .StoreAccumulatorInRegister(reg)
        .LoadLiteral(test_cases[i].rhs)
        .BinaryOperation(Token::Value::ADD, reg, Strength::WEAK)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*test_cases[i].expected_value));
  }
}


TEST(InterpreterParameter1) {
  HandleAndZoneScope handles;
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadAccumulatorWithRegister(builder.Parameter(0)).Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<Handle<Object>>();

  // Check for heap objects.
  Handle<Object> true_value = handles.main_isolate()->factory()->true_value();
  Handle<Object> return_val = callable(true_value).ToHandleChecked();
  CHECK(return_val.is_identical_to(true_value));

  // Check for Smis.
  return_val = callable(Handle<Smi>(Smi::FromInt(3), handles.main_isolate()))
                   .ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(3));
}


TEST(InterpreterParameter8) {
  HandleAndZoneScope handles;
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_context_count(0);
  builder.set_parameter_count(8);
  builder.LoadAccumulatorWithRegister(builder.Parameter(0))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(1), Strength::WEAK)
      .BinaryOperation(Token::Value::ADD, builder.Parameter(2), Strength::WEAK)
      .BinaryOperation(Token::Value::ADD, builder.Parameter(3), Strength::WEAK)
      .BinaryOperation(Token::Value::ADD, builder.Parameter(4), Strength::WEAK)
      .BinaryOperation(Token::Value::ADD, builder.Parameter(5), Strength::WEAK)
      .BinaryOperation(Token::Value::ADD, builder.Parameter(6), Strength::WEAK)
      .BinaryOperation(Token::Value::ADD, builder.Parameter(7), Strength::WEAK)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  typedef Handle<Object> H;
  auto callable = tester.GetCallable<H, H, H, H, H, H, H, H>();

  Handle<Smi> arg1 = Handle<Smi>(Smi::FromInt(1), handles.main_isolate());
  Handle<Smi> arg2 = Handle<Smi>(Smi::FromInt(2), handles.main_isolate());
  Handle<Smi> arg3 = Handle<Smi>(Smi::FromInt(3), handles.main_isolate());
  Handle<Smi> arg4 = Handle<Smi>(Smi::FromInt(4), handles.main_isolate());
  Handle<Smi> arg5 = Handle<Smi>(Smi::FromInt(5), handles.main_isolate());
  Handle<Smi> arg6 = Handle<Smi>(Smi::FromInt(6), handles.main_isolate());
  Handle<Smi> arg7 = Handle<Smi>(Smi::FromInt(7), handles.main_isolate());
  Handle<Smi> arg8 = Handle<Smi>(Smi::FromInt(8), handles.main_isolate());
  // Check for Smis.
  Handle<Object> return_val =
      callable(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
          .ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(36));
}


TEST(InterpreterParameter1Assign) {
  HandleAndZoneScope handles;
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadLiteral(Smi::FromInt(5))
      .StoreAccumulatorInRegister(builder.Parameter(0))
      .LoadAccumulatorWithRegister(builder.Parameter(0))
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<Handle<Object>>();

  Handle<Object> return_val =
      callable(Handle<Smi>(Smi::FromInt(3), handles.main_isolate()))
          .ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(5));
}


TEST(InterpreterLoadGlobal) {
  HandleAndZoneScope handles;

  // Test loading a global.
  std::string source(
      "var global = 321;\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  return global;\n"
      "}");
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(321));
}


TEST(InterpreterStoreGlobal) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  // Test storing to a global.
  std::string source(
      "var global = 321;\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  global = 999;\n"
      "}");
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  callable().ToHandleChecked();
  Handle<i::String> name = factory->InternalizeUtf8String("global");
  Handle<i::Object> global_obj =
      Object::GetProperty(isolate->global_object(), name).ToHandleChecked();
  CHECK_EQ(Smi::cast(*global_obj), Smi::FromInt(999));
}


TEST(InterpreterCallGlobal) {
  HandleAndZoneScope handles;

  // Test calling a global function.
  std::string source(
      "function g_add(a, b) { return a + b; }\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  return g_add(5, 10);\n"
      "}");
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(15));
}


TEST(InterpreterLoadUnallocated) {
  HandleAndZoneScope handles;

  // Test loading an unallocated global.
  std::string source(
      "unallocated = 123;\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  return unallocated;\n"
      "}");
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));
}


TEST(InterpreterStoreUnallocated) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  // Test storing to an unallocated global.
  std::string source(
      "unallocated = 321;\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  unallocated = 999;\n"
      "}");
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  callable().ToHandleChecked();
  Handle<i::String> name = factory->InternalizeUtf8String("unallocated");
  Handle<i::Object> global_obj =
      Object::GetProperty(isolate->global_object(), name).ToHandleChecked();
  CHECK_EQ(Smi::cast(*global_obj), Smi::FromInt(999));
}


TEST(InterpreterLoadNamedProperty) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();
  i::Zone zone;

  i::FeedbackVectorSpec feedback_spec(&zone);
  i::FeedbackVectorSlot slot = feedback_spec.AddLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(isolate, &feedback_spec);

  Handle<i::String> name = factory->NewStringFromAsciiChecked("val");
  name = factory->string_table()->LookupString(isolate, name);

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadNamedProperty(builder.Parameter(0), name, vector->GetIndex(slot),
                            i::SLOPPY)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
  auto callable = tester.GetCallable<Handle<Object>>();

  Handle<Object> object = InterpreterTester::NewObject("({ val : 123 })");
  // Test IC miss.
  Handle<Object> return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to monomorphic IC.
  return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to polymorphic IC.
  Handle<Object> object2 =
      InterpreterTester::NewObject("({ val : 456, other : 123 })");
  return_val = callable(object2).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(456));

  // Test transition to megamorphic IC.
  Handle<Object> object3 =
      InterpreterTester::NewObject("({ val : 789, val2 : 123 })");
  callable(object3).ToHandleChecked();
  Handle<Object> object4 =
      InterpreterTester::NewObject("({ val : 789, val3 : 123 })");
  callable(object4).ToHandleChecked();
  Handle<Object> object5 =
      InterpreterTester::NewObject("({ val : 789, val4 : 123 })");
  return_val = callable(object5).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(789));
}


TEST(InterpreterLoadKeyedProperty) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();
  i::Zone zone;

  i::FeedbackVectorSpec feedback_spec(&zone);
  i::FeedbackVectorSlot slot = feedback_spec.AddKeyedLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(isolate, &feedback_spec);

  Handle<i::String> key = factory->NewStringFromAsciiChecked("key");
  key = factory->string_table()->LookupString(isolate, key);

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(1);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadLiteral(key)
      .LoadKeyedProperty(builder.Parameter(0), vector->GetIndex(slot),
                         i::STRICT)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
  auto callable = tester.GetCallable<Handle<Object>>();

  Handle<Object> object = InterpreterTester::NewObject("({ key : 123 })");
  // Test IC miss.
  Handle<Object> return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to monomorphic IC.
  return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to megamorphic IC.
  Handle<Object> object3 =
      InterpreterTester::NewObject("({ key : 789, val2 : 123 })");
  return_val = callable(object3).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(789));
}


TEST(InterpreterStoreNamedProperty) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();
  i::Zone zone;

  i::FeedbackVectorSpec feedback_spec(&zone);
  i::FeedbackVectorSlot slot = feedback_spec.AddStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(isolate, &feedback_spec);

  Handle<i::String> name = factory->NewStringFromAsciiChecked("val");
  name = factory->string_table()->LookupString(isolate, name);

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadLiteral(Smi::FromInt(999))
      .StoreNamedProperty(builder.Parameter(0), name, vector->GetIndex(slot),
                          i::STRICT)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(isolate, bytecode_array, vector);
  auto callable = tester.GetCallable<Handle<Object>>();
  Handle<Object> object = InterpreterTester::NewObject("({ val : 123 })");
  // Test IC miss.
  Handle<Object> result;
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to monomorphic IC.
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to polymorphic IC.
  Handle<Object> object2 =
      InterpreterTester::NewObject("({ val : 456, other : 123 })");
  callable(object2).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object2, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to megamorphic IC.
  Handle<Object> object3 =
      InterpreterTester::NewObject("({ val : 789, val2 : 123 })");
  callable(object3).ToHandleChecked();
  Handle<Object> object4 =
      InterpreterTester::NewObject("({ val : 789, val3 : 123 })");
  callable(object4).ToHandleChecked();
  Handle<Object> object5 =
      InterpreterTester::NewObject("({ val : 789, val4 : 123 })");
  callable(object5).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object5, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));
}


TEST(InterpreterStoreKeyedProperty) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();
  i::Zone zone;

  i::FeedbackVectorSpec feedback_spec(&zone);
  i::FeedbackVectorSlot slot = feedback_spec.AddKeyedStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(isolate, &feedback_spec);

  Handle<i::String> name = factory->NewStringFromAsciiChecked("val");
  name = factory->string_table()->LookupString(isolate, name);

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(1);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadLiteral(name)
      .StoreAccumulatorInRegister(Register(0))
      .LoadLiteral(Smi::FromInt(999))
      .StoreKeyedProperty(builder.Parameter(0), Register(0),
                          vector->GetIndex(slot), i::SLOPPY)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(isolate, bytecode_array, vector);
  auto callable = tester.GetCallable<Handle<Object>>();
  Handle<Object> object = InterpreterTester::NewObject("({ val : 123 })");
  // Test IC miss.
  Handle<Object> result;
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to monomorphic IC.
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to megamorphic IC.
  Handle<Object> object2 =
      InterpreterTester::NewObject("({ val : 456, other : 123 })");
  callable(object2).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object2, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));
}


TEST(InterpreterCall) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();
  i::Zone zone;

  i::FeedbackVectorSpec feedback_spec(&zone);
  i::FeedbackVectorSlot slot = feedback_spec.AddLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(isolate, &feedback_spec);
  int slot_index = vector->GetIndex(slot);

  Handle<i::String> name = factory->NewStringFromAsciiChecked("func");
  name = factory->string_table()->LookupString(isolate, name);

  // Check with no args.
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(1);
    builder.set_context_count(0);
    builder.set_parameter_count(1);
    builder.LoadNamedProperty(builder.Parameter(0), name, slot_index, i::SLOPPY)
        .StoreAccumulatorInRegister(Register(0))
        .Call(Register(0), builder.Parameter(0), 0, 0)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() { this.func = function() { return 0x265; }})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(0x265));
  }

  // Check that receiver is passed properly.
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(1);
    builder.set_context_count(0);
    builder.set_parameter_count(1);
    builder.LoadNamedProperty(builder.Parameter(0), name, slot_index, i::SLOPPY)
        .StoreAccumulatorInRegister(Register(0))
        .Call(Register(0), builder.Parameter(0), 0, 0)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() {"
        "  this.val = 1234;"
        "  this.func = function() { return this.val; };"
        "})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(1234));
  }

  // Check with two parameters (+ receiver).
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(4);
    builder.set_context_count(0);
    builder.set_parameter_count(1);
    builder.LoadNamedProperty(builder.Parameter(0), name, slot_index, i::SLOPPY)
        .StoreAccumulatorInRegister(Register(0))
        .LoadAccumulatorWithRegister(builder.Parameter(0))
        .StoreAccumulatorInRegister(Register(1))
        .LoadLiteral(Smi::FromInt(51))
        .StoreAccumulatorInRegister(Register(2))
        .LoadLiteral(Smi::FromInt(11))
        .StoreAccumulatorInRegister(Register(3))
        .Call(Register(0), Register(1), 2, 0)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() { "
        "  this.func = function(a, b) { return a - b; }"
        "})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    CHECK(return_val->SameValue(Smi::FromInt(40)));
  }

  // Check with 10 parameters (+ receiver).
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(12);
    builder.set_context_count(0);
    builder.set_parameter_count(1);
    builder.LoadNamedProperty(builder.Parameter(0), name, slot_index, i::SLOPPY)
        .StoreAccumulatorInRegister(Register(0))
        .LoadAccumulatorWithRegister(builder.Parameter(0))
        .StoreAccumulatorInRegister(Register(1))
        .LoadLiteral(factory->NewStringFromAsciiChecked("a"))
        .StoreAccumulatorInRegister(Register(2))
        .LoadLiteral(factory->NewStringFromAsciiChecked("b"))
        .StoreAccumulatorInRegister(Register(3))
        .LoadLiteral(factory->NewStringFromAsciiChecked("c"))
        .StoreAccumulatorInRegister(Register(4))
        .LoadLiteral(factory->NewStringFromAsciiChecked("d"))
        .StoreAccumulatorInRegister(Register(5))
        .LoadLiteral(factory->NewStringFromAsciiChecked("e"))
        .StoreAccumulatorInRegister(Register(6))
        .LoadLiteral(factory->NewStringFromAsciiChecked("f"))
        .StoreAccumulatorInRegister(Register(7))
        .LoadLiteral(factory->NewStringFromAsciiChecked("g"))
        .StoreAccumulatorInRegister(Register(8))
        .LoadLiteral(factory->NewStringFromAsciiChecked("h"))
        .StoreAccumulatorInRegister(Register(9))
        .LoadLiteral(factory->NewStringFromAsciiChecked("i"))
        .StoreAccumulatorInRegister(Register(10))
        .LoadLiteral(factory->NewStringFromAsciiChecked("j"))
        .StoreAccumulatorInRegister(Register(11))
        .Call(Register(0), Register(1), 10, 0)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() { "
        "  this.prefix = \"prefix_\";"
        "  this.func = function(a, b, c, d, e, f, g, h, i, j) {"
        "      return this.prefix + a + b + c + d + e + f + g + h + i + j;"
        "  }"
        "})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    Handle<i::String> expected =
        factory->NewStringFromAsciiChecked("prefix_abcdefghij");
    CHECK(i::String::cast(*return_val)->Equals(*expected));
  }
}


static BytecodeArrayBuilder& SetRegister(BytecodeArrayBuilder& builder,
                                         Register reg, int value,
                                         Register scratch) {
  return builder.StoreAccumulatorInRegister(scratch)
      .LoadLiteral(Smi::FromInt(value))
      .StoreAccumulatorInRegister(reg)
      .LoadAccumulatorWithRegister(scratch);
}


static BytecodeArrayBuilder& IncrementRegister(BytecodeArrayBuilder& builder,
                                               Register reg, int value,
                                               Register scratch) {
  return builder.StoreAccumulatorInRegister(scratch)
      .LoadLiteral(Smi::FromInt(value))
      .BinaryOperation(Token::Value::ADD, reg, Strength::WEAK)
      .StoreAccumulatorInRegister(reg)
      .LoadAccumulatorWithRegister(scratch);
}


TEST(InterpreterJumps) {
  HandleAndZoneScope handles;
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(2);
  builder.set_context_count(0);
  builder.set_parameter_count(0);
  Register reg(0), scratch(1);
  BytecodeLabel label[3];

  builder.LoadLiteral(Smi::FromInt(0))
      .StoreAccumulatorInRegister(reg)
      .Jump(&label[1]);
  SetRegister(builder, reg, 1024, scratch).Bind(&label[0]);
  IncrementRegister(builder, reg, 1, scratch).Jump(&label[2]);
  SetRegister(builder, reg, 2048, scratch).Bind(&label[1]);
  IncrementRegister(builder, reg, 2, scratch).Jump(&label[0]);
  SetRegister(builder, reg, 4096, scratch).Bind(&label[2]);
  IncrementRegister(builder, reg, 4, scratch)
      .LoadAccumulatorWithRegister(reg)
      .Return();

  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_value = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_value)->value(), 7);
}


TEST(InterpreterConditionalJumps) {
  HandleAndZoneScope handles;
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(2);
  builder.set_context_count(0);
  builder.set_parameter_count(0);
  Register reg(0), scratch(1);
  BytecodeLabel label[2];
  BytecodeLabel done, done1;

  builder.LoadLiteral(Smi::FromInt(0))
      .StoreAccumulatorInRegister(reg)
      .LoadFalse()
      .JumpIfFalse(&label[0]);
  IncrementRegister(builder, reg, 1024, scratch)
      .Bind(&label[0])
      .LoadTrue()
      .JumpIfFalse(&done);
  IncrementRegister(builder, reg, 1, scratch).LoadTrue().JumpIfTrue(&label[1]);
  IncrementRegister(builder, reg, 2048, scratch).Bind(&label[1]);
  IncrementRegister(builder, reg, 2, scratch).LoadFalse().JumpIfTrue(&done1);
  IncrementRegister(builder, reg, 4, scratch)
      .LoadAccumulatorWithRegister(reg)
      .Bind(&done)
      .Bind(&done1)
      .Return();

  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_value = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_value)->value(), 7);
}


TEST(InterpreterConditionalJumps2) {
  // TODO(oth): Add tests for all conditional jumps near and far.
  HandleAndZoneScope handles;
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(2);
  builder.set_context_count(0);
  builder.set_parameter_count(0);
  Register reg(0), scratch(1);
  BytecodeLabel label[2];
  BytecodeLabel done, done1;

  builder.LoadLiteral(Smi::FromInt(0))
      .StoreAccumulatorInRegister(reg)
      .LoadFalse()
      .JumpIfFalse(&label[0]);
  IncrementRegister(builder, reg, 1024, scratch)
      .Bind(&label[0])
      .LoadTrue()
      .JumpIfFalse(&done);
  IncrementRegister(builder, reg, 1, scratch).LoadTrue().JumpIfTrue(&label[1]);
  IncrementRegister(builder, reg, 2048, scratch).Bind(&label[1]);
  IncrementRegister(builder, reg, 2, scratch).LoadFalse().JumpIfTrue(&done1);
  IncrementRegister(builder, reg, 4, scratch)
      .LoadAccumulatorWithRegister(reg)
      .Bind(&done)
      .Bind(&done1)
      .Return();

  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_value = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_value)->value(), 7);
}


static const Token::Value kComparisonTypes[] = {
    Token::Value::EQ,        Token::Value::NE, Token::Value::EQ_STRICT,
    Token::Value::NE_STRICT, Token::Value::LT, Token::Value::LTE,
    Token::Value::GT,        Token::Value::GTE};


template <typename T>
bool CompareC(Token::Value op, T lhs, T rhs, bool types_differed = false) {
  switch (op) {
    case Token::Value::EQ:
      return lhs == rhs;
    case Token::Value::NE:
      return lhs != rhs;
    case Token::Value::EQ_STRICT:
      return (lhs == rhs) && !types_differed;
    case Token::Value::NE_STRICT:
      return (lhs != rhs) || types_differed;
    case Token::Value::LT:
      return lhs < rhs;
    case Token::Value::LTE:
      return lhs <= rhs;
    case Token::Value::GT:
      return lhs > rhs;
    case Token::Value::GTE:
      return lhs >= rhs;
    default:
      UNREACHABLE();
      return false;
  }
}


TEST(InterpreterSmiComparisons) {
  // NB Constants cover 31-bit space.
  int inputs[] = {v8::internal::kMinInt / 2,
                  v8::internal::kMinInt / 4,
                  -108733832,
                  -999,
                  -42,
                  -2,
                  -1,
                  0,
                  +1,
                  +2,
                  42,
                  12345678,
                  v8::internal::kMaxInt / 4,
                  v8::internal::kMaxInt / 2};

  for (size_t c = 0; c < arraysize(kComparisonTypes); c++) {
    Token::Value comparison = kComparisonTypes[c];
    for (size_t i = 0; i < arraysize(inputs); i++) {
      for (size_t j = 0; j < arraysize(inputs); j++) {
        HandleAndZoneScope handles;
        BytecodeArrayBuilder builder(handles.main_isolate(),
                                     handles.main_zone());
        Register r0(0);
        builder.set_locals_count(1);
        builder.set_context_count(0);
        builder.set_parameter_count(0);
        builder.LoadLiteral(Smi::FromInt(inputs[i]))
            .StoreAccumulatorInRegister(r0)
            .LoadLiteral(Smi::FromInt(inputs[j]))
            .CompareOperation(comparison, r0, Strength::WEAK)
            .Return();

        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
        InterpreterTester tester(handles.main_isolate(), bytecode_array);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        CHECK(return_value->IsBoolean());
        CHECK_EQ(return_value->BooleanValue(),
                 CompareC(comparison, inputs[i], inputs[j]));
      }
    }
  }
}


TEST(InterpreterHeapNumberComparisons) {
  double inputs[] = {std::numeric_limits<double>::min(),
                     std::numeric_limits<double>::max(),
                     -0.001,
                     0.01,
                     0.1000001,
                     1e99,
                     -1e-99};
  for (size_t c = 0; c < arraysize(kComparisonTypes); c++) {
    Token::Value comparison = kComparisonTypes[c];
    for (size_t i = 0; i < arraysize(inputs); i++) {
      for (size_t j = 0; j < arraysize(inputs); j++) {
        HandleAndZoneScope handles;
        i::Factory* factory = handles.main_isolate()->factory();
        BytecodeArrayBuilder builder(handles.main_isolate(),
                                     handles.main_zone());
        Register r0(0);
        builder.set_locals_count(1);
        builder.set_context_count(0);
        builder.set_parameter_count(0);
        builder.LoadLiteral(factory->NewHeapNumber(inputs[i]))
            .StoreAccumulatorInRegister(r0)
            .LoadLiteral(factory->NewHeapNumber(inputs[j]))
            .CompareOperation(comparison, r0, Strength::WEAK)
            .Return();

        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
        InterpreterTester tester(handles.main_isolate(), bytecode_array);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        CHECK(return_value->IsBoolean());
        CHECK_EQ(return_value->BooleanValue(),
                 CompareC(comparison, inputs[i], inputs[j]));
      }
    }
  }
}


TEST(InterpreterStringComparisons) {
  std::string inputs[] = {"A", "abc", "z", "", "Foo!", "Foo"};

  for (size_t c = 0; c < arraysize(kComparisonTypes); c++) {
    Token::Value comparison = kComparisonTypes[c];
    for (size_t i = 0; i < arraysize(inputs); i++) {
      for (size_t j = 0; j < arraysize(inputs); j++) {
        const char* lhs = inputs[i].c_str();
        const char* rhs = inputs[j].c_str();
        HandleAndZoneScope handles;
        i::Factory* factory = handles.main_isolate()->factory();
        BytecodeArrayBuilder builder(handles.main_isolate(),
                                     handles.main_zone());
        Register r0(0);
        builder.set_locals_count(1);
        builder.set_context_count(0);
        builder.set_parameter_count(0);
        builder.LoadLiteral(factory->NewStringFromAsciiChecked(lhs))
            .StoreAccumulatorInRegister(r0)
            .LoadLiteral(factory->NewStringFromAsciiChecked(rhs))
            .CompareOperation(comparison, r0, Strength::WEAK)
            .Return();

        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
        InterpreterTester tester(handles.main_isolate(), bytecode_array);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        CHECK(return_value->IsBoolean());
        CHECK_EQ(return_value->BooleanValue(),
                 CompareC(comparison, inputs[i], inputs[j]));
      }
    }
  }
}


TEST(InterpreterMixedComparisons) {
  // This test compares a HeapNumber with a String. The latter is
  // convertible to a HeapNumber so comparison will be between numeric
  // values except for the strict comparisons where no conversion is
  // performed.
  const char* inputs[] = {"-1.77", "-40.333", "0.01", "55.77e5", "2.01"};

  i::UnicodeCache unicode_cache;

  for (size_t c = 0; c < arraysize(kComparisonTypes); c++) {
    Token::Value comparison = kComparisonTypes[c];
    for (size_t i = 0; i < arraysize(inputs); i++) {
      for (size_t j = 0; j < arraysize(inputs); j++) {
        for (int pass = 0; pass < 2; pass++) {
          const char* lhs_cstr = inputs[i];
          const char* rhs_cstr = inputs[j];
          double lhs = StringToDouble(&unicode_cache, lhs_cstr,
                                      i::ConversionFlags::NO_FLAGS);
          double rhs = StringToDouble(&unicode_cache, rhs_cstr,
                                      i::ConversionFlags::NO_FLAGS);
          HandleAndZoneScope handles;
          i::Factory* factory = handles.main_isolate()->factory();
          BytecodeArrayBuilder builder(handles.main_isolate(),
                                       handles.main_zone());
          Register r0(0);
          builder.set_locals_count(1);
          builder.set_context_count(0);
          builder.set_parameter_count(0);
          if (pass == 0) {
            // Comparison with HeapNumber on the lhs and String on the rhs
            builder.LoadLiteral(factory->NewNumber(lhs))
                .StoreAccumulatorInRegister(r0)
                .LoadLiteral(factory->NewStringFromAsciiChecked(rhs_cstr))
                .CompareOperation(comparison, r0, Strength::WEAK)
                .Return();
          } else {
            // Comparison with HeapNumber on the rhs and String on the lhs
            builder.LoadLiteral(factory->NewStringFromAsciiChecked(lhs_cstr))
                .StoreAccumulatorInRegister(r0)
                .LoadLiteral(factory->NewNumber(rhs))
                .CompareOperation(comparison, r0, Strength::WEAK)
                .Return();
          }

          Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
          InterpreterTester tester(handles.main_isolate(), bytecode_array);
          auto callable = tester.GetCallable<>();
          Handle<Object> return_value = callable().ToHandleChecked();
          CHECK(return_value->IsBoolean());
          CHECK_EQ(return_value->BooleanValue(),
                   CompareC(comparison, lhs, rhs, true));
        }
      }
    }
  }
}


TEST(InterpreterInstanceOf) {
  HandleAndZoneScope handles;
  i::Factory* factory = handles.main_isolate()->factory();
  Handle<i::String> name = factory->NewStringFromAsciiChecked("cons");
  Handle<i::JSFunction> func = factory->NewFunction(name);
  Handle<i::JSObject> instance = factory->NewJSObject(func);
  Handle<i::Object> other = factory->NewNumber(3.3333);
  Handle<i::Object> cases[] = {Handle<i::Object>::cast(instance), other};
  for (size_t i = 0; i < arraysize(cases); i++) {
    bool expected_value = (i == 0);
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    Register r0(0);
    builder.set_locals_count(1);
    builder.set_context_count(0);
    builder.set_parameter_count(0);
    builder.LoadLiteral(cases[i]);
    builder.StoreAccumulatorInRegister(r0)
        .LoadLiteral(func)
        .CompareOperation(Token::Value::INSTANCEOF, r0, Strength::WEAK)
        .Return();

    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->IsBoolean());
    CHECK_EQ(return_value->BooleanValue(), expected_value);
  }
}


TEST(InterpreterTestIn) {
  HandleAndZoneScope handles;
  i::Factory* factory = handles.main_isolate()->factory();
  // Allocate an array
  Handle<i::JSArray> array =
      factory->NewJSArray(i::ElementsKind::FAST_SMI_ELEMENTS);
  // Check for these properties on the array object
  const char* properties[] = {"length", "fuzzle", "x", "0"};
  for (size_t i = 0; i < arraysize(properties); i++) {
    bool expected_value = (i == 0);
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    Register r0(0);
    builder.set_locals_count(1);
    builder.set_context_count(0);
    builder.set_parameter_count(0);
    builder.LoadLiteral(factory->NewStringFromAsciiChecked(properties[i]))
        .StoreAccumulatorInRegister(r0)
        .LoadLiteral(Handle<Object>::cast(array))
        .CompareOperation(Token::Value::IN, r0, Strength::WEAK)
        .Return();

    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->IsBoolean());
    CHECK_EQ(return_value->BooleanValue(), expected_value);
  }
}


TEST(InterpreterUnaryNot) {
  HandleAndZoneScope handles;
  for (size_t i = 1; i < 10; i++) {
    bool expected_value = ((i & 1) == 1);
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    Register r0(0);
    builder.set_locals_count(0);
    builder.set_context_count(0);
    builder.set_parameter_count(0);
    builder.LoadFalse();
    for (size_t j = 0; j < i; j++) {
      builder.LogicalNot();
    }
    builder.Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->IsBoolean());
    CHECK_EQ(return_value->BooleanValue(), expected_value);
  }
}


static void LoadAny(BytecodeArrayBuilder* builder,
                    v8::internal::Factory* factory, Handle<Object> obj) {
  if (obj->IsOddball()) {
    if (obj->SameValue(*factory->true_value())) {
      builder->LoadTrue();
    } else if (obj->SameValue(*factory->false_value())) {
      builder->LoadFalse();
    } else if (obj->SameValue(*factory->the_hole_value())) {
      builder->LoadTheHole();
    } else if (obj->SameValue(*factory->null_value())) {
      builder->LoadNull();
    } else if (obj->SameValue(*factory->undefined_value())) {
      builder->LoadUndefined();
    } else {
      UNREACHABLE();
    }
  } else if (obj->IsSmi()) {
    builder->LoadLiteral(*Handle<Smi>::cast(obj));
  } else {
    builder->LoadLiteral(obj);
  }
}


TEST(InterpreterUnaryNotNonBoolean) {
  HandleAndZoneScope handles;
  i::Factory* factory = handles.main_isolate()->factory();

  std::pair<Handle<Object>, bool> object_type_tuples[] = {
      std::make_pair(factory->undefined_value(), true),
      std::make_pair(factory->null_value(), true),
      std::make_pair(factory->false_value(), true),
      std::make_pair(factory->true_value(), false),
      std::make_pair(factory->NewNumber(9.1), false),
      std::make_pair(factory->NewNumberFromInt(0), true),
      std::make_pair(
          Handle<Object>::cast(factory->NewStringFromStaticChars("hello")),
          false),
      std::make_pair(
          Handle<Object>::cast(factory->NewStringFromStaticChars("")), true),
  };

  for (size_t i = 0; i < arraysize(object_type_tuples); i++) {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    Register r0(0);
    builder.set_locals_count(0);
    builder.set_context_count(0);
    builder.set_parameter_count(0);
    LoadAny(&builder, factory, object_type_tuples[i].first);
    builder.LogicalNot();
    builder.Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->IsBoolean());
    CHECK_EQ(return_value->BooleanValue(), object_type_tuples[i].second);
  }
}


TEST(InterpreterTypeof) {
  HandleAndZoneScope handles;

  std::pair<const char*, const char*> typeof_vals[] = {
      std::make_pair("return typeof undefined;", "undefined"),
      std::make_pair("return typeof null;", "object"),
      std::make_pair("return typeof true;", "boolean"),
      std::make_pair("return typeof false;", "boolean"),
      std::make_pair("return typeof 9.1;", "number"),
      std::make_pair("return typeof 7771;", "number"),
      std::make_pair("return typeof 'hello';", "string"),
      std::make_pair("return typeof global_unallocated;", "undefined"),
  };

  for (size_t i = 0; i < arraysize(typeof_vals); i++) {
    std::string source(InterpreterTester::SourceForBody(typeof_vals[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());

    auto callable = tester.GetCallable<>();
    Handle<v8::internal::String> return_value =
        Handle<v8::internal::String>::cast(callable().ToHandleChecked());
    auto actual = return_value->ToCString();
    CHECK_EQ(strcmp(&actual[0], typeof_vals[i].second), 0);
  }
}


TEST(InterpreterCallRuntime) {
  HandleAndZoneScope handles;

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(2);
  builder.set_context_count(0);
  builder.set_parameter_count(1);
  builder.LoadLiteral(Smi::FromInt(15))
      .StoreAccumulatorInRegister(Register(0))
      .LoadLiteral(Smi::FromInt(40))
      .StoreAccumulatorInRegister(Register(1))
      .CallRuntime(Runtime::kAdd, Register(0), 2)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(55));
}


TEST(InterpreterFunctionLiteral) {
  HandleAndZoneScope handles;

  // Test calling a function literal.
  std::string source(
      "function " + InterpreterTester::function_name() + "(a) {\n"
      "  return (function(x){ return x + 2; })(a);\n"
      "}");
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<Handle<Object>>();

  Handle<i::Object> return_val = callable(
      Handle<Smi>(Smi::FromInt(3), handles.main_isolate())).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(5));
}


TEST(InterpreterRegExpLiterals) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("return /abd/.exec('cccabbdd');\n",
                     factory->null_value()),
      std::make_pair("return /ab+d/.exec('cccabbdd')[0];\n",
                     factory->NewStringFromStaticChars("abbd")),
      std::make_pair("return /AbC/i.exec('ssaBC')[0];\n",
                     factory->NewStringFromStaticChars("aBC")),
      std::make_pair("return 'ssaBC'.match(/AbC/i)[0];\n",
                     factory->NewStringFromStaticChars("aBC")),
      std::make_pair("return 'ssaBCtAbC'.match(/(AbC)/gi)[1];\n",
                     factory->NewStringFromStaticChars("AbC")),
  };

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}


TEST(InterpreterArrayLiterals) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("return [][0];\n",
                     factory->undefined_value()),
      std::make_pair("return [1, 3, 2][1];\n",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("return ['a', 'b', 'c'][2];\n",
                     factory->NewStringFromStaticChars("c")),
      std::make_pair("var a = 100; return [a, a + 1, a + 2, a + 3][2];\n",
                     handle(Smi::FromInt(102), isolate)),
      std::make_pair("return [[1, 2, 3], ['a', 'b', 'c']][1][0];\n",
                     factory->NewStringFromStaticChars("a")),
      std::make_pair("var t = 't'; return [[t, t + 'est'], [1 + t]][0][1];\n",
                     factory->NewStringFromStaticChars("test"))
  };

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}


TEST(InterpreterObjectLiterals) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("return { }.name;",
                     factory->undefined_value()),
      std::make_pair("return { name: 'string', val: 9.2 }.name;",
                     factory->NewStringFromStaticChars("string")),
      std::make_pair("var a = 15; return { name: 'string', val: a }.val;",
                     handle(Smi::FromInt(15), isolate)),
      std::make_pair("var a = 5; return { val: a, val: a + 1 }.val;",
                     handle(Smi::FromInt(6), isolate)),
      std::make_pair("return { func: function() { return 'test' } }.func();",
                     factory->NewStringFromStaticChars("test")),
      std::make_pair("return { func(a) { return a + 'st'; } }.func('te');",
                     factory->NewStringFromStaticChars("test")),
      std::make_pair("return { get a() { return 22; } }.a;",
                     handle(Smi::FromInt(22), isolate)),
      std::make_pair("var a = { get b() { return this.x + 't'; },\n"
                     "          set b(val) { this.x = val + 's' } };\n"
                     "a.b = 'te';\n"
                     "return a.b;",
                     factory->NewStringFromStaticChars("test")),
      std::make_pair("var a = 123; return { 1: a }[1];",
                     handle(Smi::FromInt(123), isolate)),
      std::make_pair("return Object.getPrototypeOf({ __proto__: null });",
                     factory->null_value()),
      std::make_pair("var a = 'test'; return { [a]: 1 }.test;",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var a = 'test'; return { b: a, [a]: a + 'ing' }['test']",
                     factory->NewStringFromStaticChars("testing")),
      std::make_pair("var a = 'proto_str';\n"
                     "var b = { [a]: 1, __proto__: { var : a } };\n"
                     "return Object.getPrototypeOf(b).var",
                     factory->NewStringFromStaticChars("proto_str")),
      std::make_pair("var n = 'name';\n"
                     "return { [n]: 'val', get a() { return 987 } }['a'];",
                     handle(Smi::FromInt(987), isolate)),
  };

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}


TEST(InterpreterConstruct) {
  HandleAndZoneScope handles;

  std::string source(
      "function counter() { this.count = 0; }\n"
      "function " +
      InterpreterTester::function_name() +
      "() {\n"
      "  var c = new counter();\n"
      "  return c.count;\n"
      "}");
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(0));
}


TEST(InterpreterConstructWithArgument) {
  HandleAndZoneScope handles;

  std::string source(
      "function counter(arg0) { this.count = 17; this.x = arg0; }\n"
      "function " +
      InterpreterTester::function_name() +
      "() {\n"
      "  var c = new counter(3);\n"
      "  return c.x;\n"
      "}");
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(3));
}


TEST(InterpreterConstructWithArguments) {
  HandleAndZoneScope handles;

  std::string source(
      "function counter(arg0, arg1) {\n"
      "  this.count = 7; this.x = arg0; this.y = arg1;\n"
      "}\n"
      "function " +
      InterpreterTester::function_name() +
      "() {\n"
      "  var c = new counter(3, 5);\n"
      "  return c.count + c.x + c.y;\n"
      "}");
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(15));
}


TEST(InterpreterContextVariables) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();

  std::ostringstream unique_vars;
  for (int i = 0; i < 250; i++) {
    unique_vars << "var a" << i << " = 0;";
  }
  std::pair<std::string, Handle<Object>> context_vars[] = {
      std::make_pair("var a; (function() { a = 1; })(); return a;",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var a = 10; (function() { a; })(); return a;",
                     handle(Smi::FromInt(10), isolate)),
      std::make_pair("var a = 20; var b = 30;\n"
                     "return (function() { return a + b; })();",
                     handle(Smi::FromInt(50), isolate)),
      std::make_pair("'use strict'; let a = 1;\n"
                     "{ let b = 2; return (function() { return a + b; })(); }",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("'use strict'; let a = 10;\n"
                     "{ let b = 20; var c = function() { [a, b] };\n"
                     "  return a + b; }",
                     handle(Smi::FromInt(30), isolate)),
      std::make_pair("'use strict';" + unique_vars.str() +
                     "eval(); var b = 100; return b;",
                     handle(Smi::FromInt(100), isolate)),
  };

  for (size_t i = 0; i < arraysize(context_vars); i++) {
    std::string source(
        InterpreterTester::SourceForBody(context_vars[i].first.c_str()));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*context_vars[i].second));
  }
}


TEST(InterpreterContextParameters) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> context_params[] = {
      std::make_pair("return (function() { return arg1; })();",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("(function() { arg1 = 4; })(); return arg1;",
                     handle(Smi::FromInt(4), isolate)),
      std::make_pair("(function() { arg3 = arg2 - arg1; })(); return arg3;",
                     handle(Smi::FromInt(1), isolate)),
  };

  for (size_t i = 0; i < arraysize(context_params); i++) {
    std::string source = "function " + InterpreterTester::function_name() +
                         "(arg1, arg2, arg3) {" + context_params[i].first + "}";
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable =
        tester.GetCallable<Handle<Object>, Handle<Object>, Handle<Object>>();

    Handle<Object> a1 = handle(Smi::FromInt(1), isolate);
    Handle<Object> a2 = handle(Smi::FromInt(2), isolate);
    Handle<Object> a3 = handle(Smi::FromInt(3), isolate);
    Handle<i::Object> return_value = callable(a1, a2, a3).ToHandleChecked();
    CHECK(return_value->SameValue(*context_params[i].second));
  }
}


TEST(InterpreterOuterContextVariables) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> context_vars[] = {
      std::make_pair("return outerVar * innerArg;",
                     handle(Smi::FromInt(200), isolate)),
      std::make_pair("outerVar = innerArg; return outerVar",
                     handle(Smi::FromInt(20), isolate)),
  };

  std::string header(
      "function Outer() {"
      "  var outerVar = 10;"
      "  function Inner(innerArg) {"
      "    this.innerFunc = function() { ");
  std::string footer(
      "  }}"
      "  this.getInnerFunc = function() { return new Inner(20).innerFunc; }"
      "}"
      "var f = new Outer().getInnerFunc();");

  for (size_t i = 0; i < arraysize(context_vars); i++) {
    std::string source = header + context_vars[i].first + footer;
    InterpreterTester tester(handles.main_isolate(), source.c_str(), "*");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*context_vars[i].second));
  }
}


TEST(InterpreterComma) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("var a; return 0, a;\n", factory->undefined_value()),
      std::make_pair("return 'a', 2.2, 3;\n",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("return 'a', 'b', 'c';\n",
                     factory->NewStringFromStaticChars("c")),
      std::make_pair("return 3.2, 2.3, 4.5;\n", factory->NewNumber(4.5)),
      std::make_pair("var a = 10; return b = a, b = b+1;\n",
                     handle(Smi::FromInt(11), isolate)),
      std::make_pair("var a = 10; return b = a, b = b+1, b + 10;\n",
                     handle(Smi::FromInt(21), isolate))};

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}


TEST(InterpreterLogicalOr) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("var a, b; return a || b;\n", factory->undefined_value()),
      std::make_pair("var a, b = 10; return a || b;\n",
                     handle(Smi::FromInt(10), isolate)),
      std::make_pair("var a = '0', b = 10; return a || b;\n",
                     factory->NewStringFromStaticChars("0")),
      std::make_pair("return 0 || 3.2;\n", factory->NewNumber(3.2)),
      std::make_pair("return 'a' || 0;\n",
                     factory->NewStringFromStaticChars("a")),
      std::make_pair("var a = '0', b = 10; return (a == 0) || b;\n",
                     factory->true_value())};

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}


TEST(InterpreterLogicalAnd) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("var a, b = 10; return a && b;\n",
                     factory->undefined_value()),
      std::make_pair("var a = 0, b = 10; return a && b / a;\n",
                     handle(Smi::FromInt(0), isolate)),
      std::make_pair("var a = '0', b = 10; return a && b;\n",
                     handle(Smi::FromInt(10), isolate)),
      std::make_pair("return 0.0 && 3.2;\n", handle(Smi::FromInt(0), isolate)),
      std::make_pair("return 'a' && 'b';\n",
                     factory->NewStringFromStaticChars("b")),
      std::make_pair("return 'a' && 0 || 'b', 'c';\n",
                     factory->NewStringFromStaticChars("c")),
      std::make_pair("var x = 1, y = 3; return x && 0 + 1 || y;\n",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var x = 1, y = 3; return (x == 1) && (3 == 3) || y;\n",
                     factory->true_value())};

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}


TEST(InterpreterTryCatch) {
  HandleAndZoneScope handles;

  // TODO(rmcilroy): modify tests when we have real try catch support.
  std::string source(InterpreterTester::SourceForBody(
      "var a = 1; try { a = a + 1; } catch(e) { a = a + 2; }; return a;"));
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(2));
}


TEST(InterpreterTryFinally) {
  HandleAndZoneScope handles;

  // TODO(rmcilroy): modify tests when we have real try finally support.
  std::string source(InterpreterTester::SourceForBody(
      "var a = 1; try { a = a + 1; } finally { a = a + 2; }; return a;"));
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(4));
}


TEST(InterpreterThrow) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  // TODO(rmcilroy): modify tests when we have real try catch support.
  std::pair<const char*, Handle<Object>> throws[] = {
      std::make_pair("throw undefined;\n",
                     factory->undefined_value()),
      std::make_pair("throw 1;\n",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("throw 'Error';\n",
                     factory->NewStringFromStaticChars("Error")),
      std::make_pair("var a = true; if (a) { throw 'Error'; }\n",
                     factory->NewStringFromStaticChars("Error")),
      std::make_pair("var a = false; if (a) { throw 'Error'; }\n",
                     factory->undefined_value()),
      std::make_pair("throw 'Error1'; throw 'Error2'\n",
                     factory->NewStringFromStaticChars("Error1")),
  };

  const char* try_wrapper =
      "(function() { try { f(); } catch(e) { return e; }})()";

  for (size_t i = 0; i < arraysize(throws); i++) {
    std::string source(InterpreterTester::SourceForBody(throws[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    tester.GetCallable<>();
    Handle<Object> thrown_obj = v8::Utils::OpenHandle(*CompileRun(try_wrapper));
    CHECK(thrown_obj->SameValue(*throws[i].second));
  }
}


TEST(InterpreterCountOperators) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> count_ops[] = {
      std::make_pair("var a = 1; return ++a;",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var a = 1; return a++;",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var a = 5; return --a;",
                     handle(Smi::FromInt(4), isolate)),
      std::make_pair("var a = 5; return a--;",
                     handle(Smi::FromInt(5), isolate)),
      std::make_pair("var a = 5.2; return --a;",
                     factory->NewHeapNumber(4.2)),
      std::make_pair("var a = 'string'; return ++a;",
                     factory->nan_value()),
      std::make_pair("var a = 'string'; return a--;",
                     factory->nan_value()),
      std::make_pair("var a = true; return ++a;",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var a = false; return a--;",
                     handle(Smi::FromInt(0), isolate)),
      std::make_pair("var a = { val: 11 }; return ++a.val;",
                     handle(Smi::FromInt(12), isolate)),
      std::make_pair("var a = { val: 11 }; return a.val--;",
                     handle(Smi::FromInt(11), isolate)),
      std::make_pair("var a = { val: 11 }; return ++a.val;",
                     handle(Smi::FromInt(12), isolate)),
      std::make_pair("var name = 'val'; var a = { val: 22 }; return --a[name];",
                     handle(Smi::FromInt(21), isolate)),
      std::make_pair("var name = 'val'; var a = { val: 22 }; return a[name]++;",
                     handle(Smi::FromInt(22), isolate)),
      std::make_pair("var a = 1; (function() { a = 2 })(); return ++a;",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 1; (function() { a = 2 })(); return a--;",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var i = 5; while(i--) {}; return i;",
                     handle(Smi::FromInt(-1), isolate)),
      std::make_pair("var i = 1; if(i--) { return 1; } else { return 2; };",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var i = -2; do {} while(i++) {}; return i;",
                      handle(Smi::FromInt(1), isolate)),
      std::make_pair("var i = -1; for(; i++; ) {}; return i",
                      handle(Smi::FromInt(1), isolate)),
      std::make_pair("var i = 20; switch(i++) {\n"
                     "  case 20: return 1;\n"
                     "  default: return 2;\n"
                     "}",
                     handle(Smi::FromInt(1), isolate)),
  };

  for (size_t i = 0; i < arraysize(count_ops); i++) {
    std::string source(InterpreterTester::SourceForBody(count_ops[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*count_ops[i].second));
  }
}


TEST(InterpreterGlobalCountOperators) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> count_ops[] = {
      std::make_pair("var global = 100;function f(){ return ++global; }",
                     handle(Smi::FromInt(101), isolate)),
      std::make_pair("var global = 100; function f(){ return --global; }",
                     handle(Smi::FromInt(99), isolate)),
      std::make_pair("var global = 100; function f(){ return global++; }",
                     handle(Smi::FromInt(100), isolate)),
      std::make_pair("unallocated = 200; function f(){ return ++unallocated; }",
                     handle(Smi::FromInt(201), isolate)),
      std::make_pair("unallocated = 200; function f(){ return --unallocated; }",
                     handle(Smi::FromInt(199), isolate)),
      std::make_pair("unallocated = 200; function f(){ return unallocated++; }",
                     handle(Smi::FromInt(200), isolate)),
  };

  for (size_t i = 0; i < arraysize(count_ops); i++) {
    InterpreterTester tester(handles.main_isolate(), count_ops[i].first);
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*count_ops[i].second));
  }
}


TEST(InterpreterCompoundExpressions) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> compound_expr[] = {
      std::make_pair("var a = 1; a += 2; return a;",
                     Handle<Object>(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 10; a /= 2; return a;",
                     Handle<Object>(Smi::FromInt(5), isolate)),
      std::make_pair("var a = 'test'; a += 'ing'; return a;",
                     factory->NewStringFromStaticChars("testing")),
      std::make_pair("var a = { val: 2 }; a.val *= 2; return a.val;",
                     Handle<Object>(Smi::FromInt(4), isolate)),
      std::make_pair("var a = 1; (function f() { a = 2; })(); a += 24;"
                     "return a;",
                     Handle<Object>(Smi::FromInt(26), isolate)),
  };

  for (size_t i = 0; i < arraysize(compound_expr); i++) {
    std::string source(
        InterpreterTester::SourceForBody(compound_expr[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*compound_expr[i].second));
  }
}


TEST(InterpreterGlobalCompoundExpressions) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> compound_expr[2] = {
      std::make_pair("var global = 100;"
                     "function f() { global += 20; return global; }",
                     Handle<Object>(Smi::FromInt(120), isolate)),
      std::make_pair("unallocated = 100;"
                     "function f() { unallocated -= 20; return unallocated; }",
                     Handle<Object>(Smi::FromInt(80), isolate)),
  };

  for (size_t i = 0; i < arraysize(compound_expr); i++) {
    InterpreterTester tester(handles.main_isolate(), compound_expr[i].first);
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*compound_expr[i].second));
  }
}


TEST(InterpreterCreateArguments) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, int> create_args[] = {
      std::make_pair("function f() { return arguments[0]; }", 0),
      std::make_pair("function f(a) { return arguments[0]; }", 0),
      std::make_pair("function f() { return arguments[2]; }", 2),
      std::make_pair("function f(a) { return arguments[2]; }", 2),
      std::make_pair("function f(a, b, c, d) { return arguments[2]; }", 2),
      std::make_pair("function f(a) {"
                     "'use strict'; return arguments[0]; }",
                     0),
      std::make_pair("function f(a, b, c, d) {"
                     "'use strict'; return arguments[2]; }",
                     2),
      // Check arguments are mapped in sloppy mode and unmapped in strict.
      std::make_pair("function f(a, b, c, d) {"
                     "  c = b; return arguments[2]; }",
                     1),
      std::make_pair("function f(a, b, c, d) {"
                     "  'use strict'; c = b; return arguments[2]; }",
                     2),
  };

  // Test passing no arguments.
  for (size_t i = 0; i < arraysize(create_args); i++) {
    InterpreterTester tester(handles.main_isolate(), create_args[i].first);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK(return_val.is_identical_to(factory->undefined_value()));
  }

  // Test passing one argument.
  for (size_t i = 0; i < arraysize(create_args); i++) {
    InterpreterTester tester(handles.main_isolate(), create_args[i].first);
    auto callable = tester.GetCallable<Handle<Object>>();
    Handle<Object> return_val =
        callable(handle(Smi::FromInt(40), isolate)).ToHandleChecked();
    if (create_args[i].second == 0) {
      CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(40));
    } else {
      CHECK(return_val.is_identical_to(factory->undefined_value()));
    }
  }

  // Test passing three argument.
  for (size_t i = 0; i < arraysize(create_args); i++) {
    Handle<Object> args[3] = {
        handle(Smi::FromInt(40), isolate),
        handle(Smi::FromInt(60), isolate),
        handle(Smi::FromInt(80), isolate),
    };

    InterpreterTester tester(handles.main_isolate(), create_args[i].first);
    auto callable =
        tester.GetCallable<Handle<Object>, Handle<Object>, Handle<Object>>();
    Handle<Object> return_val =
        callable(args[0], args[1], args[2]).ToHandleChecked();
    CHECK(return_val->SameValue(*args[create_args[i].second]));
  }
}


TEST(InterpreterConditional) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> conditional[] = {
      std::make_pair("return true ? 2 : 3;",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("return false ? 2 : 3;",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 1; return a ? 20 : 30;",
                     handle(Smi::FromInt(20), isolate)),
      std::make_pair("var a = 1; return a ? 20 : 30;",
                     handle(Smi::FromInt(20), isolate)),
      std::make_pair("var a = 'string'; return a ? 20 : 30;",
                     handle(Smi::FromInt(20), isolate)),
      std::make_pair("var a = undefined; return a ? 20 : 30;",
                     handle(Smi::FromInt(30), isolate)),
      std::make_pair("return 1 ? 2 ? 3 : 4 : 5;",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("return 0 ? 2 ? 3 : 4 : 5;",
                     handle(Smi::FromInt(5), isolate)),
  };

  for (size_t i = 0; i < arraysize(conditional); i++) {
    std::string source(InterpreterTester::SourceForBody(conditional[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*conditional[i].second));
  }
}


TEST(InterpreterDelete) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  // Tests for delete for local variables that work both in strict
  // and sloppy modes
  std::pair<const char*, Handle<Object>> test_delete[] = {
      std::make_pair(
          "var a = { x:10, y:'abc', z:30.2}; delete a.x; return a.x;\n",
          factory->undefined_value()),
      std::make_pair(
          "var b = { x:10, y:'abc', z:30.2}; delete b.x; return b.y;\n",
          factory->NewStringFromStaticChars("abc")),
      std::make_pair("var c = { x:10, y:'abc', z:30.2}; var d = c; delete d.x; "
                     "return c.x;\n",
                     factory->undefined_value()),
      std::make_pair("var e = { x:10, y:'abc', z:30.2}; var g = e; delete g.x; "
                     "return e.y;\n",
                     factory->NewStringFromStaticChars("abc")),
      std::make_pair("var a = { x:10, y:'abc', z:30.2};\n"
                     "var b = a;"
                     "delete b.x;"
                     "return b.x;\n",
                     factory->undefined_value()),
      std::make_pair("var a = {1:10};\n"
                     "(function f1() {return a;});"
                     "return delete a[1];",
                     factory->ToBoolean(true)),
      std::make_pair("return delete this;", factory->ToBoolean(true)),
      std::make_pair("return delete 'test';", factory->ToBoolean(true))};

  // Test delete in sloppy mode
  for (size_t i = 0; i < arraysize(test_delete); i++) {
    std::string source(InterpreterTester::SourceForBody(test_delete[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*test_delete[i].second));
  }

  // Test delete in strict mode
  for (size_t i = 0; i < arraysize(test_delete); i++) {
    std::string strict_test =
        "'use strict'; " + std::string(test_delete[i].first);
    std::string source(InterpreterTester::SourceForBody(strict_test.c_str()));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*test_delete[i].second));
  }
}


TEST(InterpreterDeleteSloppyUnqualifiedIdentifier) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  // These tests generate a syntax error for strict mode. We don't
  // test for it here.
  std::pair<const char*, Handle<Object>> test_delete[] = {
      std::make_pair("var sloppy_a = { x:10, y:'abc'};\n"
                     "var sloppy_b = delete sloppy_a;\n"
                     "if (delete sloppy_a) {\n"
                     "  return undefined;\n"
                     "} else {\n"
                     "  return sloppy_a.x;\n"
                     "}\n",
                     Handle<Object>(Smi::FromInt(10), isolate)),
      // TODO(mythria) When try-catch is implemented change the tests to check
      // if delete actually deletes
      std::make_pair("sloppy_a = { x:10, y:'abc'};\n"
                     "var sloppy_b = delete sloppy_a;\n"
                     // "try{return a.x;} catch(e) {return b;}\n"
                     "return sloppy_b;",
                     factory->ToBoolean(true)),
      std::make_pair("sloppy_a = { x:10, y:'abc'};\n"
                     "var sloppy_b = delete sloppy_c;\n"
                     "return sloppy_b;",
                     factory->ToBoolean(true))};

  for (size_t i = 0; i < arraysize(test_delete); i++) {
    std::string source(InterpreterTester::SourceForBody(test_delete[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*test_delete[i].second));
  }
}


TEST(InterpreterGlobalDelete) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> test_global_delete[] = {
      std::make_pair("var a = { x:10, y:'abc', z:30.2 };\n"
                     "function f() {\n"
                     "  delete a.x;\n"
                     "  return a.x;\n"
                     "}\n"
                     "f();\n",
                     factory->undefined_value()),
      std::make_pair("var b = {1:10, 2:'abc', 3:30.2 };\n"
                     "function f() {\n"
                     "  delete b[2];\n"
                     "  return b[1];\n"
                     " }\n"
                     "f();\n",
                     Handle<Object>(Smi::FromInt(10), isolate)),
      std::make_pair("var c = { x:10, y:'abc', z:30.2 };\n"
                     "function f() {\n"
                     "   var d = c;\n"
                     "   delete d.y;\n"
                     "   return d.x;\n"
                     "}\n"
                     "f();\n",
                     Handle<Object>(Smi::FromInt(10), isolate)),
      std::make_pair("e = { x:10, y:'abc' };\n"
                     "function f() {\n"
                     "  return delete e;\n"
                     "}\n"
                     "f();\n",
                     factory->ToBoolean(true)),
      std::make_pair("var g = { x:10, y:'abc' };\n"
                     "function f() {\n"
                     "  return delete g;\n"
                     "}\n"
                     "f();\n",
                     factory->ToBoolean(false)),
      std::make_pair("function f() {\n"
                     "  var obj = {h:10, f1() {return delete this;}};\n"
                     "  return obj.f1();\n"
                     "}\n"
                     "f();",
                     factory->ToBoolean(true)),
      std::make_pair("function f() {\n"
                     "  var obj = {h:10,\n"
                     "             f1() {\n"
                     "              'use strict';\n"
                     "              return delete this.h;}};\n"
                     "  return obj.f1();\n"
                     "}\n"
                     "f();",
                     factory->ToBoolean(true))};

  for (size_t i = 0; i < arraysize(test_global_delete); i++) {
    InterpreterTester tester(handles.main_isolate(),
                             test_global_delete[i].first);
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*test_global_delete[i].second));
  }
}


TEST(InterpreterBasicLoops) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> loops[] = {
      std::make_pair("var a = 10; var b = 1;\n"
                     "while (a) {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "};\n"
                     "return b;\n",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 1; var b = 1;\n"
                     "do {\n"
                     "  b = b * 2;\n"
                     "  --a;\n"
                     "} while(a);\n"
                     "return b;\n",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var b = 1;\n"
                     "for ( var a = 10; a; a--) {\n"
                     "  b *= 2;\n"
                     "}\n"
                     "return b;",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "while (a > 0) {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "};\n"
                     "return b;\n",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 1; var b = 1;\n"
                     "do {\n"
                     "  b = b * 2;\n"
                     "  --a;\n"
                     "} while(a);\n"
                     "return b;\n",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var b = 1;\n"
                     "for ( var a = 10; a > 0; a--) {\n"
                     "  b *= 2;\n"
                     "}\n"
                     "return b;",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "while (false) {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "}\n"
                     "return b;\n",
                     Handle<Object>(Smi::FromInt(1), isolate)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "while (true) {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "  if (a == 0) break;"
                     "  continue;"
                     "}\n"
                     "return b;\n",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "do {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "  if (a == 0) break;"
                     "} while(true);\n"
                     "return b;\n",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "do {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "  if (a == 0) break;"
                     "} while(false);\n"
                     "return b;\n",
                     Handle<Object>(Smi::FromInt(2), isolate)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "for ( a = 1, b = 30; false; ) {\n"
                     "  b = b * 2;\n"
                     "}\n"
                     "return b;\n",
                     Handle<Object>(Smi::FromInt(30), isolate))};

  for (size_t i = 0; i < arraysize(loops); i++) {
    std::string source(InterpreterTester::SourceForBody(loops[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*loops[i].second));
  }
}


TEST(InterpreterForIn) {
  HandleAndZoneScope handles;

  std::pair<const char*, int> for_in_samples[] = {
      {"function f() {\n"
       "  var r = -1;\n"
       "  for (var a in null) { r = a; }\n"
       "  return r;\n"
       "}",
       -1},
      {"function f() {\n"
       "  var r = -1;\n"
       "  for (var a in undefined) { r = a; }\n"
       "  return r;\n"
       "}",
       -1},
      {"function f() {\n"
       "  var r = 0;\n"
       "  for (var a in [0,6,7,9]) { r = r + (1 << a); }\n"
       "  return r;\n"
       "}",
       0xf},
      {"function f() {\n"
       "  var r = 0;\n"
       "  for (var a in [0,6,7,9]) { r = r + (1 << a); }\n"
       "  var r = 0;\n"
       "  for (var a in [0,6,7,9]) { r = r + (1 << a); }\n"
       "  return r;\n"
       "}",
       0xf},
      {"function f() {\n"
       "  var r = 0;\n"
       "  for (var a in 'foobar') { r = r + (1 << a); }\n"
       "  return r;\n"
       "}",
       0x3f},
      {"function f() {\n"
       "  var r = 0;\n"
       "  for (var a in {1:0, 10:1, 100:2, 1000:3}) {\n"
       "    r = r + Number(a);\n"
       "   }\n"
       "   return r;\n"
       "}",
       1111},
      {"function f() {\n"
       "  var r = 0;\n"
       "  var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "  for (var a in data) {\n"
       "    if (a == 1) delete data[1];\n"
       "    r = r + Number(a);\n"
       "   }\n"
       "   return r;\n"
       "}",
       1111},
      {"function f() {\n"
       "  var r = 0;\n"
       "  var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "  for (var a in data) {\n"
       "    if (a == 10) delete data[100];\n"
       "    r = r + Number(a);\n"
       "   }\n"
       "   return r;\n"
       "}",
       1011},
      {"function f() {\n"
       "  var r = 0;\n"
       "  var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "  for (var a in data) {\n"
       "    if (a == 10) data[10000] = 4;\n"
       "    r = r + Number(a);\n"
       "   }\n"
       "   return r;\n"
       "}",
       1111},
      {"function f() {\n"
       "  var r = 0;\n"
       "  var input = 'foobar';\n"
       "  for (var a in input) {\n"
       "    if (input[a] == 'b') break;\n"
       "    r = r + (1 << a);\n"
       "  }\n"
       "  return r;\n"
       "}",
       0x7},
      {"function f() {\n"
       "var r = 0;\n"
       "var input = 'foobar';\n"
       "for (var a in input) {\n"
       "   if (input[a] == 'b') continue;\n"
       "   r = r + (1 << a);\n"
       "}\n"
       "return r;\n"
       "}",
       0x37},
      {"function f() {\n"
       "  var r = 0;\n"
       "  var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "  for (var a in data) {\n"
       "    if (a == 10) {\n"
       "       data[10000] = 4;\n"
       "    }\n"
       "    r = r + Number(a);\n"
       "  }\n"
       "  return r;\n"
       "}",
       1111},
      {"function f() {\n"
       "  var r = [ 3 ];\n"
       "  var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "  for (r[10] in data) {\n"
       "  }\n"
       "  return Number(r[10]);\n"
       "}",
       1000},
      {"function f() {\n"
       "  var r = [ 3 ];\n"
       "  var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "  for (r['100'] in data) {\n"
       "  }\n"
       "  return Number(r['100']);\n"
       "}",
       1000},
      {"function f() {\n"
       "  var obj = {}\n"
       "  var descObj = new Boolean(false);\n"
       "  var accessed = 0;\n"
       "  descObj.enumerable = true;\n"
       "  Object.defineProperties(obj, { prop:descObj });\n"
       "  for (var p in obj) {\n"
       "    if (p === 'prop') { accessed = 1; }\n"
       "  }\n"
       "  return accessed;"
       "}",
       1},
      {"function f() {\n"
       "  var appointment = {};\n"
       "  Object.defineProperty(appointment, 'startTime', {\n"
       "      value: 1001,\n"
       "      writable: false,\n"
       "      enumerable: false,\n"
       "      configurable: true\n"
       "  });\n"
       "  Object.defineProperty(appointment, 'name', {\n"
       "      value: 'NAME',\n"
       "      writable: false,\n"
       "      enumerable: false,\n"
       "      configurable: true\n"
       "  });\n"
       "  var meeting = Object.create(appointment);\n"
       "  Object.defineProperty(meeting, 'conferenceCall', {\n"
       "      value: 'In-person meeting',\n"
       "      writable: false,\n"
       "      enumerable: false,\n"
       "      configurable: true\n"
       "  });\n"
       "\n"
       "  var teamMeeting = Object.create(meeting);\n"
       "\n"
       "  var flags = 0;\n"
       "  for (var p in teamMeeting) {\n"
       "      if (p === 'startTime') {\n"
       "          flags |= 1;\n"
       "      }\n"
       "      if (p === 'name') {\n"
       "          flags |= 2;\n"
       "      }\n"
       "      if (p === 'conferenceCall') {\n"
       "          flags |= 4;\n"
       "      }\n"
       "  }\n"
       "\n"
       "  var hasOwnProperty = !teamMeeting.hasOwnProperty('name') &&\n"
       "      !teamMeeting.hasOwnProperty('startTime') &&\n"
       "      !teamMeeting.hasOwnProperty('conferenceCall');\n"
       "  if (!hasOwnProperty) {\n"
       "      flags |= 8;\n"
       "  }\n"
       "  return flags;\n"
       "  }",
       0},
      {"function f() {\n"
       " var data = {x:23, y:34};\n"
       " var result = 0;\n"
       " var o = {};\n"
       " var arr = [o];\n"
       " for (arr[0].p in data)\n"      // This is to test if value is loaded
       "  result += data[arr[0].p];\n"  // back from accumulator before storing
       " return result;\n"              // named properties.
       "}",
       57},
      {"function f() {\n"
       " var data = {x:23, y:34};\n"
       " var result = 0;\n"
       " var o = {};\n"
       " var i = 0;\n"
       " for (o[i++] in data)\n"      // This is to test if value is loaded
       "  result += data[o[i-1]];\n"  // back from accumulator before
       " return result;\n"            // storing keyed properties.
       "}",
       57}};

  for (size_t i = 0; i < arraysize(for_in_samples); i++) {
    InterpreterTester tester(handles.main_isolate(), for_in_samples[i].first);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK_EQ(Handle<Smi>::cast(return_val)->value(), for_in_samples[i].second);
  }
}


TEST(InterpreterSwitch) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> switch_ops[] = {
      std::make_pair("var a = 1;\n"
                     "switch(a) {\n"
                     " case 1: return 2;\n"
                     " case 2: return 3;\n"
                     "}\n",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var a = 1;\n"
                     "switch(a) {\n"
                     " case 2: a = 2; break;\n"
                     " case 1: a = 3; break;\n"
                     "}\n"
                     "return a;",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 1;\n"
                     "switch(a) {\n"
                     " case 1: a = 2; // fall-through\n"
                     " case 2: a = 3; break;\n"
                     "}\n"
                     "return a;",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 100;\n"
                     "switch(a) {\n"
                     " case 1: return 100;\n"
                     " case 2: return 200;\n"
                     "}\n"
                     "return undefined;",
                     factory->undefined_value()),
      std::make_pair("var a = 100;\n"
                     "switch(a) {\n"
                     " case 1: return 100;\n"
                     " case 2: return 200;\n"
                     " default: return 300;\n"
                     "}\n"
                     "return undefined;",
                     handle(Smi::FromInt(300), isolate)),
      std::make_pair("var a = 100;\n"
                     "switch(typeof(a)) {\n"
                     " case 'string': return 1;\n"
                     " case 'number': return 2;\n"
                     " default: return 3;\n"
                     "}\n",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var a = 100;\n"
                     "switch(a) {\n"
                     " case a += 20: return 1;\n"
                     " case a -= 10: return 2;\n"
                     " case a -= 10: return 3;\n"
                     " default: return 3;\n"
                     "}\n",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 1;\n"
                     "switch(a) {\n"
                     " case 1: \n"
                     "   switch(a + 1) {\n"
                     "      case 2 : a += 1; break;\n"
                     "      default : a += 2; break;\n"
                     "   }  // fall-through\n"
                     " case 2: a += 3;\n"
                     "}\n"
                     "return a;",
                     handle(Smi::FromInt(5), isolate)),
  };

  for (size_t i = 0; i < arraysize(switch_ops); i++) {
    std::string source(InterpreterTester::SourceForBody(switch_ops[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*switch_ops[i].second));
  }
}


TEST(InterpreterSloppyThis) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> sloppy_this[] = {
      std::make_pair("var global_val = 100;\n"
                     "function f() { return this.global_val; }\n",
                     handle(Smi::FromInt(100), isolate)),
      std::make_pair("var global_val = 110;\n"
                     "function g() { return this.global_val; };"
                     "function f() { return g(); }\n",
                     handle(Smi::FromInt(110), isolate)),
      std::make_pair("var global_val = 110;\n"
                     "function g() { return this.global_val };"
                     "function f() { 'use strict'; return g(); }\n",
                     handle(Smi::FromInt(110), isolate)),
      std::make_pair("function f() { 'use strict'; return this; }\n",
                     factory->undefined_value()),
      std::make_pair("function g() { 'use strict'; return this; };"
                     "function f() { return g(); }\n",
                     factory->undefined_value()),
  };

  for (size_t i = 0; i < arraysize(sloppy_this); i++) {
    InterpreterTester tester(handles.main_isolate(), sloppy_this[i].first);
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*sloppy_this[i].second));
  }
}


TEST(InterpreterThisFunction) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  InterpreterTester tester(handles.main_isolate(),
                           "var f;\n f = function f() { return f.name; }");
  auto callable = tester.GetCallable<>();

  Handle<i::Object> return_value = callable().ToHandleChecked();
  CHECK(return_value->SameValue(*factory->NewStringFromStaticChars("f")));
}


TEST(InterpreterNewTarget) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  // TODO(rmcilroy): Add tests that we get the original constructor for
  // superclass constructors once we have class support.
  InterpreterTester tester(handles.main_isolate(),
                           "function f() { this.a = new.target; }");
  auto callable = tester.GetCallable<>();
  callable().ToHandleChecked();

  Handle<Object> new_target_name = v8::Utils::OpenHandle(
      *CompileRun("(function() { return (new f()).a.name; })();"));
  CHECK(new_target_name->SameValue(*factory->NewStringFromStaticChars("f")));
}


TEST(InterpreterAssignmentInExpressions) {
  HandleAndZoneScope handles;

  std::pair<const char*, int> samples[] = {
      {"function f() {\n"
       "  var x = 7;\n"
       "  var y = x + (x = 1) + (x = 2);\n"
       "  return y;\n"
       "}",
       10},
      {"function f() {\n"
       "  var x = 7;\n"
       "  var y = x + (x = 1) + (x = 2);\n"
       "  return x;\n"
       "}",
       2},
      {"function f() {\n"
       "  var x = 55;\n"
       "  x = x + (x = 100) + (x = 101);\n"
       "  return x;\n"
       "}",
       256},
      {"function f() {\n"
       "  var x = 7;\n"
       "  return ++x + x + x++;\n"
       "}",
       24},
      {"function f() {\n"
       "  var x = 7;\n"
       "  var y = 1 + ++x + x + x++;\n"
       "  return x;\n"
       "}",
       9},
      {"function f() {\n"
       "  var x = 7;\n"
       "  var y = ++x + x + x++;\n"
       "  return x;\n"
       "}",
       9},
      {"function f() {\n"
       "  var x = 7, y = 100, z = 1000;\n"
       "  return x + (x += 3) + y + (y *= 10) + (z *= 7) + z;\n"
       "}",
       15117},
      {"function f() {\n"
       "  var inner = function (x) { return x + (x = 2) + (x = 4) + x; };\n"
       "  return inner(1);\n"
       "}",
       11},
      {"function f() {\n"
       "  var x = 1, y = 2;\n"
       "  x = x + (x = 3) + y + (y = 4), y = y + (y = 5) + y + x;\n"
       "  return x + y;\n"
       "}",
       10 + 24},
      {"function f() {\n"
       "  var x = 0;\n"
       "  var y = x | (x = 1) | (x = 2);\n"
       "  return x;\n"
       "}",
       2},
      {"function f() {\n"
       "  var x = 0;\n"
       "  var y = x || (x = 1);\n"
       "  return x;\n"
       "}",
       1},
      {"function f() {\n"
       "  var x = 1;\n"
       "  var y = x && (x = 2) && (x = 3);\n"
       "  return x;\n"
       "}",
       3},
      {"function f() {\n"
       "  var x = 1;\n"
       "  var y = x || (x = 2);\n"
       "  return x;\n"
       "}",
       1},
      {"function f() {\n"
       "  var x = 1;\n"
       "  x = (x << (x = 3)) | (x = 16);\n"
       "  return x;\n"
       "}",
       24},
      {"function f() {\n"
       "  var r = 7;\n"
       "  var s = 11;\n"
       "  var t = 13;\n"
       "  var u = r + s + t + (r = 10) + (s = 20) +"
       "          (t = (r + s)) + r + s + t;\n"
       "  return r + s + t + u;\n"
       "}",
       211},
      {"function f() {\n"
       "  var r = 7;\n"
       "  var s = 11;\n"
       "  var t = 13;\n"
       "  return r > (3 * s * (s = 1)) ? (t + (t += 1)) : (r + (r = 4));\n"
       "}",
       11},
      {"function f() {\n"
       "  var r = 7;\n"
       "  var s = 11;\n"
       "  var t = 13;\n"
       "  return r > (3 * s * (s = 0)) ? (t + (t += 1)) : (r + (r = 4));\n"
       "}",
       27},
      {"function f() {\n"
       "  var r = 7;\n"
       "  var s = 11;\n"
       "  var t = 13;\n"
       "  return (r + (r = 5)) > s ? r : t;\n"
       "}",
       5},
      {"function f(a) {\n"
       "  return a + (arguments[0] = 10);\n"
       "}",
       50},
      {"function f(a) {\n"
       "  return a + (arguments[0] = 10) + a;\n"
       "}",
       60},
      {"function f(a) {\n"
       "  return a + (arguments[0] = 10) + arguments[0];\n"
       "}",
       60},
  };

  const int arg_value = 40;
  for (size_t i = 0; i < arraysize(samples); i++) {
    InterpreterTester tester(handles.main_isolate(), samples[i].first);
    auto callable = tester.GetCallable<Handle<Object>>();
    Handle<Object> return_val =
        callable(handle(Smi::FromInt(arg_value), handles.main_isolate()))
            .ToHandleChecked();
    CHECK_EQ(Handle<Smi>::cast(return_val)->value(), samples[i].second);
  }
}


TEST(InterpreterToName) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> to_name_tests[] = {
      {"var a = 'val'; var obj = {[a] : 10}; return obj.val;",
       factory->NewNumberFromInt(10)},
      {"var a = 20; var obj = {[a] : 10}; return obj['20'];",
       factory->NewNumberFromInt(10)},
      {"var a = 20; var obj = {[a] : 10}; return obj[20];",
       factory->NewNumberFromInt(10)},
      {"var a = {val:23}; var obj = {[a] : 10}; return obj[a];",
       factory->NewNumberFromInt(10)},
      {"var a = {val:23}; var obj = {[a] : 10};\n"
       "return obj['[object Object]'];",
       factory->NewNumberFromInt(10)},
      {"var a = {toString : function() { return 'x'}};\n"
       "var obj = {[a] : 10};\n"
       "return obj.x;",
       factory->NewNumberFromInt(10)},
      {"var a = {valueOf : function() { return 'x'}};\n"
       "var obj = {[a] : 10};\n"
       "return obj.x;",
       factory->undefined_value()},
      {"var a = {[Symbol.toPrimitive] : function() { return 'x'}};\n"
       "var obj = {[a] : 10};\n"
       "return obj.x;",
       factory->NewNumberFromInt(10)},
  };

  for (size_t i = 0; i < arraysize(to_name_tests); i++) {
    std::string source(
        InterpreterTester::SourceForBody(to_name_tests[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*to_name_tests[i].second));
  }
}


TEST(TemporaryRegisterAllocation) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> reg_tests[] = {
      {"function add(a, b, c) {"
       "   return a + b + c;"
       "}"
       "function f() {"
       "  var a = 10, b = 10;"
       "   return add(a, b++, b);"
       "}",
       factory->NewNumberFromInt(31)},
      {"function add(a, b, c, d) {"
       "  return a + b + c + d;"
       "}"
       "function f() {"
       "  var x = 10, y = 20, z = 30;"
       "  return x + add(x, (y= x++), x, z);"
       "}",
       factory->NewNumberFromInt(71)},
  };

  for (size_t i = 0; i < arraysize(reg_tests); i++) {
    InterpreterTester tester(handles.main_isolate(), reg_tests[i].first);
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*reg_tests[i].second));
  }
}


TEST(InterpreterLookupSlot) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  // TODO(mythria): Add more tests when we have support for eval/with.
  const char* function_prologue = "var f;"
                                  "var x = 1;"
                                  "function f1() {"
                                  "  eval(\"function t() {";
  const char* function_epilogue = "        }; f = t;\");"
                                  "}"
                                  "f1();";


  std::pair<const char*, Handle<Object>> lookup_slot[] = {
      {"return x;", handle(Smi::FromInt(1), isolate)},
      {"return typeof x;", factory->NewStringFromStaticChars("number")},
      {"return typeof dummy;", factory->NewStringFromStaticChars("undefined")},
      {"x = 10; return x;", handle(Smi::FromInt(10), isolate)},
      {"'use strict'; x = 20; return x;", handle(Smi::FromInt(20), isolate)},
  };

  for (size_t i = 0; i < arraysize(lookup_slot); i++) {
    std::string script = std::string(function_prologue) +
                         std::string(lookup_slot[i].first) +
                         std::string(function_epilogue);

    InterpreterTester tester(handles.main_isolate(), script.c_str(), "t");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*lookup_slot[i].second));
  }
}


TEST(InterpreterCallLookupSlot) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> call_lookup[] = {
      {"g = function(){ return 2 }; eval(''); return g();",
       handle(Smi::FromInt(2), isolate)},
      {"g = function(){ return 2 }; eval('g = function() {return 3}');\n"
       "return g();",
       handle(Smi::FromInt(3), isolate)},
      {"g = { x: function(){ return this.y }, y: 20 };\n"
       "eval('g = { x: g.x, y: 30 }');\n"
       "return g.x();",
       handle(Smi::FromInt(30), isolate)},
  };

  for (size_t i = 0; i < arraysize(call_lookup); i++) {
    std::string source(InterpreterTester::SourceForBody(call_lookup[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*call_lookup[i].second));
  }
}


TEST(InterpreterLookupSlotWide) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  const char* function_prologue =
      "var f;"
      "var x = 1;"
      "function f1() {"
      "  eval(\"function t() {";
  const char* function_epilogue =
      "        }; f = t;\");"
      "}"
      "f1();";
  std::ostringstream str;
  str << "var y = 2.3;";
  for (int i = 1; i < 256; i++) {
    str << "y = " << 2.3 + i << ";";
  }
  std::string init_function_body = str.str();

  std::pair<std::string, Handle<Object>> lookup_slot[] = {
      {init_function_body + "return x;", handle(Smi::FromInt(1), isolate)},
      {init_function_body + "return typeof x;",
       factory->NewStringFromStaticChars("number")},
      {init_function_body + "return x = 10;",
       handle(Smi::FromInt(10), isolate)},
      {"'use strict';" + init_function_body + "x = 20; return x;",
       handle(Smi::FromInt(20), isolate)},
  };

  for (size_t i = 0; i < arraysize(lookup_slot); i++) {
    std::string script = std::string(function_prologue) + lookup_slot[i].first +
                         std::string(function_epilogue);

    InterpreterTester tester(handles.main_isolate(), script.c_str(), "t");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*lookup_slot[i].second));
  }
}


TEST(InterpreterDeleteLookupSlot) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  // TODO(mythria): Add more tests when we have support for eval/with.
  const char* function_prologue = "var f;"
                                  "var x = 1;"
                                  "y = 10;"
                                  "var obj = {val:10};"
                                  "var z = 30;"
                                  "function f1() {"
                                  "  var z = 20;"
                                  "  eval(\"function t() {";
  const char* function_epilogue = "        }; f = t;\");"
                                  "}"
                                  "f1();";


  std::pair<const char*, Handle<Object>> delete_lookup_slot[] = {
      {"return delete x;", factory->false_value()},
      {"return delete y;", factory->true_value()},
      {"return delete z;", factory->false_value()},
      {"return delete obj.val;", factory->true_value()},
      {"'use strict'; return delete obj.val;", factory->true_value()},
  };

  for (size_t i = 0; i < arraysize(delete_lookup_slot); i++) {
    std::string script = std::string(function_prologue) +
                         std::string(delete_lookup_slot[i].first) +
                         std::string(function_epilogue);

    InterpreterTester tester(handles.main_isolate(), script.c_str(), "t");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*delete_lookup_slot[i].second));
  }
}


TEST(JumpWithConstantsAndWideConstants) {
  HandleAndZoneScope handles;
  auto isolate = handles.main_isolate();
  auto factory = isolate->factory();
  const int kStep = 13;
  for (int constants = 3; constants < 256 + 3 * kStep; constants += kStep) {
    std::ostringstream filler_os;
    // Generate a string that consumes constant pool entries and
    // spread out branch distances in script below.
    for (int i = 0; i < constants; i++) {
      filler_os << "var x_ = 'x_" << i << "';\n";
    }
    std::string filler(filler_os.str());
    std::ostringstream script_os;
    script_os << "function " << InterpreterTester::function_name() << "(a) {\n";
    script_os << "  " << filler;
    script_os << "  for (var i = a; i < 2; i++) {\n";
    script_os << "  " << filler;
    script_os << "    if (i == 0) { " << filler << "i = 10; continue; }\n";
    script_os << "    else if (i == a) { " << filler << "i = 12; break; }\n";
    script_os << "    else { " << filler << " }\n";
    script_os << "  }\n";
    script_os << "  return i;\n";
    script_os << "}\n";
    std::string script(script_os.str());
    for (int a = 0; a < 3; a++) {
      InterpreterTester tester(handles.main_isolate(), script.c_str());
      auto callable = tester.GetCallable<Handle<Object>>();
      Handle<Object> return_val =
          callable(factory->NewNumberFromInt(a)).ToHandleChecked();
      static const int results[] = {11, 12, 2};
      CHECK_EQ(Handle<Smi>::cast(return_val)->value(), results[a]);
    }
  }
}


TEST(InterpreterEval) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> eval[] = {
      {"return eval('1;');", handle(Smi::FromInt(1), isolate)},
      {"return eval('100 * 20;');", handle(Smi::FromInt(2000), isolate)},
      {"var x = 10; return eval('x + 20;');",
       handle(Smi::FromInt(30), isolate)},
      {"var x = 10; eval('x = 33;'); return x;",
       handle(Smi::FromInt(33), isolate)},
      {"'use strict'; var x = 20; var z = 0;\n"
       "eval('var x = 33; z = x;'); return x + z;",
       handle(Smi::FromInt(53), isolate)},
      {"eval('var x = 33;'); eval('var y = x + 20'); return x + y;",
       handle(Smi::FromInt(86), isolate)},
      {"var x = 1; eval('for(i = 0; i < 10; i++) x = x + 1;'); return x",
       handle(Smi::FromInt(11), isolate)},
      {"var x = 10; eval('var x = 20;'); return x;",
       handle(Smi::FromInt(20), isolate)},
      {"var x = 1; eval('\"use strict\"; var x = 2;'); return x;",
       handle(Smi::FromInt(1), isolate)},
      {"'use strict'; var x = 1; eval('var x = 2;'); return x;",
       handle(Smi::FromInt(1), isolate)},
      {"var x = 10; eval('x + 20;'); return typeof x;",
       factory->NewStringFromStaticChars("number")},
      {"eval('var y = 10;'); return typeof unallocated;",
       factory->NewStringFromStaticChars("undefined")},
      {"'use strict'; eval('var y = 10;'); return typeof unallocated;",
       factory->NewStringFromStaticChars("undefined")},
      {"eval('var x = 10;'); return typeof x;",
       factory->NewStringFromStaticChars("number")},
      {"var x = {}; eval('var x = 10;'); return typeof x;",
       factory->NewStringFromStaticChars("number")},
      {"'use strict'; var x = {}; eval('var x = 10;'); return typeof x;",
       factory->NewStringFromStaticChars("object")},
  };

  for (size_t i = 0; i < arraysize(eval); i++) {
    std::string source(InterpreterTester::SourceForBody(eval[i].first));
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*eval[i].second));
  }
}


TEST(InterpreterEvalParams) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> eval_params[] = {
      {"var x = 10; return eval('x + p1;');",
       handle(Smi::FromInt(30), isolate)},
      {"var x = 10; eval('p1 = x;'); return p1;",
       handle(Smi::FromInt(10), isolate)},
      {"var a = 10;"
       "function inner() { return eval('a + p1;');}"
       "return inner();",
       handle(Smi::FromInt(30), isolate)},
  };

  for (size_t i = 0; i < arraysize(eval_params); i++) {
    std::string source = "function " + InterpreterTester::function_name() +
                         "(p1) {" + eval_params[i].first + "}";
    InterpreterTester tester(handles.main_isolate(), source.c_str());
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<i::Object> return_value =
        callable(handle(Smi::FromInt(20), isolate)).ToHandleChecked();
    CHECK(return_value->SameValue(*eval_params[i].second));
  }
}


TEST(InterpreterEvalGlobal) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> eval_global[] = {
      {"function add_global() { eval('function test() { z = 33; }; test()'); };"
       "function f() { add_global(); return z; }; f();",
       handle(Smi::FromInt(33), isolate)},
      {"function add_global() {\n"
       " eval('\"use strict\"; function test() { y = 33; };"
       "      try { test() } catch(e) {}');\n"
       "}\n"
       "function f() { add_global(); return typeof y; } f();",
       factory->NewStringFromStaticChars("undefined")},
  };

  for (size_t i = 0; i < arraysize(eval_global); i++) {
    InterpreterTester tester(handles.main_isolate(), eval_global[i].first,
                             "test");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*eval_global[i].second));
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
