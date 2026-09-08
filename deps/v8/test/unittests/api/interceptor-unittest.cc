// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-exception.h"
#include "include/v8-function.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-template.h"
#include "src/flags/flags.h"
#include "src/objects/js-interceptor-map-inl.h"
#include "src/objects/map-inl.h"
#include "test/common/flag-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using InterceptorTest = TestWithContext;

v8::Intercepted NamedGetter(Local<Name> property,
                            const PropertyCallbackInfo<Value>& info) {
  return v8::Intercepted::kNo;
}

}  // namespace

TEST_F(InterceptorTest, FreezeApiObjectWithInterceptor) {
  TryCatch try_catch(isolate());

  Local<FunctionTemplate> tmpl = FunctionTemplate::New(isolate());
  tmpl->InstanceTemplate()->SetHandler(
      NamedPropertyHandlerConfiguration(NamedGetter));

  Local<Function> ctor = tmpl->GetFunction(context()).ToLocalChecked();
  Local<Object> obj = ctor->NewInstance(context()).ToLocalChecked();
  ASSERT_TRUE(
      obj->SetIntegrityLevel(context(), IntegrityLevel::kFrozen).IsNothing());
  ASSERT_TRUE(try_catch.HasCaught());
}

namespace {

v8::Intercepted NamedDescriptor(Local<Name> property,
                                const PropertyCallbackInfo<Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  Local<Name> names[] = {
      v8::String::NewFromUtf8(isolate, "enumerable").ToLocalChecked(),
      v8::String::NewFromUtf8(isolate, "configurable").ToLocalChecked(),
      v8::String::NewFromUtf8(isolate, "writable").ToLocalChecked(),
      v8::String::NewFromUtf8(isolate, "value").ToLocalChecked(),
  };
  Local<Value> values[] = {
      v8::Boolean::New(isolate, true),
      v8::Boolean::New(isolate, true),
      v8::Boolean::New(isolate, true),
      v8::Number::New(isolate, 42),
  };
  // The prototype must be an Object, so that `Object.prototype.get` can be
  // looked up.
  Local<Object> descriptor =
      v8::Object::New(isolate, v8::Object::New(isolate), names, values, 4);
  info.GetReturnValue().Set(descriptor);
  return v8::Intercepted::kYes;
}

}  // namespace

TEST_F(InterceptorTest, GetPropertyDescriptorWithObjectPrototypeProps) {
  TryCatch try_catch(isolate());

  Local<FunctionTemplate> tmpl = FunctionTemplate::New(isolate());
  tmpl->InstanceTemplate()->SetHandler(NamedPropertyHandlerConfiguration(
      nullptr, nullptr, NamedDescriptor, nullptr, nullptr, nullptr));

  Local<Function> ctor = tmpl->GetFunction(context()).ToLocalChecked();
  Local<Object> obj = ctor->NewInstance(context()).ToLocalChecked();

  SetGlobalProperty("obj", obj);
  TryRunJS(
      "Object.prototype.get = 3;"
      "obj.x = 4;");
  ASSERT_TRUE(try_catch.HasCaught());
}

