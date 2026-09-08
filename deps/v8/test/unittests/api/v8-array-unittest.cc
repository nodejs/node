// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-container.h"
#include "include/v8-primitive.h"
#include "include/v8-value.h"
#include "src/api/api-inl.h"
#include "src/execution/protectors-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/js-array-inl.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/interpreter/interpreter-tester.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using ArrayTest = TestWithContext;
using CbResult = v8::Array::CallbackResult;

TEST_F(ArrayTest, IterateEmpty) {
  HandleScope scope(isolate());
  Local<Array> array = Array::New(isolate());
  Array::IterationCallback unreachable_callback =
      [](uint32_t index, Local<Value> element, void* data) -> CbResult {
    UNREACHABLE();
  };
  CHECK(array->Iterate(context(), unreachable_callback, nullptr).IsJust());
}

TEST_F(ArrayTest, IterateOneElement) {
  HandleScope scope(isolate());
  Local<Array> smi_array = Array::New(isolate());
  Local<Array> double_array = Array::New(isolate());
  Local<Array> object_array = Array::New(isolate());
  Local<Array> dictionary_array = Array::New(isolate());
  struct Data {
    int sentinel;
    Local<Context> context;
    Isolate* isolate;
    int invocation_count = 0;
  };
  Data data{42, context(), isolate()};
  const Local<Value> kSmi = Number::New(isolate(), 333);
  const uint32_t kIndex = 3;

  CHECK(smi_array->Set(context(), kIndex, kSmi).FromJust());
  Array::IterationCallback smi_callback =
      [](uint32_t index, Local<Value> element, void* data) -> CbResult {
    Data* d = reinterpret_cast<Data*>(data);
    CHECK_EQ(42, d->sentinel);
    ++d->invocation_count;
    if (index != kIndex) {
      CHECK(element->IsUndefined());
      return CbResult::kContinue;
    }
    CHECK_EQ(333, element->NumberValue(d->context).FromJust());
    return CbResult::kContinue;
  };
  CHECK(smi_array->Iterate(context(), smi_callback, &data).IsJust());
  CHECK_EQ(kIndex + 1, data.invocation_count);

  const Local<Value> kDouble = Number::New(isolate(), 1.5);
  CHECK(double_array->Set(context(), kIndex, kDouble).FromJust());
  Array::IterationCallback double_callback =
      [](uint32_t index, Local<Value> element, void* data) -> CbResult {
    Data* d = reinterpret_cast<Data*>(data);
    CHECK_EQ(42, d->sentinel);
    if (index != kIndex) {
      CHECK(element->IsUndefined());
      return CbResult::kContinue;
    }
    CHECK_EQ(1.5, element->NumberValue(d->context).FromJust());
    return CbResult::kContinue;
  };
  CHECK(double_array->Iterate(context(), double_callback, &data).IsJust());

  // An "object" in the ElementsKind sense.
  const Local<Value> kObject = String::NewFromUtf8Literal(isolate(), "foo");
  CHECK(object_array->Set(context(), kIndex, kObject).FromJust());
  Array::IterationCallback object_callback =
      [](uint32_t index, Local<Value> element, void* data) -> CbResult {
    Data* d = reinterpret_cast<Data*>(data);
    CHECK_EQ(42, d->sentinel);
    if (index != kIndex) {
      CHECK(element->IsUndefined());
      return CbResult::kContinue;
    }
    CHECK_EQ(kIndex, index);
    Local<String> str = element->ToString(d->context).ToLocalChecked();
    CHECK_EQ(0, strcmp("foo", *String::Utf8Value(d->isolate, str)));
    return CbResult::kContinue;
  };
  CHECK(object_array->Iterate(context(), object_callback, &data).IsJust());

  Local<String> zero = String::NewFromUtf8Literal(isolate(), "0");
  CHECK(dictionary_array->DefineOwnProperty(context(), zero, kSmi, v8::ReadOnly)
            .FromJust());
  Array::IterationCallback dictionary_callback =
      [](uint32_t index, Local<Value> element, void* data) -> CbResult {
    Data* d = reinterpret_cast<Data*>(data);
    CHECK_EQ(42, d->sentinel);
    CHECK_EQ(0, index);
    CHECK_EQ(333, element->NumberValue(d->context).FromJust());
    return CbResult::kContinue;
  };
  CHECK(dictionary_array->Iterate(context(), dictionary_callback, &data)
            .IsJust());
}

