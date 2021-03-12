// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/init/v8.h"
#include "src/objects/function-kind.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

static void CheckObject(Isolate* isolate, Handle<Object> obj,
                        const char* string) {
  Handle<String> print_string = String::Flatten(
      isolate,
      Handle<String>::cast(Object::NoSideEffectsToString(isolate, obj)));
  CHECK(print_string->IsOneByteEqualTo(CStrVector(string)));
}

static void CheckSmi(Isolate* isolate, int value, const char* string) {
  Handle<Object> handle(Smi::FromInt(value), isolate);
  CheckObject(isolate, handle, string);
}

static void CheckString(Isolate* isolate, const char* value,
                        const char* string) {
  Handle<String> handle(isolate->factory()->NewStringFromAsciiChecked(value));
  CheckObject(isolate, handle, string);
}

static void CheckNumber(Isolate* isolate, double value, const char* string) {
  Handle<Object> number = isolate->factory()->NewNumber(value);
  CHECK(number->IsNumber());
  CheckObject(isolate, number, string);
}

static void CheckBoolean(Isolate* isolate, bool value, const char* string) {
  CheckObject(isolate, value ? isolate->factory()->true_value()
                             : isolate->factory()->false_value(),
              string);
}

TEST(NoSideEffectsToString) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  HandleScope scope(isolate);

  CheckString(isolate, "fisk hest", "fisk hest");
  CheckNumber(isolate, 42.3, "42.3");
  CheckSmi(isolate, 42, "42");
  CheckBoolean(isolate, true, "true");
  CheckBoolean(isolate, false, "false");
  CheckBoolean(isolate, false, "false");
  Handle<Object> smi_42 = handle(Smi::FromInt(42), isolate);
  CheckObject(isolate, BigInt::FromNumber(isolate, smi_42).ToHandleChecked(),
              "42");
  CheckObject(isolate, factory->undefined_value(), "undefined");
  CheckObject(isolate, factory->null_value(), "null");

  CheckObject(isolate, factory->error_to_string(), "[object Error]");
  CheckObject(isolate, factory->unscopables_symbol(),
              "Symbol(Symbol.unscopables)");
  CheckObject(isolate, factory->NewError(isolate->error_function(),
                                         factory->empty_string()),
              "Error");
  CheckObject(isolate, factory->NewError(
                           isolate->error_function(),
                           factory->NewStringFromAsciiChecked("fisk hest")),
              "Error: fisk hest");
  CheckObject(isolate, factory->NewJSObject(isolate->object_function()),
              "#<Object>");
}