namespace {

struct InterceptorData {
  int call_count = 0;
  std::vector<int> items = {11, 22, 33};
};

void LengthGetter(const FunctionCallbackInfo<v8::Value>& info) {
  InterceptorData* data = GetData<InterceptorData>(info);
  info.GetReturnValue().Set(static_cast<uint32_t>(data->items.size()));
}

void IndexedIterableToListCallback(
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  InterceptorData* data = GetData<InterceptorData>(info);
  data->call_count++;
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  size_t length = data->items.size();
  size_t index = 0;
  v8::Local<v8::Array> array =
      v8::Array::New(context, length,
                     [isolate, data, &index]() -> v8::MaybeLocal<v8::Value> {
                       if (index < data->items.size()) {
                         return v8::Integer::New(isolate, data->items[index++]);
                       }
                       return v8::MaybeLocal<v8::Value>();
                     })
          .ToLocalChecked();
  info.GetReturnValue().Set(array);
}

v8::Intercepted IndexedGetter(uint32_t index,
                              const v8::PropertyCallbackInfo<v8::Value>& info) {
  InterceptorData* data = GetData<InterceptorData>(info);
  if (index < data->items.size()) {
    info.GetReturnValue().Set(data->items[index]);
    return v8::Intercepted::kYes;
  }
  return v8::Intercepted::kNo;
}

v8::Intercepted IndexedQuery(
    uint32_t index, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  InterceptorData* data = GetData<InterceptorData>(info);
  if (index < data->items.size()) {
    info.GetReturnValue().Set(v8::None);
    return v8::Intercepted::kYes;
  }
  return v8::Intercepted::kNo;
}

Local<FunctionTemplate> CreateIterableToListInterceptorTemplate(
    v8::Isolate* isolate, InterceptorData* data, bool has_callback = true) {
  Local<FunctionTemplate> tmpl = FunctionTemplate::New(isolate);
  Local<Value> data_val = MakeData(isolate, data);
  tmpl->InstanceTemplate()->SetHandler(v8::IndexedPropertyHandlerConfiguration(
      IndexedGetter, nullptr, IndexedQuery, nullptr, nullptr, nullptr, nullptr,
      nullptr, has_callback ? IndexedIterableToListCallback : nullptr,
      data_val));

  Local<FunctionTemplate> length_getter =
      FunctionTemplate::New(isolate, &LengthGetter, data_val);

  tmpl->PrototypeTemplate()->Set(v8::String::NewFromUtf8Literal(isolate, "foo"),
                                 v8::Integer::New(isolate, 42));

  tmpl->PrototypeTemplate()->SetAccessorProperty(
      v8::String::NewFromUtf8Literal(isolate, "bar"), length_getter);

  tmpl->PrototypeTemplate()->SetIntrinsicDataProperty(
      v8::Symbol::GetIterator(isolate), v8::Intrinsic::kArrayProto_values);

  tmpl->PrototypeTemplate()->SetAccessorProperty(
      v8::String::NewFromUtf8Literal(isolate, "length"), length_getter);
  return tmpl;
}

void CheckFastIterationResults(InterceptorTest* test, v8::Isolate* isolate,
                               Local<Context> context, const char* var_name,
                               InterceptorData* data, bool expected_fast) {
  int initial_count = data->call_count;
  uint32_t expected_length = static_cast<uint32_t>(data->items.size());

  std::string code_from = std::string("Array.from(") + var_name + ")";
  Local<Value> res_from = test->RunJS(code_from.c_str());
  ASSERT_TRUE(res_from->IsArray());
  Local<Array> arr_from = res_from.As<Array>();

  std::string code_spread =
      std::string("((...args) => args)(...") + var_name + ")";
  Local<Value> res_spread = test->RunJS(code_spread.c_str());
  ASSERT_TRUE(res_spread->IsArray());
  Local<Array> arr_spread = res_spread.As<Array>();

  if (expected_fast) {
    ASSERT_EQ(initial_count + 2, data->call_count);
  } else {
    ASSERT_EQ(initial_count, data->call_count);
  }

  ASSERT_EQ(expected_length, arr_from->Length());
  for (uint32_t i = 0; i < expected_length; ++i) {
    ASSERT_EQ(
        data->items[i],
        arr_from->Get(context, i).ToLocalChecked().As<Integer>()->Value());
  }

  ASSERT_EQ(expected_length, arr_spread->Length());
  for (uint32_t i = 0; i < expected_length; ++i) {
    ASSERT_EQ(
        data->items[i],
        arr_spread->Get(context, i).ToLocalChecked().As<Integer>()->Value());
  }
}

}  // namespace

TEST_F(InterceptorTest, IndexedInterceptorIterableToList_EnabledFlag) {
  i::FlagScope<bool> enable_flag(&i::v8_flags.fast_api_iterable_to_list, true);
  v8::HandleScope scope(isolate());
  InterceptorData data;

  Local<FunctionTemplate> tmpl =
      CreateIterableToListInterceptorTemplate(isolate(), &data);
  Local<Function> ctor = tmpl->GetFunction(context()).ToLocalChecked();
  Local<Object> obj = ctor->NewInstance(context()).ToLocalChecked();
  SetGlobalProperty("obj", obj);

  // Enabled flag state: callback called for Array.from and spread.
  CheckFastIterationResults(this, isolate(), context(), "obj", &data, true);
}