static void GetElement(Local<Name> name,
                       const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  Isolate* isolate = info.GetIsolate();
  Local<String> zero_str = String::NewFromUtf8Literal(isolate, "0");
  Local<Value> zero_num = Number::New(isolate, 123);
  CHECK(name->Equals(isolate->GetCurrentContext(), zero_str).FromJust());
  info.GetReturnValue().Set(zero_num);
}

TEST_F(ArrayTest, IterateAccessorElements) {
  HandleScope scope(isolate());
  // {SetAccessor} doesn't automatically set the length.
  Local<Array> array = Array::New(isolate(), 1);
  struct Data {
    int sentinel;
    Local<Context> context;
    Isolate* isolate;
  };
  Data data{42, context(), isolate()};
  Local<String> zero = String::NewFromUtf8Literal(isolate(), "0");
  CHECK(array->SetNativeDataProperty(context(), zero, GetElement).FromJust());
  Array::IterationCallback callback = [](uint32_t index, Local<Value> element,
                                         void* data) -> CbResult {
    Data* d = reinterpret_cast<Data*>(data);
    CHECK_EQ(0, index);
    CHECK_EQ(123, element->NumberValue(d->context).FromJust());
    d->sentinel = 234;
    return CbResult::kContinue;
  };
  CHECK(array->Iterate(context(), callback, &data).IsJust());
  CHECK_EQ(234, data.sentinel);  // Callback has been called at least once.
}

TEST_F(ArrayTest, IterateEarlyTermination) {
  HandleScope scope(isolate());
  Local<Array> array = Array::New(isolate());
  const Local<Value> kValue = Number::New(isolate(), 333);
  CHECK(array->Set(context(), 0, kValue).FromJust());
  CHECK(array->Set(context(), 1, kValue).FromJust());
  CHECK(array->Set(context(), 2, kValue).FromJust());

  Array::IterationCallback exception_callback =
      [](uint32_t index, Local<Value> element, void* data) -> CbResult {
    CHECK_EQ(0, index);
    return CbResult::kException;
  };
  CHECK(array->Iterate(context(), exception_callback, nullptr).IsNothing());

  Array::IterationCallback break_callback =
      [](uint32_t index, Local<Value> element, void* data) -> CbResult {
    CHECK_EQ(0, index);
    return CbResult::kBreak;
  };
  CHECK(array->Iterate(context(), break_callback, nullptr).IsJust());
}

TEST_F(ArrayTest, IterateWithUndefined) {
  Local<Array> array = internal::interpreter::CompileRun(
                           "(function() { return [0.2,undefined,8.1]; })()")
                           .As<Array>();
  CHECK(array->IsArray());

  struct Data {
    Local<Context> context;
  };
  Data data{context()};
  Array::IterationCallback callback = [](uint32_t index, Local<Value> element,
                                         void* data) -> CbResult {
    Data* d = reinterpret_cast<Data*>(data);
    switch (index) {
      case 0:
        CHECK_EQ(element->NumberValue(d->context).FromJust(), 0.2);
        break;
      case 1:
        CHECK(element->IsUndefined());
        break;
      case 2:
        CHECK_EQ(element->NumberValue(d->context).FromJust(), 8.1);
        break;
      default:
        UNREACHABLE();
    }
    return CbResult::kContinue;
  };
  CHECK(array->Iterate(context(), callback, &data).IsJust());
}

TEST_F(ArrayTest,
       IteratorAttributeChangeShouldNotInvalidateArrayIteratorProtectCell) {
  HandleScope handle_scope(isolate());
  const char source[] = R"(
    ("use strict");
    let threw = false;
    try {
      Object.defineProperty(Array.prototype, Symbol.iterator, {
        writable: false,
        configurable: false,
        enumerable: true,
      });
    } catch (e) {
      threw = e instanceof TypeError;
    }
    threw;
  )";

  Local<Value> result = internal::interpreter::CompileRun(source);
  CHECK(result->IsBoolean() && !result.As<Boolean>()->Value());

  CHECK(internal::Protectors::IsArrayIteratorLookupChainIntact(
      internal::Isolate::Current()));
}

