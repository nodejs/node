// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/interpreter/interpreter-tester.h"

#include "src/api/api-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

MaybeDirectHandle<Object> CallInterpreter(Isolate* isolate,
                                          DirectHandle<JSFunction> function) {
  return Execution::Call(isolate, function,
                         isolate->factory()->undefined_value(), {});
}

InterpreterTester::InterpreterTester(
    Isolate* isolate, const char* source, MaybeHandle<BytecodeArray> bytecode,
    MaybeHandle<FeedbackMetadata> feedback_metadata, const char* filter)
    : isolate_(isolate),
      source_(source),
      bytecode_(bytecode),
      feedback_metadata_(feedback_metadata) {
}

InterpreterTester::InterpreterTester(
    Isolate* isolate, Handle<BytecodeArray> bytecode,
    MaybeHandle<FeedbackMetadata> feedback_metadata, const char* filter)
    : InterpreterTester(isolate, nullptr, bytecode, feedback_metadata, filter) {
}

InterpreterTester::InterpreterTester(Isolate* isolate, const char* source,
                                     const char* filter)
    : InterpreterTester(isolate, source, MaybeHandle<BytecodeArray>(),
                        MaybeHandle<FeedbackMetadata>(), filter) {}

InterpreterTester::~InterpreterTester() = default;

Local<Message> InterpreterTester::CheckThrowsReturnMessage() {
  TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate_));
  auto callable = GetCallable<>();
  MaybeDirectHandle<Object> no_result = callable();
  CHECK(isolate_->has_exception());
  CHECK(try_catch.HasCaught());
  CHECK(no_result.is_null());
  CHECK(!try_catch.Message().IsEmpty());
  return try_catch.Message();
}

Handle<JSAny> InterpreterTester::NewObject(const char* script) {
  return Cast<JSAny>(v8::Utils::OpenHandle(*CompileRun(script)));
}

DirectHandle<String> InterpreterTester::GetName(Isolate* isolate,
                                                const char* name) {
  DirectHandle<String> result =
      isolate->factory()->NewStringFromAsciiChecked(name);
  return isolate->string_table()->LookupString(isolate, result);
}

std::string InterpreterTester::SourceForBody(const char* body) {
  return "function " + function_name() + "() {\n" + std::string(body) + "\n}";
}

std::string InterpreterTester::function_name() {
  return std::string(kFunctionName);
}

const char InterpreterTester::kFunctionName[] = "f";

template <typename EmbeddedFeedbackType>
  requires std::is_same_v<EmbeddedFeedbackType, CompareOperationFeedback::Type>
EmbeddedFeedbackType InterpreterTester::GetEmbeddedFeedback(
    Token::Value token, size_t bytecode_offset, int feedback_value_offset) {
  if constexpr (std::is_same_v<EmbeddedFeedbackType,
                               CompareOperationFeedback::Type>) {
    DCHECK_EQ(token, Token::Value::kEqStrict);
  }

  auto bytecode_array = bytecode_.ToHandleChecked();

  uint16_t feedback_value;
#if defined(V8_TARGET_LITTLE_ENDIAN)
  feedback_value =
      static_cast<uint16_t>(bytecode_array->get(
          static_cast<int>(bytecode_offset) + feedback_value_offset)) |
      static_cast<uint16_t>(
          bytecode_array->get(static_cast<int>(bytecode_offset) +
                              feedback_value_offset + 1)
          << 8);
#elif defined(V8_TARGET_BIG_ENDIAN)
  feedback_value =
      static_cast<uint16_t>(
          bytecode_array->get(static_cast<int>(bytecode_offset) +
                              feedback_value_offset)
          << 8) |
      static_cast<uint16_t>(bytecode_array->get(
          static_cast<int>(bytecode_offset) + feedback_value_offset + 1));
#endif

  DCHECK_LE(feedback_value, EmbeddedFeedbackType::kAny);
  DCHECK_GE(feedback_value, EmbeddedFeedbackType::kNone);
  return static_cast<EmbeddedFeedbackType>(feedback_value);
}

template CompareOperationFeedback::Type InterpreterTester::GetEmbeddedFeedback<
    CompareOperationFeedback::Type>(Token::Value, size_t, int);

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
