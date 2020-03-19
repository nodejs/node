// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_TEST_UTILS_H_
#define V8_UNITTESTS_TEST_UTILS_H_

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

// RAII-like Isolate instance wrapper.
class IsolateWrapper final {
 public:
  // When enforce_pointer_compression is true the Isolate is created with
  // enabled pointer compression. When it's false then the Isolate is created
  // with the default pointer compression state for current build.
  explicit IsolateWrapper(CounterLookupCallback counter_lookup_callback,
                          bool enforce_pointer_compression = false);
  ~IsolateWrapper();

  v8::Isolate* isolate() const { return isolate_; }

 private:
  v8::ArrayBuffer::Allocator* array_buffer_allocator_;
  v8::Isolate* isolate_;

  DISALLOW_COPY_AND_ASSIGN(IsolateWrapper);
};

class SharedIsolateHolder final {
 public:
  static v8::Isolate* isolate() { return isolate_wrapper_->isolate(); }

  static void CreateIsolate() {
    CHECK_NULL(isolate_wrapper_);
    isolate_wrapper_ =
        new IsolateWrapper([](const char* name) -> int* { return nullptr; });
  }

  static void DeleteIsolate() {
    CHECK_NOT_NULL(isolate_wrapper_);
    delete isolate_wrapper_;
    isolate_wrapper_ = nullptr;
  }

 private:
  static v8::IsolateWrapper* isolate_wrapper_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SharedIsolateHolder);
};

class SharedIsolateAndCountersHolder final {
 public:
  static v8::Isolate* isolate() { return isolate_wrapper_->isolate(); }

  static void CreateIsolate() {
    CHECK_NULL(counter_map_);
    CHECK_NULL(isolate_wrapper_);
    counter_map_ = new CounterMap();
    isolate_wrapper_ = new IsolateWrapper(LookupCounter);
  }

  static void DeleteIsolate() {
    CHECK_NOT_NULL(counter_map_);
    CHECK_NOT_NULL(isolate_wrapper_);
    delete isolate_wrapper_;
    isolate_wrapper_ = nullptr;
    delete counter_map_;
    counter_map_ = nullptr;
  }

 private:
  static int* LookupCounter(const char* name);
  static CounterMap* counter_map_;
  static v8::IsolateWrapper* isolate_wrapper_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SharedIsolateAndCountersHolder);
};

//
// A set of mixins from which the test fixtures will be constructed.
//
template <typename TMixin>
class WithPrivateIsolateMixin : public TMixin {
 public:
  explicit WithPrivateIsolateMixin(bool enforce_pointer_compression = false)
      : isolate_wrapper_([](const char* name) -> int* { return nullptr; },
                         enforce_pointer_compression) {}

  v8::Isolate* v8_isolate() const { return isolate_wrapper_.isolate(); }

  static void SetUpTestCase() { TMixin::SetUpTestCase(); }
  static void TearDownTestCase() { TMixin::TearDownTestCase(); }

 private:
  v8::IsolateWrapper isolate_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(WithPrivateIsolateMixin);
};

template <typename TMixin, typename TSharedIsolateHolder = SharedIsolateHolder>
class WithSharedIsolateMixin : public TMixin {
 public:
  WithSharedIsolateMixin() = default;

  v8::Isolate* v8_isolate() const { return TSharedIsolateHolder::isolate(); }

  static void SetUpTestCase() {
    TMixin::SetUpTestCase();
    TSharedIsolateHolder::CreateIsolate();
  }

  static void TearDownTestCase() {
    TSharedIsolateHolder::DeleteIsolate();
    TMixin::TearDownTestCase();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WithSharedIsolateMixin);
};

template <typename TMixin>
class WithPointerCompressionIsolateMixin
    : public WithPrivateIsolateMixin<TMixin> {
 public:
  WithPointerCompressionIsolateMixin()
      : WithPrivateIsolateMixin<TMixin>(true) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WithPointerCompressionIsolateMixin);
};

template <typename TMixin>
class WithIsolateScopeMixin : public TMixin {
 public:
  WithIsolateScopeMixin()
      : isolate_scope_(v8_isolate()), handle_scope_(v8_isolate()) {}

  v8::Isolate* isolate() const { return v8_isolate(); }
  v8::Isolate* v8_isolate() const { return TMixin::v8_isolate(); }

