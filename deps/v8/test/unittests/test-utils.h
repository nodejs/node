// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_TEST_UTILS_H_
#define V8_UNITTESTS_TEST_UTILS_H_

#include <vector>

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/utils/random-number-generator.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "testing/gtest-support.h"

namespace v8 {

class ArrayBufferAllocator;

// Use v8::internal::TestWithIsolate  if you are testing internals,
// aka. directly work with Handles.
class TestWithIsolate : public virtual ::testing::Test {
 public:
  TestWithIsolate();
  ~TestWithIsolate() override;

  v8::Isolate* isolate() const { return v8_isolate(); }

  v8::Isolate* v8_isolate() const { return isolate_; }

  v8::internal::Isolate* i_isolate() const {
    return reinterpret_cast<v8::internal::Isolate*>(isolate());
  }

  Local<Value> RunJS(const char* source);
  Local<Value> RunJS(String::ExternalOneByteStringResource* source);

  static void SetUpTestCase();
  static void TearDownTestCase();

 private:
  static v8::ArrayBuffer::Allocator* array_buffer_allocator_;
  static v8::Isolate* isolate_;
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;

  DISALLOW_COPY_AND_ASSIGN(TestWithIsolate);
};

// Use v8::internal::TestWithNativeContext if you are testing internals,
// aka. directly work with Handles.
class TestWithContext : public virtual v8::TestWithIsolate {
 public:
  TestWithContext();
  ~TestWithContext() override;

  const Local<Context>& context() const { return v8_context(); }
  const Local<Context>& v8_context() const { return context_; }

  v8::Local<v8::String> NewString(const char* string);
  void SetGlobalProperty(const char* name, v8::Local<v8::Value> value);

 private:
  Local<Context> context_;
  v8::Context::Scope context_scope_;

  DISALLOW_COPY_AND_ASSIGN(TestWithContext);
};

namespace internal {

// Forward declarations.
class Factory;


class TestWithIsolate : public virtual ::v8::TestWithIsolate {
 public:
  TestWithIsolate() = default;
  ~TestWithIsolate() override;

  Factory* factory() const;
  Isolate* isolate() const { return i_isolate(); }
  template <typename T = Object>
  Handle<T> RunJS(const char* source) {
    return Handle<T>::cast(RunJSInternal(source));
  }
  Handle<Object> RunJSInternal(const char* source);
  template <typename T = Object>
  Handle<T> RunJS(::v8::String::ExternalOneByteStringResource* source) {
    return Handle<T>::cast(RunJSInternal(source));
  }
  Handle<Object> RunJSInternal(
      ::v8::String::ExternalOneByteStringResource* source);

  base::RandomNumberGenerator* random_number_generator() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWithIsolate);
};

class TestWithZone : public virtual ::testing::Test {
 public:
  TestWithZone() : zone_(&allocator_, ZONE_NAME) {}
  ~TestWithZone() override;

  Zone* zone() { return &zone_; }

 private:
  v8::internal::AccountingAllocator allocator_;
  Zone zone_;

  DISALLOW_COPY_AND_ASSIGN(TestWithZone);
};

class TestWithIsolateAndZone : public virtual TestWithIsolate {
 public:
  TestWithIsolateAndZone() : zone_(&allocator_, ZONE_NAME) {}
  ~TestWithIsolateAndZone() override;

  Zone* zone() { return &zone_; }

 private:
  v8::internal::AccountingAllocator allocator_;
  Zone zone_;

  DISALLOW_COPY_AND_ASSIGN(TestWithIsolateAndZone);
};

class TestWithNativeContext : public virtual ::v8::TestWithContext,
                              public virtual TestWithIsolate {
 public:
  TestWithNativeContext() = default;
  ~TestWithNativeContext() override;

  Handle<Context> native_context() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWithNativeContext);
};

class SaveFlags {
 public:
  SaveFlags();
  ~SaveFlags();

 private:
  std::vector<const char*>* non_default_flags_;

  DISALLOW_COPY_AND_ASSIGN(SaveFlags);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_TEST_UTILS_H_