TEST(EnumCache) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  i::Factory* factory = CcTest::i_isolate()->factory();
  v8::HandleScope scope(isolate);

  // Create a nice transition tree:
  // (a) --> (b) --> (c)   shared DescriptorArray 1
  //          |
  //          +---> (cc)   shared DescriptorArray 2
  CompileRun(
      "function O(a) { this.a = 1 };"

      "a = new O();"

      "b = new O();"
      "b.b = 2;"

      "c = new O();"
      "c.b = 2;"
      "c.c = 3;"

      "cc = new O();"
      "cc.b = 2;"
      "cc.cc = 4;");

  Handle<JSObject> a = Handle<JSObject>::cast(v8::Utils::OpenHandle(
      *env->Global()->Get(env.local(), v8_str("a")).ToLocalChecked()));
  Handle<JSObject> b = Handle<JSObject>::cast(v8::Utils::OpenHandle(
      *env->Global()->Get(env.local(), v8_str("b")).ToLocalChecked()));
  Handle<JSObject> c = Handle<JSObject>::cast(v8::Utils::OpenHandle(
      *env->Global()->Get(env.local(), v8_str("c")).ToLocalChecked()));
  Handle<JSObject> cc = Handle<JSObject>::cast(v8::Utils::OpenHandle(
      *env->Global()->Get(env.local(), v8_str("cc")).ToLocalChecked()));

  // Check the transition tree.
  CHECK_EQ(a->map().instance_descriptors(kRelaxedLoad),
           b->map().instance_descriptors(kRelaxedLoad));
  CHECK_EQ(b->map().instance_descriptors(kRelaxedLoad),
           c->map().instance_descriptors(kRelaxedLoad));
  CHECK_NE(c->map().instance_descriptors(kRelaxedLoad),
           cc->map().instance_descriptors(kRelaxedLoad));
  CHECK_NE(b->map().instance_descriptors(kRelaxedLoad),
           cc->map().instance_descriptors(kRelaxedLoad));

  // Check that the EnumLength is unset.
  CHECK_EQ(a->map().EnumLength(), kInvalidEnumCacheSentinel);
  CHECK_EQ(b->map().EnumLength(), kInvalidEnumCacheSentinel);
  CHECK_EQ(c->map().EnumLength(), kInvalidEnumCacheSentinel);
  CHECK_EQ(cc->map().EnumLength(), kInvalidEnumCacheSentinel);

  // Check that the EnumCache is empty.
  CHECK_EQ(a->map().instance_descriptors(kRelaxedLoad).enum_cache(),
           *factory->empty_enum_cache());
  CHECK_EQ(b->map().instance_descriptors(kRelaxedLoad).enum_cache(),
           *factory->empty_enum_cache());
  CHECK_EQ(c->map().instance_descriptors(kRelaxedLoad).enum_cache(),
           *factory->empty_enum_cache());
  CHECK_EQ(cc->map().instance_descriptors(kRelaxedLoad).enum_cache(),
           *factory->empty_enum_cache());

  // The EnumCache is shared on the DescriptorArray, creating it on {cc} has no
  // effect on the other maps.
  CompileRun("var s = 0; for (let key in cc) { s += cc[key] };");
  {
    CHECK_EQ(a->map().EnumLength(), kInvalidEnumCacheSentinel);
    CHECK_EQ(b->map().EnumLength(), kInvalidEnumCacheSentinel);
    CHECK_EQ(c->map().EnumLength(), kInvalidEnumCacheSentinel);
    CHECK_EQ(cc->map().EnumLength(), 3);

    CHECK_EQ(a->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             *factory->empty_enum_cache());
    CHECK_EQ(b->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             *factory->empty_enum_cache());
    CHECK_EQ(c->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             *factory->empty_enum_cache());

    EnumCache enum_cache =
        cc->map().instance_descriptors(kRelaxedLoad).enum_cache();
    CHECK_NE(enum_cache, *factory->empty_enum_cache());
    CHECK_EQ(enum_cache.keys().length(), 3);
    CHECK_EQ(enum_cache.indices().length(), 3);
  }

  // Initializing the EnumCache for the the topmost map {a} will not create the
  // cache for the other maps.
  CompileRun("var s = 0; for (let key in a) { s += a[key] };");
  {
    CHECK_EQ(a->map().EnumLength(), 1);
    CHECK_EQ(b->map().EnumLength(), kInvalidEnumCacheSentinel);
    CHECK_EQ(c->map().EnumLength(), kInvalidEnumCacheSentinel);
    CHECK_EQ(cc->map().EnumLength(), 3);

    // The enum cache is shared on the descriptor array of maps {a}, {b} and
    // {c} only.
    EnumCache enum_cache =
        a->map().instance_descriptors(kRelaxedLoad).enum_cache();
    CHECK_NE(enum_cache, *factory->empty_enum_cache());
    CHECK_NE(cc->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             *factory->empty_enum_cache());
    CHECK_NE(cc->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);
    CHECK_EQ(a->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);
    CHECK_EQ(b->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);
    CHECK_EQ(c->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);

    CHECK_EQ(enum_cache.keys().length(), 1);
    CHECK_EQ(enum_cache.indices().length(), 1);
  }

  // Creating the EnumCache for {c} will create a new EnumCache on the shared
  // DescriptorArray.
  Handle<EnumCache> previous_enum_cache(
      a->map().instance_descriptors(kRelaxedLoad).enum_cache(),
      a->GetIsolate());
  Handle<FixedArray> previous_keys(previous_enum_cache->keys(),
                                   a->GetIsolate());
  Handle<FixedArray> previous_indices(previous_enum_cache->indices(),
                                      a->GetIsolate());
  CompileRun("var s = 0; for (let key in c) { s += c[key] };");
  {
    CHECK_EQ(a->map().EnumLength(), 1);
    CHECK_EQ(b->map().EnumLength(), kInvalidEnumCacheSentinel);
    CHECK_EQ(c->map().EnumLength(), 3);
    CHECK_EQ(cc->map().EnumLength(), 3);

    EnumCache enum_cache =
        c->map().instance_descriptors(kRelaxedLoad).enum_cache();
    CHECK_NE(enum_cache, *factory->empty_enum_cache());
    // The keys and indices caches are updated.
    CHECK_EQ(enum_cache, *previous_enum_cache);
    CHECK_NE(enum_cache.keys(), *previous_keys);
    CHECK_NE(enum_cache.indices(), *previous_indices);
    CHECK_EQ(previous_keys->length(), 1);
    CHECK_EQ(previous_indices->length(), 1);
    CHECK_EQ(enum_cache.keys().length(), 3);
    CHECK_EQ(enum_cache.indices().length(), 3);

    // The enum cache is shared on the descriptor array of maps {a}, {b} and
    // {c} only.
    CHECK_NE(cc->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             *factory->empty_enum_cache());
    CHECK_NE(cc->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);
    CHECK_NE(cc->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             *previous_enum_cache);
    CHECK_EQ(a->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);
    CHECK_EQ(b->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);
    CHECK_EQ(c->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);
  }

  // {b} can reuse the existing EnumCache, hence we only need to set the correct
  // EnumLength on the map without modifying the cache itself.
  previous_enum_cache =
      handle(a->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             a->GetIsolate());
  previous_keys = handle(previous_enum_cache->keys(), a->GetIsolate());
  previous_indices = handle(previous_enum_cache->indices(), a->GetIsolate());
  CompileRun("var s = 0; for (let key in b) { s += b[key] };");
  {
    CHECK_EQ(a->map().EnumLength(), 1);
    CHECK_EQ(b->map().EnumLength(), 2);
    CHECK_EQ(c->map().EnumLength(), 3);
    CHECK_EQ(cc->map().EnumLength(), 3);

    EnumCache enum_cache =
        c->map().instance_descriptors(kRelaxedLoad).enum_cache();
    CHECK_NE(enum_cache, *factory->empty_enum_cache());
    // The keys and indices caches are not updated.
    CHECK_EQ(enum_cache, *previous_enum_cache);
    CHECK_EQ(enum_cache.keys(), *previous_keys);
    CHECK_EQ(enum_cache.indices(), *previous_indices);
    CHECK_EQ(enum_cache.keys().length(), 3);
    CHECK_EQ(enum_cache.indices().length(), 3);

    // The enum cache is shared on the descriptor array of maps {a}, {b} and
    // {c} only.
    CHECK_NE(cc->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             *factory->empty_enum_cache());
    CHECK_NE(cc->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);
    CHECK_NE(cc->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             *previous_enum_cache);
    CHECK_EQ(a->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);
    CHECK_EQ(b->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);
    CHECK_EQ(c->map().instance_descriptors(kRelaxedLoad).enum_cache(),
             enum_cache);
  }
}