TEST_F(InterceptorTest, IndexedInterceptorIterableToList_DisabledFlag) {
  i::FlagScope<bool> disable_flag(&i::v8_flags.fast_api_iterable_to_list,
                                  false);
  v8::HandleScope scope(isolate());
  InterceptorData data;

  Local<FunctionTemplate> tmpl =
      CreateIterableToListInterceptorTemplate(isolate(), &data);
  Local<Function> ctor = tmpl->GetFunction(context()).ToLocalChecked();
  Local<Object> obj = ctor->NewInstance(context()).ToLocalChecked();
  SetGlobalProperty("obj", obj);

  // Disabled flag state: callback not called.
  CheckFastIterationResults(this, isolate(), context(), "obj", &data, false);
}

TEST_F(InterceptorTest, IndexedInterceptorIterableToList_ReceiverModification) {
  i::FlagScope<bool> enable_flag(&i::v8_flags.fast_api_iterable_to_list, true);
  v8::HandleScope scope(isolate());
  InterceptorData data;

  Local<FunctionTemplate> tmpl =
      CreateIterableToListInterceptorTemplate(isolate(), &data);
  Local<Function> ctor = tmpl->GetFunction(context()).ToLocalChecked();
  Local<Object> obj = ctor->NewInstance(context()).ToLocalChecked();
  SetGlobalProperty("obj", obj);

  // Fresh state: callback called for Array.from and spread.
  CheckFastIterationResults(this, isolate(), context(), "obj", &data, true);

  // Invalidate: add an own property to receiver.
  RunJS("obj.foo = 'bar';");

  // Invalidated state: callback not called.
  CheckFastIterationResults(this, isolate(), context(), "obj", &data, false);

  {
    i::DirectHandle<i::JSObject> i_obj =
        i::Cast<i::JSObject>(v8::Utils::OpenDirectHandle(*obj));
    i::Tagged<i::Map> map = i_obj->map();
    ASSERT_TRUE(i::IsJSInterceptorMap(map));
    ASSERT_FALSE(
        i::Cast<i::JSInterceptorMap>(map)->supports_fast_iterable_to_list());
  }
}

TEST_F(InterceptorTest,
       IndexedInterceptorIterableToList_PrototypeModification) {
  i::FlagScope<bool> enable_flag(&i::v8_flags.fast_api_iterable_to_list, true);
  v8::HandleScope scope(isolate());
  InterceptorData data;

  Local<FunctionTemplate> tmpl =
      CreateIterableToListInterceptorTemplate(isolate(), &data);
  Local<Function> ctor = tmpl->GetFunction(context()).ToLocalChecked();
  Local<Object> obj = ctor->NewInstance(context()).ToLocalChecked();
  SetGlobalProperty("obj", obj);

  // Prototype property modifications that don't shadow length or
  // Symbol.iterator can be healed lazily.
  RunJS("Object.getPrototypeOf(obj).foo = 42;");

  CheckFastIterationResults(this, isolate(), context(), "obj", &data, true);

  {
    i::DirectHandle<i::JSObject> i_obj =
        i::Cast<i::JSObject>(v8::Utils::OpenDirectHandle(*obj));
    i::Tagged<i::Map> map = i_obj->map();
    ASSERT_TRUE(i::IsJSInterceptorMap(map));
    ASSERT_TRUE(
        i::Cast<i::JSInterceptorMap>(map)->supports_fast_iterable_to_list());
  }

  // Modifying the length property invalidates the fast interceptor iteration.
  RunJS(
      "Object.defineProperty(Object.getPrototypeOf(obj), 'length', {get: () => "
      "0});");

  int initial_count = data.call_count;
  Local<Value> res_from = RunJS("Array.from(obj)");
  ASSERT_EQ(0u, res_from.As<Array>()->Length());
  Local<Value> res_spread = RunJS("((...args) => args)(...obj)");
  ASSERT_EQ(0u, res_spread.As<Array>()->Length());

  // Interceptor callback should not be called because length is 0.
  ASSERT_EQ(initial_count, data.call_count);

  {
    i::DirectHandle<i::JSObject> i_obj =
        i::Cast<i::JSObject>(v8::Utils::OpenDirectHandle(*obj));
    i::Tagged<i::Map> map = i_obj->map();
    ASSERT_TRUE(i::IsJSInterceptorMap(map));
    ASSERT_FALSE(
        i::Cast<i::JSInterceptorMap>(map)->supports_fast_iterable_to_list());
  }
}