TEST_F(ArrayTest, IteratorValueChangeShouldInvalidateArrayIteratorProtectCell) {
  HandleScope handle_scope(isolate());
  const char source[] = R"(
    ("use strict");
    let threw = false;
    try {
      Object.defineProperty(Array.prototype, Symbol.iterator, {
        value: 42,
      });
    } catch (e) {
      threw = e instanceof TypeError;
    }
    threw;
  )";

  Local<Value> result = internal::interpreter::CompileRun(source);
  CHECK(result->IsBoolean() && !result.As<Boolean>()->Value());

  CHECK(!internal::Protectors::IsArrayIteratorLookupChainIntact(
      internal::Isolate::Current()));
}

TEST_F(ArrayTest, IterateWithGC) {
  i::v8_flags.expose_gc = true;
  HandleScope scope(isolate());

  struct ArrayTestCase {
    const char* js_source;
    internal::ElementsKind expected_kind;
    std::vector<std::string> expected_values;
  };

  const ArrayTestCase test_cases[] = {
      // PACKED_SMI_ELEMENTS
      {"[10, 20, 30]", internal::PACKED_SMI_ELEMENTS, {"10", "20", "30"}},
      // HOLEY_SMI_ELEMENTS
      {"(() => { const a = [10, 20, 30]; delete a[1]; return a; })()",
       internal::HOLEY_SMI_ELEMENTS,
       {"10", "undefined", "30"}},
      // PACKED_ELEMENTS
      {"['foo', 'bar', 'baz']",
       internal::PACKED_ELEMENTS,
       {"foo", "bar", "baz"}},
      // HOLEY_ELEMENTS
      {"(() => { const a = ['foo', 'bar', 'baz']; delete a[1]; return a; })()",
       internal::HOLEY_ELEMENTS,
       {"foo", "undefined", "baz"}},
      // PACKED_DOUBLE_ELEMENTS
      {"[1.5, 2.5, 3.5]",
       internal::PACKED_DOUBLE_ELEMENTS,
       {"1.5", "2.5", "3.5"}},
      // HOLEY_DOUBLE_ELEMENTS
      {"(() => { const a = [1.5, 2.5, 3.5]; delete a[1]; return a; })()",
       internal::HOLEY_DOUBLE_ELEMENTS,
       {"1.5", "undefined", "3.5"}},
      // PACKED_NONEXTENSIBLE_ELEMENTS (Object and Smi)
      {"Object.preventExtensions(['a', 'b', 'c'])",
       internal::PACKED_NONEXTENSIBLE_ELEMENTS,
       {"a", "b", "c"}},
      {"Object.preventExtensions([10, 20, 30])",
       internal::PACKED_NONEXTENSIBLE_ELEMENTS,
       {"10", "20", "30"}},
      // HOLEY_NONEXTENSIBLE_ELEMENTS (Object and Smi)
      {"(() => { const a = ['a', 'b', 'c']; delete a[1]; return "
       "Object.preventExtensions(a); })()",
       internal::HOLEY_NONEXTENSIBLE_ELEMENTS,
       {"a", "undefined", "c"}},
      {"(() => { const a = [10, 20, 30]; delete a[1]; return "
       "Object.preventExtensions(a); })()",
       internal::HOLEY_NONEXTENSIBLE_ELEMENTS,
       {"10", "undefined", "30"}},
      // PACKED_SEALED_ELEMENTS (Object and Smi)
      {"Object.seal(['a', 'b', 'c'])",
       internal::PACKED_SEALED_ELEMENTS,
       {"a", "b", "c"}},
      {"Object.seal([10, 20, 30])",
       internal::PACKED_SEALED_ELEMENTS,
       {"10", "20", "30"}},
      // HOLEY_SEALED_ELEMENTS (Object and Smi)
      {"(() => { const a = ['a', 'b', 'c']; delete a[1]; return "
       "Object.seal(a); })()",
       internal::HOLEY_SEALED_ELEMENTS,
       {"a", "undefined", "c"}},
      {"(() => { const a = [10, 20, 30]; delete a[1]; return "
       "Object.seal(a); })()",
       internal::HOLEY_SEALED_ELEMENTS,
       {"10", "undefined", "30"}},
      // PACKED_FROZEN_ELEMENTS (Object and Smi)
      {"Object.freeze(['a', 'b', 'c'])",
       internal::PACKED_FROZEN_ELEMENTS,
       {"a", "b", "c"}},
      {"Object.freeze([10, 20, 30])",
       internal::PACKED_FROZEN_ELEMENTS,
       {"10", "20", "30"}},
      // HOLEY_FROZEN_ELEMENTS (Object and Smi)
      {"(() => { const a = ['a', 'b', 'c']; delete a[1]; return "
       "Object.freeze(a); })()",
       internal::HOLEY_FROZEN_ELEMENTS,
       {"a", "undefined", "c"}},
      {"(() => { const a = [10, 20, 30]; delete a[1]; return "
       "Object.freeze(a); })()",
       internal::HOLEY_FROZEN_ELEMENTS,
       {"10", "undefined", "30"}},
      // DICTIONARY_ELEMENTS (Sparse array)
      {"(() => { const a = []; a[0] = 'a'; a[10000] = 'b'; return a; })()",
       internal::DICTIONARY_ELEMENTS,
       {"a", "b"}},
      // DICTIONARY_ELEMENTS (Dense array with reconfigured property)
      {"(() => { const a = [10, 20, 30]; Object.defineProperty(a, '0', "
       "{value: 10, configurable: false, writable: false}); return a; })()",
       internal::DICTIONARY_ELEMENTS,
       {"10", "20", "30"}},
      // Empty array (PACKED_SMI_ELEMENTS, length 0)
      {"[]", internal::PACKED_SMI_ELEMENTS, {}},
  };

  struct IterationData {
    Isolate* isolate;
    Local<Context> context;
    std::vector<std::pair<uint32_t, std::string>> visited;
  };

  for (const auto& tc : test_cases) {
    Local<Array> array = RunJS(tc.js_source).As<Array>();
    CHECK(array->IsArray());
    auto i_array = Utils::OpenDirectHandle(*array);
    CHECK_EQ(i_array->GetElementsKind(), tc.expected_kind);

    IterationData data{isolate(), context(), {}};
    Array::IterationCallback callback = [](uint32_t index, Local<Value> element,
                                           void* data_ptr) -> CbResult {
      IterationData* d = static_cast<IterationData*>(data_ptr);
      // Perform GC within the iteration callback.
      d->isolate->RequestGarbageCollectionForTesting(
          v8::Isolate::kFullGarbageCollection);

      // Also allocate some new objects in the heap.
      Local<String> allocated_str =
          String::NewFromUtf8Literal(d->isolate, "test_alloc");
      CHECK(!allocated_str.IsEmpty());

      std::string val_str;
      if (element->IsUndefined()) {
        val_str = "undefined";
      } else if (element->IsNumber()) {
        double val = element.As<Number>()->Value();
        if (val == static_cast<int64_t>(val)) {
          val_str = std::to_string(static_cast<int64_t>(val));
        } else {
          val_str = std::to_string(val);
          // Strip trailing zeroes for clean comparison with "1.5", "2.5", "3.5"
          val_str.erase(val_str.find_last_not_of('0') + 1, std::string::npos);
          if (val_str.back() == '.') val_str.pop_back();
        }
      } else if (element->IsString()) {
        String::Utf8Value utf8(d->isolate, element.As<String>());
        val_str = *utf8;
      } else {
        val_str = "unknown";
      }
      d->visited.emplace_back(index, val_str);
      return CbResult::kContinue;
    };

    CHECK(array->Iterate(context(), callback, &data).IsJust());

    if (tc.expected_kind == internal::DICTIONARY_ELEMENTS &&
        tc.expected_values.size() == 2 && tc.expected_values[0] == "a" &&
        tc.expected_values[1] == "b") {
      CHECK_EQ(data.visited.size(), 2);
      CHECK_EQ(data.visited[0].first, 0);
      CHECK_EQ(data.visited[0].second, "a");
      CHECK_EQ(data.visited[1].first, 10000);
      CHECK_EQ(data.visited[1].second, "b");
    } else {
      CHECK_EQ(data.visited.size(), tc.expected_values.size());
      for (size_t i = 0; i < tc.expected_values.size(); ++i) {
        CHECK_EQ(data.visited[i].first, static_cast<uint32_t>(i));
        CHECK_EQ(data.visited[i].second, tc.expected_values[i]);
      }
    }
  }
}

}  // namespace
}  // namespace v8
