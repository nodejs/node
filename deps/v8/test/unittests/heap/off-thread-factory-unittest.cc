// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

#include "src/ast/ast-value-factory.h"
#include "src/handles/handles-inl.h"
#include "src/handles/handles.h"
#include "src/heap/off-thread-factory.h"
#include "src/objects/fixed-array.h"
#include "src/objects/string.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

class OffThreadFactoryTest : public TestWithIsolateAndZone {
 public:
  OffThreadFactoryTest()
      : TestWithIsolateAndZone(), off_thread_factory_(isolate()) {}

  OffThreadFactory* off_thread_factory() { return &off_thread_factory_; }

  // We only internalize strings which are referred to in other slots, so create
  // a wrapper pointing at the off_thread_string.
  OffThreadHandle<FixedArray> WrapString(OffThreadHandle<String> string) {
    // TODO(leszeks): Replace with a different factory method (e.g. FixedArray)
    // once OffThreadFactory supports it.
    return off_thread_factory()->StringWrapperForTest(string);
  }

 private:
  OffThreadFactory off_thread_factory_;
};

TEST_F(OffThreadFactoryTest, HandleOrOffThreadHandle_IsNullWhenConstructed) {
  // Default constructed HandleOrOffThreadHandles should be considered both null
  // and uninitialized.
  EXPECT_TRUE(HandleOrOffThreadHandle<HeapObject>().is_null());
#ifdef DEBUG
  EXPECT_TRUE(!HandleOrOffThreadHandle<HeapObject>().is_initialized());
#endif

  // Default constructed HandleOrOffThreadHandles should work as both null
  // handles and null off-thread handles.
  EXPECT_TRUE(HandleOrOffThreadHandle<HeapObject>().get<Factory>().is_null());
  EXPECT_TRUE(
      HandleOrOffThreadHandle<HeapObject>().get<OffThreadFactory>().is_null());
}

TEST_F(OffThreadFactoryTest, OneByteInternalizedString_IsAddedToStringTable) {
  Vector<const uint8_t> string_vector = StaticCharVector("foo");
  uint32_t hash_field = StringHasher::HashSequentialString<uint8_t>(
      string_vector.begin(), string_vector.length(), HashSeed(isolate()));

  OffThreadHandle<String> off_thread_string =
      off_thread_factory()->NewOneByteInternalizedString(string_vector,
                                                         hash_field);

  OffThreadHandle<FixedArray> off_thread_wrapper =
      off_thread_factory()->StringWrapperForTest(off_thread_string);

  off_thread_factory()->FinishOffThread();

  Handle<FixedArray> wrapper = handle(*off_thread_wrapper, isolate());
  off_thread_factory()->Publish(isolate());

  Handle<String> string = handle(String::cast(wrapper->get(0)), isolate());

  EXPECT_TRUE(string->IsOneByteEqualTo(CStrVector("foo")));
  EXPECT_TRUE(string->IsInternalizedString());

  Handle<String> same_string = isolate()
                                   ->factory()
                                   ->NewStringFromOneByte(string_vector)
                                   .ToHandleChecked();
  EXPECT_NE(*string, *same_string);
  EXPECT_FALSE(same_string->IsInternalizedString());

  Handle<String> internalized_string =
      isolate()->factory()->InternalizeString(same_string);
  EXPECT_EQ(*string, *internalized_string);
}

TEST_F(OffThreadFactoryTest,
       OneByteInternalizedString_DuplicateIsDeduplicated) {
  Vector<const uint8_t> string_vector = StaticCharVector("foo");
  uint32_t hash_field = StringHasher::HashSequentialString<uint8_t>(
      string_vector.begin(), string_vector.length(), HashSeed(isolate()));

  OffThreadHandle<String> off_thread_string_1 =
      off_thread_factory()->NewOneByteInternalizedString(string_vector,
                                                         hash_field);
  OffThreadHandle<String> off_thread_string_2 =
      off_thread_factory()->NewOneByteInternalizedString(string_vector,
                                                         hash_field);

  OffThreadHandle<FixedArray> off_thread_wrapper_1 =
      WrapString(off_thread_string_1);
  OffThreadHandle<FixedArray> off_thread_wrapper_2 =
      WrapString(off_thread_string_2);

  off_thread_factory()->FinishOffThread();

  Handle<FixedArray> wrapper_1 = handle(*off_thread_wrapper_1, isolate());
  Handle<FixedArray> wrapper_2 = handle(*off_thread_wrapper_2, isolate());
  off_thread_factory()->Publish(isolate());

  Handle<String> string_1 = handle(String::cast(wrapper_1->get(0)), isolate());
  Handle<String> string_2 = handle(String::cast(wrapper_2->get(0)), isolate());

  EXPECT_TRUE(string_1->IsOneByteEqualTo(CStrVector("foo")));
  EXPECT_TRUE(string_1->IsInternalizedString());
  EXPECT_EQ(*string_1, *string_2);
}

TEST_F(OffThreadFactoryTest, AstRawString_IsInternalized) {
  AstValueFactory ast_value_factory(zone(), isolate()->ast_string_constants(),
                                    HashSeed(isolate()));

  const AstRawString* raw_string = ast_value_factory.GetOneByteString("foo");

  ast_value_factory.Internalize(off_thread_factory());

  OffThreadHandle<FixedArray> off_thread_wrapper =
      WrapString(raw_string->string().get<OffThreadFactory>());

  off_thread_factory()->FinishOffThread();

  Handle<FixedArray> wrapper = handle(*off_thread_wrapper, isolate());
  off_thread_factory()->Publish(isolate());

  Handle<String> string = handle(String::cast(wrapper->get(0)), isolate());

  EXPECT_TRUE(string->IsOneByteEqualTo(CStrVector("foo")));
  EXPECT_TRUE(string->IsInternalizedString());
}

TEST_F(OffThreadFactoryTest, AstConsString_CreatesConsString) {
  AstValueFactory ast_value_factory(zone(), isolate()->ast_string_constants(),
                                    HashSeed(isolate()));

  const AstRawString* foo_string = ast_value_factory.GetOneByteString("foo");
  const AstRawString* bar_string =
      ast_value_factory.GetOneByteString("bar-plus-padding-for-length");
  const AstConsString* foobar_string =
      ast_value_factory.NewConsString(foo_string, bar_string);

  ast_value_factory.Internalize(off_thread_factory());

  OffThreadHandle<FixedArray> off_thread_wrapper =
      WrapString(foobar_string->string().get<OffThreadFactory>());

  off_thread_factory()->FinishOffThread();

  Handle<FixedArray> wrapper = handle(*off_thread_wrapper, isolate());
  off_thread_factory()->Publish(isolate());

  Handle<String> string = handle(String::cast(wrapper->get(0)), isolate());

  EXPECT_TRUE(string->IsConsString());
  EXPECT_TRUE(string->Equals(*isolate()->factory()->NewStringFromStaticChars(
      "foobar-plus-padding-for-length")));
}

}  // namespace internal
}  // namespace v8