TEST_F(InterceptorTest, IndexedInterceptorIterableToList_MissingCallback) {
  i::FlagScope<bool> enable_flag(&i::v8_flags.fast_api_iterable_to_list, true);
  v8::HandleScope scope(isolate());
  InterceptorData data;

  Local<FunctionTemplate> tmpl_no_cb = CreateIterableToListInterceptorTemplate(
      isolate(), &data, /*has_callback=*/false);
  Local<Function> ctor_no_cb =
      tmpl_no_cb->GetFunction(context()).ToLocalChecked();
  Local<Object> obj_no_cb = ctor_no_cb->NewInstance(context()).ToLocalChecked();
  SetGlobalProperty("obj_no_cb", obj_no_cb);

  // Interceptor without callback uses slow path.
  CheckFastIterationResults(this, isolate(), context(), "obj_no_cb", &data,
                            false);
}

// namespace internal {
namespace {

const v8::EmbedderDataTypeTag kTestInterceptorTag = 1;

class InterceptorLoggingTest : public internal::TestWithNativeContext {
 public:
  InterceptorLoggingTest() = default;

  static const int kTestIndex = 0;

  static v8::Intercepted NamedPropertyGetter(
      Local<v8::Name> name, const v8::PropertyCallbackInfo<Value>& info) {
    LogCallback(info, "named getter");
    return v8::Intercepted::kNo;
  }

  static v8::Intercepted NamedPropertySetter(
      Local<v8::Name> name, Local<v8::Value> value,
      const v8::PropertyCallbackInfo<v8::Boolean>& info) {
    LogCallback(info, "named setter");
    return v8::Intercepted::kNo;
  }

  static v8::Intercepted NamedPropertyQuery(
      Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
    LogCallback(info, "named query");
    return v8::Intercepted::kNo;
  }

  static v8::Intercepted NamedPropertyDeleter(
      Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
    LogCallback(info, "named deleter");
    return v8::Intercepted::kNo;
  }

  static void NamedPropertyEnumerator(
      const v8::PropertyCallbackInfo<Array>& info) {
    LogCallback(info, "named enumerator");
  }

  static v8::Intercepted NamedPropertyDefiner(
      Local<v8::Name> name, const v8::PropertyDescriptor& desc,
      const v8::PropertyCallbackInfo<v8::Boolean>& info) {
    LogCallback(info, "named definer");
    return v8::Intercepted::kNo;
  }

  static v8::Intercepted NamedPropertyDescriptor(
      Local<v8::Name> name, const v8::PropertyCallbackInfo<Value>& info) {
    LogCallback(info, "named descriptor");
    return v8::Intercepted::kNo;
  }

  static v8::Intercepted IndexedPropertyGetter(
      uint32_t index, const v8::PropertyCallbackInfo<Value>& info) {
    LogCallback(info, "indexed getter");
    return v8::Intercepted::kNo;
  }

  static v8::Intercepted IndexedPropertySetter(
      uint32_t index, Local<v8::Value> value,
      const v8::PropertyCallbackInfo<v8::Boolean>& info) {
    LogCallback(info, "indexed setter");
    return v8::Intercepted::kNo;
  }

  static v8::Intercepted IndexedPropertyQuery(
      uint32_t index, const v8::PropertyCallbackInfo<v8::Integer>& info) {
    LogCallback(info, "indexed query");
    return v8::Intercepted::kNo;
  }

  static v8::Intercepted IndexedPropertyDeleter(
      uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
    LogCallback(info, "indexed deleter");
    return v8::Intercepted::kNo;
  }

  static void IndexedPropertyEnumerator(
      const v8::PropertyCallbackInfo<Array>& info) {
    LogCallback(info, "indexed enumerator");
  }

  static v8::Intercepted IndexedPropertyDefiner(
      uint32_t index, const v8::PropertyDescriptor& desc,
      const v8::PropertyCallbackInfo<v8::Boolean>& info) {
    LogCallback(info, "indexed definer");
    return v8::Intercepted::kNo;
  }

