// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "src/api-inl.h"
#include "src/base/platform/time.h"
#include "src/flags.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/v8.h"

namespace v8 {

// static
v8::ArrayBuffer::Allocator* TestWithIsolate::array_buffer_allocator_ = nullptr;

// static
Isolate* TestWithIsolate::isolate_ = nullptr;

TestWithIsolate::TestWithIsolate()
    : isolate_scope_(isolate()), handle_scope_(isolate()) {}

TestWithIsolate::~TestWithIsolate() = default;

// static
void TestWithIsolate::SetUpTestCase() {
  Test::SetUpTestCase();
  EXPECT_EQ(nullptr, isolate_);
  v8::Isolate::CreateParams create_params;
  array_buffer_allocator_ = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  create_params.array_buffer_allocator = array_buffer_allocator_;
  isolate_ = v8::Isolate::New(create_params);
  EXPECT_TRUE(isolate_ != nullptr);
}


// static
void TestWithIsolate::TearDownTestCase() {
  ASSERT_TRUE(isolate_ != nullptr);
  v8::Platform* platform = internal::V8::GetCurrentPlatform();
  ASSERT_TRUE(platform != nullptr);
  while (platform::PumpMessageLoop(platform, isolate_)) continue;
  isolate_->Dispose();
  isolate_ = nullptr;
  delete array_buffer_allocator_;
  Test::TearDownTestCase();
}

Local<Value> TestWithIsolate::RunJS(const char* source) {
  Local<Script> script =
      v8::Script::Compile(
          isolate()->GetCurrentContext(),
          v8::String::NewFromUtf8(isolate(), source, v8::NewStringType::kNormal)
              .ToLocalChecked())
          .ToLocalChecked();
  return script->Run(isolate()->GetCurrentContext()).ToLocalChecked();
}

Local<Value> TestWithIsolate::RunJS(
    String::ExternalOneByteStringResource* source) {
  Local<Script> script =
      v8::Script::Compile(
          isolate()->GetCurrentContext(),
          v8::String::NewExternalOneByte(isolate(), source).ToLocalChecked())
          .ToLocalChecked();
  return script->Run(isolate()->GetCurrentContext()).ToLocalChecked();
}

TestWithContext::TestWithContext()
    : context_(Context::New(isolate())), context_scope_(context_) {}

TestWithContext::~TestWithContext() = default;

v8::Local<v8::String> TestWithContext::NewString(const char* string) {
  return v8::String::NewFromUtf8(v8_isolate(), string,
                                 v8::NewStringType::kNormal)
      .ToLocalChecked();
}

void TestWithContext::SetGlobalProperty(const char* name,
                                        v8::Local<v8::Value> value) {
  CHECK(v8_context()
            ->Global()
            ->Set(v8_context(), NewString(name), value)
            .FromJust());
}

namespace internal {

TestWithIsolate::~TestWithIsolate() = default;

TestWithIsolateAndZone::~TestWithIsolateAndZone() = default;

Factory* TestWithIsolate::factory() const { return isolate()->factory(); }

Handle<Object> TestWithIsolate::RunJSInternal(const char* source) {
  return Utils::OpenHandle(*::v8::TestWithIsolate::RunJS(source));
}

Handle<Object> TestWithIsolate::RunJSInternal(
    ::v8::String::ExternalOneByteStringResource* source) {
  return Utils::OpenHandle(*::v8::TestWithIsolate::RunJS(source));
}

base::RandomNumberGenerator* TestWithIsolate::random_number_generator() const {
  return isolate()->random_number_generator();
}

TestWithZone::~TestWithZone() = default;

TestWithNativeContext::~TestWithNativeContext() = default;

Handle<Context> TestWithNativeContext::native_context() const {
  return isolate()->native_context();
}

SaveFlags::SaveFlags() { non_default_flags_ = FlagList::argv(); }

SaveFlags::~SaveFlags() {
  FlagList::ResetAllFlags();
  int argc = static_cast<int>(non_default_flags_->size());
  FlagList::SetFlagsFromCommandLine(
      &argc, const_cast<char**>(non_default_flags_->data()),
      false /* remove_flags */);
  for (auto flag = non_default_flags_->begin();
       flag != non_default_flags_->end(); ++flag) {
    delete[] * flag;
  }
  delete non_default_flags_;
}

}  // namespace internal
}  // namespace v8