TEST(ObjectMethodsThatTruncateMinusZero) {
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(env->GetIsolate());

  Handle<Object> minus_zero = factory->NewNumber(-1.0 * 0.0);
  CHECK(minus_zero->IsMinusZero());

  Handle<Object> result =
      Object::ToInteger(isolate, minus_zero).ToHandleChecked();
  CHECK(result->IsZero());

  result = Object::ToLength(isolate, minus_zero).ToHandleChecked();
  CHECK(result->IsZero());

  // Choose an error message template, doesn't matter which.
  result = Object::ToIndex(isolate, minus_zero,
                           MessageTemplate::kInvalidAtomicAccessIndex)
               .ToHandleChecked();
  CHECK(result->IsZero());
}

#define TEST_FUNCTION_KIND(Name)                                \
  TEST(Name) {                                                  \
    for (int i = 0; i < FunctionKind::kLastFunctionKind; i++) { \
      FunctionKind kind = static_cast<FunctionKind>(i);         \
      CHECK_EQ(FunctionKind##Name(kind), Name(kind));           \
    }                                                           \
  }

bool FunctionKindIsArrowFunction(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kArrowFunction:
    case FunctionKind::kAsyncArrowFunction:
      return true;
    default:
      return false;
  }
}
TEST_FUNCTION_KIND(IsArrowFunction)

bool FunctionKindIsAsyncGeneratorFunction(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kAsyncConciseGeneratorMethod:
    case FunctionKind::kStaticAsyncConciseGeneratorMethod:
    case FunctionKind::kAsyncGeneratorFunction:
      return true;
    default:
      return false;
  }
}
TEST_FUNCTION_KIND(IsAsyncGeneratorFunction)

bool FunctionKindIsGeneratorFunction(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kConciseGeneratorMethod:
    case FunctionKind::kStaticConciseGeneratorMethod:
    case FunctionKind::kAsyncConciseGeneratorMethod:
    case FunctionKind::kStaticAsyncConciseGeneratorMethod:
    case FunctionKind::kGeneratorFunction:
    case FunctionKind::kAsyncGeneratorFunction:
      return true;
    default:
      return false;
  }
}
TEST_FUNCTION_KIND(IsGeneratorFunction)