  static v8::Intercepted IndexedPropertyDescriptor(
      uint32_t index, const v8::PropertyCallbackInfo<Value>& info) {
    LogCallback(info, "indexed descriptor");
    return v8::Intercepted::kNo;
  }

  template <class T>
  static void LogCallback(const v8::PropertyCallbackInfo<T>& info,
                          const char* callback_name) {
    InterceptorLoggingTest* test = reinterpret_cast<InterceptorLoggingTest*>(
        info.Holder()->GetAlignedPointerFromInternalField(kTestIndex,
                                                          kTestInterceptorTag));
    test->Log(callback_name);
  }

  void Log(const char* callback_name) {
    if (log_is_empty_) {
      log_is_empty_ = false;
    } else {
      log_ << ", ";
    }
    log_ << callback_name;
  }

 protected:
  void SetUp() override {
    // Set up the object that supports full interceptors.
    v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(v8_isolate());
    templ->SetInternalFieldCount(1);
    templ->SetHandler(v8::NamedPropertyHandlerConfiguration(
        NamedPropertyGetter, NamedPropertySetter, NamedPropertyQuery,
        NamedPropertyDeleter, NamedPropertyEnumerator, NamedPropertyDefiner,
        NamedPropertyDescriptor));
    templ->SetHandler(v8::IndexedPropertyHandlerConfiguration(
        IndexedPropertyGetter, IndexedPropertySetter, IndexedPropertyQuery,
        IndexedPropertyDeleter, IndexedPropertyEnumerator,
        IndexedPropertyDefiner, IndexedPropertyDescriptor));
    v8::Local<v8::Object> instance =
        templ->NewInstance(context()).ToLocalChecked();
    instance->SetAlignedPointerInInternalField(kTestIndex, this,
                                               kTestInterceptorTag);
    SetGlobalProperty("obj", instance);
  }

  std::string Run(const char* script) {
    log_is_empty_ = true;
    log_.str(std::string());
    log_.clear();

    RunJS(script);
    return log_.str();
  }

 private:
  bool log_is_empty_ = false;
  std::stringstream log_;
};

}  // namespace

TEST_F(InterceptorLoggingTest, DispatchTest) {
  EXPECT_EQ(Run("for (var p in obj) {}"),
            "indexed enumerator, named enumerator");
  EXPECT_EQ(Run("Object.keys(obj)"), "indexed enumerator, named enumerator");

  EXPECT_EQ(Run("obj.foo"), "named getter");
  EXPECT_EQ(Run("obj[42]"), "indexed getter");

  EXPECT_EQ(Run("obj.foo = null"),
            "named setter, named descriptor, named query");
  EXPECT_EQ(Run("obj[42] = null"),
            "indexed setter, indexed descriptor, indexed query");

  EXPECT_EQ(Run("Object.getOwnPropertyDescriptor(obj, 'foo')"),
            "named descriptor");

  EXPECT_EQ(Run("Object.getOwnPropertyDescriptor(obj, 42)"),
            "indexed descriptor");

  EXPECT_EQ(Run("Object.defineProperty(obj, 'foo', {value: 42})"),
            "named descriptor, named definer, named setter");
  EXPECT_EQ(Run("Object.defineProperty(obj, 'foo', {get(){} })"),
            "named descriptor, named definer");
  EXPECT_EQ(Run("Object.defineProperty(obj, 'foo', {set(value){}})"),
            "named descriptor, named definer");
  EXPECT_EQ(Run("Object.defineProperty(obj, 'foo', {get(){}, set(value){}})"),
            "named descriptor, named definer");

  EXPECT_EQ(Run("Object.defineProperty(obj, 42, {value: 'foo'})"),
            "indexed descriptor, "
            // then attempt definer first and fallback to setter.
            "indexed definer, indexed setter");

  EXPECT_EQ(Run("Object.prototype.propertyIsEnumerable.call(obj, 'a')"),
            "named query");
  EXPECT_EQ(Run("Object.prototype.propertyIsEnumerable.call(obj, 42)"),
            "indexed query");

  EXPECT_EQ(Run("Object.prototype.hasOwnProperty.call(obj, 'a')"),
            "named query");
  EXPECT_EQ(Run("Object.prototype.hasOwnProperty.call(obj, '42')"),
            "indexed query");
}

}  // namespace v8
