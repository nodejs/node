// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_TEST_UTILS_H_
#define V8_UNITTESTS_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "include/v8.h"
#include "src/api/api-inl.h"
#include "src/base/macros.h"
#include "src/base/utils/random-number-generator.h"
#include "src/handles/handles.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "testing/gtest-support.h"

namespace v8 {

class ArrayBufferAllocator;

using CounterMap = std::map<std::string, int>;

enum CountersMode { kNoCounters, kEnableCounters };

// RAII-like Isolate instance wrapper.
class IsolateWrapper final {
 public:
  explicit IsolateWrapper(CountersMode counters_mode);
  ~IsolateWrapper();
  IsolateWrapper(const IsolateWrapper&) = delete;
  IsolateWrapper& operator=(const IsolateWrapper&) = delete;

  v8::Isolate* isolate() const { return isolate_; }

 private:
  std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
  std::unique_ptr<CounterMap> counter_map_;
  v8::Isolate* isolate_;
};

//
// A set of mixins from which the test fixtures will be constructed.
//
template <typename TMixin, CountersMode kCountersMode = kNoCounters>
class WithIsolateMixin : public TMixin {
 public:
  WithIsolateMixin() : isolate_wrapper_(kCountersMode) {}

  v8::Isolate* v8_isolate() const { return isolate_wrapper_.isolate(); }

 private:
  v8::IsolateWrapper isolate_wrapper_;
};

template <typename TMixin>
class WithIsolateScopeMixin : public TMixin {
 public:
  WithIsolateScopeMixin()
      : isolate_scope_(this->v8_isolate()), handle_scope_(this->v8_isolate()) {}
  WithIsolateScopeMixin(const WithIsolateScopeMixin&) = delete;
  WithIsolateScopeMixin& operator=(const WithIsolateScopeMixin&) = delete;

  v8::Isolate* isolate() const { return this->v8_isolate(); }

  v8::internal::Isolate* i_isolate() const {
    return reinterpret_cast<v8::internal::Isolate*>(this->v8_isolate());
  }

 private:
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;
};

template <typename TMixin>
class WithContextMixin : public TMixin {
 public:
  WithContextMixin()
      : context_(Context::New(this->v8_isolate())), context_scope_(context_) {}
  WithContextMixin(const WithContextMixin&) = delete;
  WithContextMixin& operator=(const WithContextMixin&) = delete;

  const Local<Context>& context() const { return v8_context(); }
  const Local<Context>& v8_context() const { return context_; }

  Local<Value> RunJS(const char* source) {
    return RunJS(
        v8::String::NewFromUtf8(this->v8_isolate(), source).ToLocalChecked());
  }

  Local<Value> RunJS(v8::String::ExternalOneByteStringResource* source) {
    return RunJS(v8::String::NewExternalOneByte(this->v8_isolate(), source)
                     .ToLocalChecked());
  }

  v8::Local<v8::String> NewString(const char* string) {
    return v8::String::NewFromUtf8(this->v8_isolate(), string).ToLocalChecked();
  }

  void SetGlobalProperty(const char* name, v8::Local<v8::Value> value) {
    CHECK(v8_context()
              ->Global()
              ->Set(v8_context(), NewString(name), value)
              .FromJust());
  }

 private:
  Local<Value> RunJS(Local<String> source) {
    auto context = this->v8_isolate()->GetCurrentContext();
    Local<Script> script =
        v8::Script::Compile(context, source).ToLocalChecked();
    return script->Run(context).ToLocalChecked();
  }

  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

// Use v8::internal::TestWithIsolate if you are testing internals,
// aka. directly work with Handles.
using TestWithIsolate =     //
    WithIsolateScopeMixin<  //
        WithIsolateMixin<   //
            ::testing::Test>>;

// Use v8::internal::TestWithNativeContext if you are testing internals,
// aka. directly work with Handles.
using TestWithContext =         //
    WithContextMixin<           //
        WithIsolateScopeMixin<  //
            WithIsolateMixin<   //
                ::testing::Test>>>;

namespace internal {

// Forward declarations.
class Factory;

template <typename TMixin>
class WithInternalIsolateMixin : public TMixin {
 public:
  WithInternalIsolateMixin() = default;
  WithInternalIsolateMixin(const WithInternalIsolateMixin&) = delete;
  WithInternalIsolateMixin& operator=(const WithInternalIsolateMixin&) = delete;