bool FunctionKindIsAsyncFunction(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kAsyncFunction:
    case FunctionKind::kAsyncArrowFunction:
    case FunctionKind::kAsyncConciseMethod:
    case FunctionKind::kStaticAsyncConciseMethod:
    case FunctionKind::kAsyncConciseGeneratorMethod:
    case FunctionKind::kStaticAsyncConciseGeneratorMethod:
    case FunctionKind::kAsyncGeneratorFunction:
      return true;
    default:
      return false;
  }
}
TEST_FUNCTION_KIND(IsAsyncFunction)

bool FunctionKindIsConciseMethod(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kConciseMethod:
    case FunctionKind::kStaticConciseMethod:
    case FunctionKind::kConciseGeneratorMethod:
    case FunctionKind::kStaticConciseGeneratorMethod:
    case FunctionKind::kAsyncConciseMethod:
    case FunctionKind::kStaticAsyncConciseMethod:
    case FunctionKind::kAsyncConciseGeneratorMethod:
    case FunctionKind::kStaticAsyncConciseGeneratorMethod:
    case FunctionKind::kClassMembersInitializerFunction:
      return true;
    default:
      return false;
  }
}
TEST_FUNCTION_KIND(IsConciseMethod)

bool FunctionKindIsAccessorFunction(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kGetterFunction:
    case FunctionKind::kStaticGetterFunction:
    case FunctionKind::kSetterFunction:
    case FunctionKind::kStaticSetterFunction:
      return true;
    default:
      return false;
  }
}
TEST_FUNCTION_KIND(IsAccessorFunction)

bool FunctionKindIsDefaultConstructor(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kDefaultBaseConstructor:
    case FunctionKind::kDefaultDerivedConstructor:
      return true;
    default:
      return false;
  }
}
TEST_FUNCTION_KIND(IsDefaultConstructor)

bool FunctionKindIsBaseConstructor(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kBaseConstructor:
    case FunctionKind::kDefaultBaseConstructor:
      return true;
    default:
      return false;
  }
}
TEST_FUNCTION_KIND(IsBaseConstructor)

bool FunctionKindIsDerivedConstructor(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kDefaultDerivedConstructor:
    case FunctionKind::kDerivedConstructor:
      return true;
    default:
      return false;
  }
}
TEST_FUNCTION_KIND(IsDerivedConstructor)

bool FunctionKindIsClassConstructor(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kBaseConstructor:
    case FunctionKind::kDefaultBaseConstructor:
    case FunctionKind::kDefaultDerivedConstructor:
    case FunctionKind::kDerivedConstructor:
      return true;
    default:
      return false;
  }
}
TEST_FUNCTION_KIND(IsClassConstructor)

bool FunctionKindIsConstructable(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kGetterFunction:
    case FunctionKind::kStaticGetterFunction:
    case FunctionKind::kSetterFunction:
    case FunctionKind::kStaticSetterFunction:
    case FunctionKind::kArrowFunction:
    case FunctionKind::kAsyncArrowFunction:
    case FunctionKind::kAsyncFunction:
    case FunctionKind::kAsyncConciseMethod:
    case FunctionKind::kStaticAsyncConciseMethod:
    case FunctionKind::kAsyncConciseGeneratorMethod:
    case FunctionKind::kStaticAsyncConciseGeneratorMethod:
    case FunctionKind::kAsyncGeneratorFunction:
    case FunctionKind::kGeneratorFunction:
    case FunctionKind::kConciseGeneratorMethod:
    case FunctionKind::kStaticConciseGeneratorMethod:
    case FunctionKind::kConciseMethod:
    case FunctionKind::kStaticConciseMethod:
    case FunctionKind::kClassMembersInitializerFunction:
      return false;
    default:
      return true;
  }
}
TEST_FUNCTION_KIND(IsConstructable)

bool FunctionKindIsStrictFunctionWithoutPrototype(FunctionKind kind) {
  return IsArrowFunction(kind) || IsConciseMethod(kind) ||
         IsAccessorFunction(kind);
}
TEST_FUNCTION_KIND(IsStrictFunctionWithoutPrototype)

#undef TEST_FUNCTION_KIND

}  // namespace internal
}  // namespace v8
