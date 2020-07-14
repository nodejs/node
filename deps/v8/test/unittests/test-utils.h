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

// When PointerCompressionMode is kEnforcePointerCompression, the Isolate is
// created with pointer compression force enabled. When it's
// kDefaultPointerCompression then the Isolate is created with the default
// pointer compression state for the current build.
enum PointerCompressionMode {
  kDefaultPointerCompression,
  kEnforcePointerCompression
};

// RAII-like Isolate instance wrapper.
class IsolateWrapper final {
 public:
  explicit IsolateWrapper(CountersMode counters_mode,
                          PointerCompressionMode pointer_compression_mode);
  ~IsolateWrapper();

  v8::Isolate* isolate() const { return isolate_; }

 private:
  std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
  std::unique_ptr<CounterMap> counter_map_;
  v8::Isolate* isolate_;

  DISALLOW_COPY_AND_ASSIGN(IsolateWrapper);
};

//
// A set of mixins from which the test fixtures will be constructed.
//
template <typename TMixin, CountersMode kCountersMode = kNoCounters,
          PointerCompressionMode kPointerCompressionMode =
              kDefaultPointerCompression>
class WithIsolateMixin : public TMixin {
 public:
  WithIsolateMixin()
      : isolate_wrapper_(kCountersMode, kPointerCompressionMode) {}

  v8::Isolate* v8_isolate() const { return isolate_wrapper_.isolate(); }

 private:
  v8::IsolateWrapper isolate_wrapper_;
};

template <typename TMixin, CountersMode kCountersMode = kNoCounters>
using WithPointerCompressionIsolateMixin =
    WithIsolateMixin<TMixin, kCountersMode, kEnforcePointerCompression>;

template <typename TMixin>
class WithIsolateScopeMixin : public TMixin {
 public:
  WithIsolateScopeMixin()
      : isolate_scope_(this->v8_isolate()), handle_scope_(this->v8_isolate()) {}

  v8::Isolate* isolate() const { return this->v8_isolate(); }

  v8::internal::Isolate* i_isolate() const {
    return reinterpret_cast<v8::internal::Isolate*>(this->v8_isolate());
  }

 private:
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;

  DISALLOW_COPY_AND_ASSIGN(WithIsolateScopeMixin);
};

template <typename TMixin>
class WithContextMixin : public TMixin {
 public:
  WithContextMixin()
      : context_(Context::New(this->v8_isolate())), context_scope_(context_) {}

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

  DISALLOW_COPY_AND_ASSIGN(WithContextMixin);
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

using TestWithIsolateAndPointerCompression =     //
    WithContextMixin<                            //
        WithIsolateScopeMixin<                   //
            WithPointerCompressionIsolateMixin<  //
                ::testing::Test>>>;

namespace internal {

// Forward declarations.
class Factory;

template <typename TMixin>
class WithInternalIsolateMixin : public TMixin {
 public:
  WithInternalIsolateMixin() = default;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(WithInternalIsolateMixin);
};

template <typename TMixin>
class WithZoneMixin : public TMixin {
 public:
  WithZoneMixin() : zone_(&allocator_, ZONE_NAME) {}

  Zone* zone() { return &zone_; }

 private:
  v8::internal::AccountingAllocator allocator_;
  Zone zone_;

  DISALLOW_COPY_AND_ASSIGN(WithZoneMixin);
};

using TestWithIsolate =         //
    WithInternalIsolateMixin<   //
        WithIsolateScopeMixin<  //
            WithIsolateMixin<   //
                ::testing::Test>>>;

using TestWithZone = WithZoneMixin<::testing::Test>;

using TestWithIsolateAndZone =  //
    WithInternalIsolateMixin<   //
        WithIsolateScopeMixin<  //
            WithIsolateMixin<   //
                WithZoneMixin<  //
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

class SaveFlags {
 public:
  SaveFlags();
  ~SaveFlags();

 private:
#define FLAG_MODE_APPLY(ftype, ctype, nam, def, cmt) ctype SAVED_##nam;
#include "src/flags/flag-definitions.h"  // NOLINT
#undef FLAG_MODE_APPLY

  DISALLOW_COPY_AND_ASSIGN(SaveFlags);
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