  Factory* factory() const { return isolate()->factory(); }
  Isolate* isolate() const { return TMixin::i_isolate(); }

  Handle<NativeContext> native_context() const {
    return isolate()->native_context();
  }

  template <typename T = Object>
  Handle<T> RunJS(const char* source) {
    return Handle<T>::cast(RunJSInternal(source));
  }

  Handle<Object> RunJSInternal(const char* source) {
    return Utils::OpenHandle(*TMixin::RunJS(source));
  }

  template <typename T = Object>
  Handle<T> RunJS(::v8::String::ExternalOneByteStringResource* source) {
    return Handle<T>::cast(RunJSInternal(source));
  }

  Handle<Object> RunJSInternal(
      ::v8::String::ExternalOneByteStringResource* source) {
    return Utils::OpenHandle(*TMixin::RunJS(source));
  }

  base::RandomNumberGenerator* random_number_generator() const {
    return isolate()->random_number_generator();
  }
};

template <typename TMixin>
class WithZoneMixin : public TMixin {
 public:
  explicit WithZoneMixin(bool support_zone_compression = false)
      : zone_(&allocator_, ZONE_NAME, support_zone_compression) {}
  WithZoneMixin(const WithZoneMixin&) = delete;
  WithZoneMixin& operator=(const WithZoneMixin&) = delete;

  Zone* zone() { return &zone_; }

 private:
  v8::internal::AccountingAllocator allocator_;
  Zone zone_;
};

using TestWithIsolate =         //
    WithInternalIsolateMixin<   //
        WithIsolateScopeMixin<  //
            WithIsolateMixin<   //
                ::testing::Test>>>;

using TestWithZone = WithZoneMixin<::testing::Test>;

using TestWithIsolateAndZone =      //
    WithZoneMixin<                  //
        WithInternalIsolateMixin<   //
            WithIsolateScopeMixin<  //
                WithIsolateMixin<   //
                    ::testing::Test>>>>;

using TestWithNativeContext =       //
    WithInternalIsolateMixin<       //
        WithContextMixin<           //
            WithIsolateScopeMixin<  //
                WithIsolateMixin<   //
                    ::testing::Test>>>>;

using TestWithNativeContextAndCounters =  //
    WithInternalIsolateMixin<             //
        WithContextMixin<                 //
            WithIsolateScopeMixin<        //
                WithIsolateMixin<         //
                    ::testing::Test, kEnableCounters>>>>;

using TestWithNativeContextAndZone =    //
    WithZoneMixin<                      //
        WithInternalIsolateMixin<       //
            WithContextMixin<           //
                WithIsolateScopeMixin<  //
                    WithIsolateMixin<   //
                        ::testing::Test>>>>>;

class V8_NODISCARD SaveFlags {
 public:
  SaveFlags();
  ~SaveFlags();
  SaveFlags(const SaveFlags&) = delete;
  SaveFlags& operator=(const SaveFlags&) = delete;

 private:
#define FLAG_MODE_APPLY(ftype, ctype, nam, def, cmt) ctype SAVED_##nam;
#include "src/flags/flag-definitions.h"  // NOLINT
#undef FLAG_MODE_APPLY
};

// For GTest.
inline void PrintTo(Object o, ::std::ostream* os) {
  *os << reinterpret_cast<void*>(o.ptr());
}
inline void PrintTo(Smi o, ::std::ostream* os) {
  *os << reinterpret_cast<void*>(o.ptr());
}

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_TEST_UTILS_H_