  v8::internal::Isolate* i_isolate() const {
    return reinterpret_cast<v8::internal::Isolate*>(v8_isolate());
  }

  static void SetUpTestCase() { TMixin::SetUpTestCase(); }
  static void TearDownTestCase() { TMixin::TearDownTestCase(); }

 private:
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;

  DISALLOW_COPY_AND_ASSIGN(WithIsolateScopeMixin);
};

template <typename TMixin>
class WithContextMixin : public TMixin {
 public:
  WithContextMixin()
      : context_(Context::New(v8_isolate())), context_scope_(context_) {}

  v8::Isolate* v8_isolate() const { return TMixin::v8_isolate(); }

  const Local<Context>& context() const { return v8_context(); }
  const Local<Context>& v8_context() const { return context_; }

  Local<Value> RunJS(const char* source) {
    return RunJS(
        v8::String::NewFromUtf8(v8_isolate(), source).ToLocalChecked());
  }

  Local<Value> RunJS(v8::String::ExternalOneByteStringResource* source) {
    return RunJS(
        v8::String::NewExternalOneByte(v8_isolate(), source).ToLocalChecked());
  }

  v8::Local<v8::String> NewString(const char* string) {
    return v8::String::NewFromUtf8(v8_isolate(), string).ToLocalChecked();
  }

  void SetGlobalProperty(const char* name, v8::Local<v8::Value> value) {
    CHECK(v8_context()
              ->Global()
              ->Set(v8_context(), NewString(name), value)
              .FromJust());
  }

  static void SetUpTestCase() { TMixin::SetUpTestCase(); }
  static void TearDownTestCase() { TMixin::TearDownTestCase(); }

 private:
  Local<Value> RunJS(Local<String> source) {
    auto context = v8_isolate()->GetCurrentContext();
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
using TestWithIsolate =          //
    WithIsolateScopeMixin<       //
        WithSharedIsolateMixin<  //
            ::testing::Test>>;

// Use v8::internal::TestWithNativeContext if you are testing internals,
// aka. directly work with Handles.
using TestWithContext =              //
    WithContextMixin<                //
        WithIsolateScopeMixin<       //
            WithSharedIsolateMixin<  //
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

  static void SetUpTestCase() { TMixin::SetUpTestCase(); }
  static void TearDownTestCase() { TMixin::TearDownTestCase(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WithInternalIsolateMixin);
};

template <typename TMixin>
class WithZoneMixin : public TMixin {
 public:
  WithZoneMixin() : zone_(&allocator_, ZONE_NAME) {}

  Zone* zone() { return &zone_; }

  static void SetUpTestCase() { TMixin::SetUpTestCase(); }
  static void TearDownTestCase() { TMixin::TearDownTestCase(); }

 private:
  v8::internal::AccountingAllocator allocator_;
  Zone zone_;

  DISALLOW_COPY_AND_ASSIGN(WithZoneMixin);
};

using TestWithIsolate =              //
    WithInternalIsolateMixin<        //
        WithIsolateScopeMixin<       //
            WithSharedIsolateMixin<  //
                ::testing::Test>>>;

using TestWithZone = WithZoneMixin<::testing::Test>;

using TestWithIsolateAndZone =       //
    WithInternalIsolateMixin<        //
        WithIsolateScopeMixin<       //
            WithSharedIsolateMixin<  //
                WithZoneMixin<       //
                    ::testing::Test>>>>;

using TestWithNativeContext =            //
    WithInternalIsolateMixin<            //
        WithContextMixin<                //
            WithIsolateScopeMixin<       //
                WithSharedIsolateMixin<  //
                    ::testing::Test>>>>;

using TestWithNativeContextAndCounters =  //
    WithInternalIsolateMixin<             //
        WithContextMixin<                 //
            WithIsolateScopeMixin<        //
                WithSharedIsolateMixin<   //
                    ::testing::Test,      //
                    SharedIsolateAndCountersHolder>>>>;

using TestWithNativeContextAndZone =         //
    WithZoneMixin<                           //
        WithInternalIsolateMixin<            //
            WithContextMixin<                //
                WithIsolateScopeMixin<       //
                    WithSharedIsolateMixin<  //
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
