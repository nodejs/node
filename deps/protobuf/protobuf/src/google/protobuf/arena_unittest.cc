// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <google/protobuf/arena.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/arena_test_util.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_arena.pb.h>
#include <google/protobuf/unittest_no_arena.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/message.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/wire_format_lite.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/strutil.h>


using proto2_arena_unittest::ArenaMessage;
using protobuf_unittest::TestAllExtensions;
using protobuf_unittest::TestAllTypes;
using protobuf_unittest::TestEmptyMessage;
using protobuf_unittest::TestOneof2;
using protobuf_unittest_no_arena::TestNoArenaMessage;

namespace google {
namespace protobuf {

class Notifier {
 public:
  Notifier() : count_(0) {}
  void Notify() { count_++; }
  int GetCount() { return count_; }

 private:
  int count_;
};

class SimpleDataType {
 public:
  SimpleDataType() : notifier_(NULL) {}
  void SetNotifier(Notifier* notifier) { notifier_ = notifier; }
  virtual ~SimpleDataType() {
    if (notifier_ != NULL) {
      notifier_->Notify();
    }
  };

 private:
  Notifier* notifier_;
};

// A simple class that does not allow copying and so cannot be used as a
// parameter type without "const &".
class PleaseDontCopyMe {
 public:
  explicit PleaseDontCopyMe(int value) : value_(value) {}

  int value() const { return value_; }

 private:
  int value_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(PleaseDontCopyMe);
};

// A class that takes four different types as constructor arguments.
class MustBeConstructedWithOneThroughFour {
 public:
  MustBeConstructedWithOneThroughFour(int one, const char* two,
                                      const std::string& three,
                                      const PleaseDontCopyMe* four)
      : one_(one), two_(two), three_(three), four_(four) {}

  int one_;
  const char* const two_;
  std::string three_;
  const PleaseDontCopyMe* four_;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MustBeConstructedWithOneThroughFour);
};

// A class that takes eight different types as constructor arguments.
class MustBeConstructedWithOneThroughEight {
 public:
  MustBeConstructedWithOneThroughEight(int one, const char* two,
                                       const std::string& three,
                                       const PleaseDontCopyMe* four, int five,
                                       const char* six,
                                       const std::string& seven,
                                       const std::string& eight)
      : one_(one),
        two_(two),
        three_(three),
        four_(four),
        five_(five),
        six_(six),
        seven_(seven),
        eight_(eight) {}

  int one_;
  const char* const two_;
  std::string three_;
  const PleaseDontCopyMe* four_;
  int five_;
  const char* const six_;
  std::string seven_;
  std::string eight_;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MustBeConstructedWithOneThroughEight);
};

TEST(ArenaTest, ArenaConstructable) {
  EXPECT_TRUE(Arena::is_arena_constructable<TestAllTypes>::type::value);
  EXPECT_TRUE(Arena::is_arena_constructable<const TestAllTypes>::type::value);
  EXPECT_FALSE(Arena::is_arena_constructable<TestNoArenaMessage>::type::value);
  EXPECT_FALSE(Arena::is_arena_constructable<Arena>::type::value);
}

TEST(ArenaTest, DestructorSkippable) {
  EXPECT_TRUE(Arena::is_destructor_skippable<TestAllTypes>::type::value);
  EXPECT_TRUE(Arena::is_destructor_skippable<const TestAllTypes>::type::value);
  EXPECT_FALSE(Arena::is_destructor_skippable<TestNoArenaMessage>::type::value);
  EXPECT_FALSE(Arena::is_destructor_skippable<Arena>::type::value);
}

TEST(ArenaTest, BasicCreate) {
  Arena arena;
  EXPECT_TRUE(Arena::Create<int32>(&arena) != NULL);
  EXPECT_TRUE(Arena::Create<int64>(&arena) != NULL);
  EXPECT_TRUE(Arena::Create<float>(&arena) != NULL);
  EXPECT_TRUE(Arena::Create<double>(&arena) != NULL);
  EXPECT_TRUE(Arena::Create<std::string>(&arena) != NULL);
  arena.Own(new int32);
  arena.Own(new int64);
  arena.Own(new float);
  arena.Own(new double);
  arena.Own(new std::string);
  arena.Own<int>(NULL);
  Notifier notifier;
  SimpleDataType* data = Arena::Create<SimpleDataType>(&arena);
  data->SetNotifier(&notifier);
  data = new SimpleDataType;
  data->SetNotifier(&notifier);
  arena.Own(data);
  arena.Reset();
  EXPECT_EQ(2, notifier.GetCount());
}

TEST(ArenaTest, CreateAndConstCopy) {
  Arena arena;
  const std::string s("foo");
  const std::string* s_copy = Arena::Create<std::string>(&arena, s);
  EXPECT_TRUE(s_copy != NULL);
  EXPECT_EQ("foo", s);
  EXPECT_EQ("foo", *s_copy);
}

TEST(ArenaTest, CreateAndNonConstCopy) {
  Arena arena;
  std::string s("foo");
  const std::string* s_copy = Arena::Create<std::string>(&arena, s);
  EXPECT_TRUE(s_copy != NULL);
  EXPECT_EQ("foo", s);
  EXPECT_EQ("foo", *s_copy);
}

TEST(ArenaTest, CreateAndMove) {
  Arena arena;
  std::string s("foo");
  const std::string* s_move = Arena::Create<std::string>(&arena, std::move(s));
  EXPECT_TRUE(s_move != NULL);
  EXPECT_TRUE(s.empty());  // NOLINT
  EXPECT_EQ("foo", *s_move);
}

TEST(ArenaTest, CreateWithFourConstructorArguments) {
  Arena arena;
  const std::string three("3");
  const PleaseDontCopyMe four(4);
  const MustBeConstructedWithOneThroughFour* new_object =
      Arena::Create<MustBeConstructedWithOneThroughFour>(&arena, 1, "2", three,
                                                         &four);
  EXPECT_TRUE(new_object != NULL);
  ASSERT_EQ(1, new_object->one_);
  ASSERT_STREQ("2", new_object->two_);
  ASSERT_EQ("3", new_object->three_);
  ASSERT_EQ(4, new_object->four_->value());
}

TEST(ArenaTest, CreateWithEightConstructorArguments) {
  Arena arena;
  const std::string three("3");
  const PleaseDontCopyMe four(4);
  const std::string seven("7");
  const std::string eight("8");
  const MustBeConstructedWithOneThroughEight* new_object =
      Arena::Create<MustBeConstructedWithOneThroughEight>(
          &arena, 1, "2", three, &four, 5, "6", seven, eight);
  EXPECT_TRUE(new_object != NULL);
  ASSERT_EQ(1, new_object->one_);
  ASSERT_STREQ("2", new_object->two_);
  ASSERT_EQ("3", new_object->three_);
  ASSERT_EQ(4, new_object->four_->value());
  ASSERT_EQ(5, new_object->five_);
  ASSERT_STREQ("6", new_object->six_);
  ASSERT_EQ("7", new_object->seven_);
  ASSERT_EQ("8", new_object->eight_);
}

class PleaseMoveMe {
 public:
  explicit PleaseMoveMe(const std::string& value) : value_(value) {}
  PleaseMoveMe(PleaseMoveMe&&) = default;
  PleaseMoveMe(const PleaseMoveMe&) = delete;

  const std::string& value() const { return value_; }

 private:
  std::string value_;
};

TEST(ArenaTest, CreateWithMoveArguments) {
  Arena arena;
  PleaseMoveMe one("1");
  const PleaseMoveMe* new_object =
      Arena::Create<PleaseMoveMe>(&arena, std::move(one));
  EXPECT_TRUE(new_object);
  ASSERT_EQ("1", new_object->value());
}

TEST(ArenaTest, InitialBlockTooSmall) {
  // Construct a small (64 byte) initial block of memory to be used by the
  // arena allocator; then, allocate an object which will not fit in the
  // initial block.
  std::vector<char> arena_block(96);
  ArenaOptions options;
  options.initial_block = &arena_block[0];
  options.initial_block_size = arena_block.size();
  Arena arena(options);

  char* p = Arena::CreateArray<char>(&arena, 96);
  uintptr_t allocation = reinterpret_cast<uintptr_t>(p);

  // Ensure that the arena allocator did not return memory pointing into the
  // initial block of memory.
  uintptr_t arena_start = reinterpret_cast<uintptr_t>(&arena_block[0]);
  uintptr_t arena_end = arena_start + arena_block.size();
  EXPECT_FALSE(allocation >= arena_start && allocation < arena_end);

  // Write to the memory we allocated; this should (but is not guaranteed to)
  // trigger a check for heap corruption if the object was allocated from the
  // initially-provided block.
  memset(p, '\0', 96);
}

TEST(ArenaTest, Parsing) {
  TestAllTypes original;
  TestUtil::SetAllFields(&original);

  // Test memory leak.
  Arena arena;
  TestAllTypes* arena_message = Arena::CreateMessage<TestAllTypes>(&arena);
  arena_message->ParseFromString(original.SerializeAsString());
  TestUtil::ExpectAllFieldsSet(*arena_message);

  // Test that string fields have nul terminator bytes (earlier bug).
  EXPECT_EQ(strlen(original.optional_string().c_str()),
            strlen(arena_message->optional_string().c_str()));
}

TEST(ArenaTest, UnknownFields) {
  TestAllTypes original;
  TestUtil::SetAllFields(&original);

  // Test basic parsing into (populating) and reading out of unknown fields on
  // an arena.
  Arena arena;
  TestEmptyMessage* arena_message =
      Arena::CreateMessage<TestEmptyMessage>(&arena);
  arena_message->ParseFromString(original.SerializeAsString());

  TestAllTypes copied;
  copied.ParseFromString(arena_message->SerializeAsString());
  TestUtil::ExpectAllFieldsSet(copied);

  // Exercise UFS manual manipulation (setters).
  arena_message = Arena::CreateMessage<TestEmptyMessage>(&arena);
  arena_message->mutable_unknown_fields()->AddVarint(
      TestAllTypes::kOptionalInt32FieldNumber, 42);
  copied.Clear();
  copied.ParseFromString(arena_message->SerializeAsString());
  EXPECT_TRUE(copied.has_optional_int32());
  EXPECT_EQ(42, copied.optional_int32());

  // Exercise UFS swap path.
  TestEmptyMessage* arena_message_2 =
      Arena::CreateMessage<TestEmptyMessage>(&arena);
  arena_message_2->Swap(arena_message);
  copied.Clear();
  copied.ParseFromString(arena_message_2->SerializeAsString());
  EXPECT_TRUE(copied.has_optional_int32());
  EXPECT_EQ(42, copied.optional_int32());

  // Test field manipulation.
  TestEmptyMessage* arena_message_3 =
      Arena::CreateMessage<TestEmptyMessage>(&arena);
  arena_message_3->mutable_unknown_fields()->AddVarint(1000, 42);
  arena_message_3->mutable_unknown_fields()->AddFixed32(1001, 42);
  arena_message_3->mutable_unknown_fields()->AddFixed64(1002, 42);
  arena_message_3->mutable_unknown_fields()->AddLengthDelimited(1003);
  arena_message_3->mutable_unknown_fields()->DeleteSubrange(0, 2);
  arena_message_3->mutable_unknown_fields()->DeleteByNumber(1002);
  arena_message_3->mutable_unknown_fields()->DeleteByNumber(1003);
  EXPECT_TRUE(arena_message_3->unknown_fields().empty());
}

TEST(ArenaTest, Swap) {
  Arena arena1;
  Arena arena2;
  TestAllTypes* arena1_message;
  TestAllTypes* arena2_message;

  // Case 1: Swap(), no UFS on either message, both messages on different
  // arenas. Arena pointers should remain the same after swap.
  arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  arena2_message = Arena::CreateMessage<TestAllTypes>(&arena2);
  arena1_message->Swap(arena2_message);
  EXPECT_EQ(&arena1, arena1_message->GetArena());
  EXPECT_EQ(&arena2, arena2_message->GetArena());

  // Case 2: Swap(), UFS on one message, both messages on different arenas.
  arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  arena2_message = Arena::CreateMessage<TestAllTypes>(&arena2);
  arena1_message->mutable_unknown_fields()->AddVarint(1, 42);
  arena1_message->Swap(arena2_message);
  EXPECT_EQ(&arena1, arena1_message->GetArena());
  EXPECT_EQ(&arena2, arena2_message->GetArena());
  EXPECT_EQ(0, arena1_message->unknown_fields().field_count());
  EXPECT_EQ(1, arena2_message->unknown_fields().field_count());
  EXPECT_EQ(42, arena2_message->unknown_fields().field(0).varint());

  // Case 3: Swap(), UFS on both messages, both messages on different arenas.
  arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  arena2_message = Arena::CreateMessage<TestAllTypes>(&arena2);
  arena1_message->mutable_unknown_fields()->AddVarint(1, 42);
  arena2_message->mutable_unknown_fields()->AddVarint(2, 84);
  arena1_message->Swap(arena2_message);
  EXPECT_EQ(&arena1, arena1_message->GetArena());
  EXPECT_EQ(&arena2, arena2_message->GetArena());
  EXPECT_EQ(1, arena1_message->unknown_fields().field_count());
  EXPECT_EQ(1, arena2_message->unknown_fields().field_count());
  EXPECT_EQ(84, arena1_message->unknown_fields().field(0).varint());
  EXPECT_EQ(42, arena2_message->unknown_fields().field(0).varint());
}

TEST(ArenaTest, ReflectionSwapFields) {
  Arena arena1;
  Arena arena2;
  TestAllTypes* arena1_message;
  TestAllTypes* arena2_message;

  // Case 1: messages on different arenas, only one message is set.
  arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  arena2_message = Arena::CreateMessage<TestAllTypes>(&arena2);
  TestUtil::SetAllFields(arena1_message);
  const Reflection* reflection = arena1_message->GetReflection();
  std::vector<const FieldDescriptor*> fields;
  reflection->ListFields(*arena1_message, &fields);
  reflection->SwapFields(arena1_message, arena2_message, fields);
  EXPECT_EQ(&arena1, arena1_message->GetArena());
  EXPECT_EQ(&arena2, arena2_message->GetArena());
  std::string output;
  arena1_message->SerializeToString(&output);
  EXPECT_EQ(0, output.size());
  TestUtil::ExpectAllFieldsSet(*arena2_message);
  reflection->SwapFields(arena1_message, arena2_message, fields);
  arena2_message->SerializeToString(&output);
  EXPECT_EQ(0, output.size());
  TestUtil::ExpectAllFieldsSet(*arena1_message);

  // Case 2: messages on different arenas, both messages are set.
  arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  arena2_message = Arena::CreateMessage<TestAllTypes>(&arena2);
  TestUtil::SetAllFields(arena1_message);
  TestUtil::SetAllFields(arena2_message);
  reflection->SwapFields(arena1_message, arena2_message, fields);
  EXPECT_EQ(&arena1, arena1_message->GetArena());
  EXPECT_EQ(&arena2, arena2_message->GetArena());
  TestUtil::ExpectAllFieldsSet(*arena1_message);
  TestUtil::ExpectAllFieldsSet(*arena2_message);

  // Case 3: messages on different arenas with different lifetimes.
  arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  {
    Arena arena3;
    TestAllTypes* arena3_message = Arena::CreateMessage<TestAllTypes>(&arena3);
    TestUtil::SetAllFields(arena3_message);
    reflection->SwapFields(arena1_message, arena3_message, fields);
  }
  TestUtil::ExpectAllFieldsSet(*arena1_message);

  // Case 4: one message on arena, the other on heap.
  arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  TestAllTypes message;
  TestUtil::SetAllFields(arena1_message);
  reflection->SwapFields(arena1_message, &message, fields);
  EXPECT_EQ(&arena1, arena1_message->GetArena());
  EXPECT_EQ(nullptr, message.GetArena());
  arena1_message->SerializeToString(&output);
  EXPECT_EQ(0, output.size());
  TestUtil::ExpectAllFieldsSet(message);
}

TEST(ArenaTest, SetAllocatedMessage) {
  Arena arena;
  TestAllTypes* arena_message = Arena::CreateMessage<TestAllTypes>(&arena);
  TestAllTypes::NestedMessage* nested = new TestAllTypes::NestedMessage;
  nested->set_bb(118);
  arena_message->set_allocated_optional_nested_message(nested);
  EXPECT_EQ(118, arena_message->optional_nested_message().bb());

  TestNoArenaMessage no_arena_message;
  EXPECT_FALSE(no_arena_message.has_arena_message());
  no_arena_message.set_allocated_arena_message(NULL);
  EXPECT_FALSE(no_arena_message.has_arena_message());
  no_arena_message.set_allocated_arena_message(new ArenaMessage);
  EXPECT_TRUE(no_arena_message.has_arena_message());
}

TEST(ArenaTest, ReleaseMessage) {
  Arena arena;
  TestAllTypes* arena_message = Arena::CreateMessage<TestAllTypes>(&arena);
  arena_message->mutable_optional_nested_message()->set_bb(118);
  std::unique_ptr<TestAllTypes::NestedMessage> nested(
      arena_message->release_optional_nested_message());
  EXPECT_EQ(118, nested->bb());

  TestAllTypes::NestedMessage* released_null =
      arena_message->release_optional_nested_message();
  EXPECT_EQ(NULL, released_null);
}

TEST(ArenaTest, SetAllocatedString) {
  Arena arena;
  TestAllTypes* arena_message = Arena::CreateMessage<TestAllTypes>(&arena);
  std::string* allocated_str = new std::string("hello");
  arena_message->set_allocated_optional_string(allocated_str);
  EXPECT_EQ("hello", arena_message->optional_string());
}

TEST(ArenaTest, ReleaseString) {
  Arena arena;
  TestAllTypes* arena_message = Arena::CreateMessage<TestAllTypes>(&arena);
  arena_message->set_optional_string("hello");
  std::unique_ptr<std::string> released_str(
      arena_message->release_optional_string());
  EXPECT_EQ("hello", *released_str);

  // Test default value.
}


TEST(ArenaTest, SwapBetweenArenasWithAllFieldsSet) {
  Arena arena1;
  TestAllTypes* arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  {
    Arena arena2;
    TestAllTypes* arena2_message = Arena::CreateMessage<TestAllTypes>(&arena2);
    TestUtil::SetAllFields(arena2_message);
    arena2_message->Swap(arena1_message);
    std::string output;
    arena2_message->SerializeToString(&output);
    EXPECT_EQ(0, output.size());
  }
  TestUtil::ExpectAllFieldsSet(*arena1_message);
}

TEST(ArenaTest, SwapBetweenArenaAndNonArenaWithAllFieldsSet) {
  TestAllTypes non_arena_message;
  TestUtil::SetAllFields(&non_arena_message);
  {
    Arena arena2;
    TestAllTypes* arena2_message = Arena::CreateMessage<TestAllTypes>(&arena2);
    TestUtil::SetAllFields(arena2_message);
    arena2_message->Swap(&non_arena_message);
    TestUtil::ExpectAllFieldsSet(*arena2_message);
    TestUtil::ExpectAllFieldsSet(non_arena_message);
  }
}

TEST(ArenaTest, UnsafeArenaSwap) {
  Arena shared_arena;
  TestAllTypes* message1 = Arena::CreateMessage<TestAllTypes>(&shared_arena);
  TestAllTypes* message2 = Arena::CreateMessage<TestAllTypes>(&shared_arena);
  TestUtil::SetAllFields(message1);
  message1->UnsafeArenaSwap(message2);
  TestUtil::ExpectAllFieldsSet(*message2);
}

TEST(ArenaTest, SwapBetweenArenasUsingReflection) {
  Arena arena1;
  TestAllTypes* arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  {
    Arena arena2;
    TestAllTypes* arena2_message = Arena::CreateMessage<TestAllTypes>(&arena2);
    TestUtil::SetAllFields(arena2_message);
    const Reflection* r = arena2_message->GetReflection();
    r->Swap(arena1_message, arena2_message);
    std::string output;
    arena2_message->SerializeToString(&output);
    EXPECT_EQ(0, output.size());
  }
  TestUtil::ExpectAllFieldsSet(*arena1_message);
}

TEST(ArenaTest, SwapBetweenArenaAndNonArenaUsingReflection) {
  TestAllTypes non_arena_message;
  TestUtil::SetAllFields(&non_arena_message);
  {
    Arena arena2;
    TestAllTypes* arena2_message = Arena::CreateMessage<TestAllTypes>(&arena2);
    TestUtil::SetAllFields(arena2_message);
    const Reflection* r = arena2_message->GetReflection();
    r->Swap(&non_arena_message, arena2_message);
    TestUtil::ExpectAllFieldsSet(*arena2_message);
    TestUtil::ExpectAllFieldsSet(non_arena_message);
  }
}

TEST(ArenaTest, ReleaseFromArenaMessageMakesCopy) {
  TestAllTypes::NestedMessage* nested_msg = NULL;
  std::string* nested_string = NULL;
  {
    Arena arena;
    TestAllTypes* arena_message = Arena::CreateMessage<TestAllTypes>(&arena);
    arena_message->mutable_optional_nested_message()->set_bb(42);
    *arena_message->mutable_optional_string() = "Hello";
    nested_msg = arena_message->release_optional_nested_message();
    nested_string = arena_message->release_optional_string();
  }
  EXPECT_EQ(42, nested_msg->bb());
  EXPECT_EQ("Hello", *nested_string);
  delete nested_msg;
  delete nested_string;
}

#if PROTOBUF_RTTI
TEST(ArenaTest, ReleaseFromArenaMessageUsingReflectionMakesCopy) {
  TestAllTypes::NestedMessage* nested_msg = NULL;
  // Note: no string: reflection API only supports releasing submessages.
  {
    Arena arena;
    TestAllTypes* arena_message = Arena::CreateMessage<TestAllTypes>(&arena);
    arena_message->mutable_optional_nested_message()->set_bb(42);
    const Reflection* r = arena_message->GetReflection();
    const FieldDescriptor* f = arena_message->GetDescriptor()->FindFieldByName(
        "optional_nested_message");
    nested_msg = static_cast<TestAllTypes::NestedMessage*>(
        r->ReleaseMessage(arena_message, f));
  }
  EXPECT_EQ(42, nested_msg->bb());
  delete nested_msg;
}
#endif  // PROTOBUF_RTTI

TEST(ArenaTest, SetAllocatedAcrossArenas) {
  Arena arena1;
  TestAllTypes* arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  TestAllTypes::NestedMessage* heap_submessage =
      new TestAllTypes::NestedMessage();
  heap_submessage->set_bb(42);
  arena1_message->set_allocated_optional_nested_message(heap_submessage);
  // Should keep same object and add to arena's Own()-list.
  EXPECT_EQ(heap_submessage, arena1_message->mutable_optional_nested_message());
  {
    Arena arena2;
    TestAllTypes::NestedMessage* arena2_submessage =
        Arena::CreateMessage<TestAllTypes::NestedMessage>(&arena2);
    arena2_submessage->set_bb(42);
    arena1_message->set_allocated_optional_nested_message(arena2_submessage);
    EXPECT_NE(arena2_submessage,
              arena1_message->mutable_optional_nested_message());
  }

  TestAllTypes::NestedMessage* arena1_submessage =
      Arena::CreateMessage<TestAllTypes::NestedMessage>(&arena1);
  arena1_submessage->set_bb(42);
  TestAllTypes* heap_message = new TestAllTypes;
  heap_message->set_allocated_optional_nested_message(arena1_submessage);
  EXPECT_NE(arena1_submessage, heap_message->mutable_optional_nested_message());
  delete heap_message;
}

TEST(ArenaTest, SetAllocatedAcrossArenasWithReflection) {
  // Same as above, with reflection.
  Arena arena1;
  TestAllTypes* arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  const Reflection* r = arena1_message->GetReflection();
  const Descriptor* d = arena1_message->GetDescriptor();
  const FieldDescriptor* msg_field =
      d->FindFieldByName("optional_nested_message");
  TestAllTypes::NestedMessage* heap_submessage =
      new TestAllTypes::NestedMessage();
  heap_submessage->set_bb(42);
  r->SetAllocatedMessage(arena1_message, heap_submessage, msg_field);
  // Should keep same object and add to arena's Own()-list.
  EXPECT_EQ(heap_submessage, arena1_message->mutable_optional_nested_message());
  {
    Arena arena2;
    TestAllTypes::NestedMessage* arena2_submessage =
        Arena::CreateMessage<TestAllTypes::NestedMessage>(&arena2);
    arena2_submessage->set_bb(42);
    r->SetAllocatedMessage(arena1_message, arena2_submessage, msg_field);
    EXPECT_NE(arena2_submessage,
              arena1_message->mutable_optional_nested_message());
  }

  TestAllTypes::NestedMessage* arena1_submessage =
      Arena::CreateMessage<TestAllTypes::NestedMessage>(&arena1);
  arena1_submessage->set_bb(42);
  TestAllTypes* heap_message = new TestAllTypes;
  r->SetAllocatedMessage(heap_message, arena1_submessage, msg_field);
  EXPECT_NE(arena1_submessage, heap_message->mutable_optional_nested_message());
  delete heap_message;
}

TEST(ArenaTest, AddAllocatedWithReflection) {
  Arena arena1;
  ArenaMessage* arena1_message = Arena::CreateMessage<ArenaMessage>(&arena1);
  const Reflection* r = arena1_message->GetReflection();
  const Descriptor* d = arena1_message->GetDescriptor();
  const FieldDescriptor* fd =
      d->FindFieldByName("repeated_import_no_arena_message");
  // Message with cc_enable_arenas = false;
  r->AddMessage(arena1_message, fd);
  r->AddMessage(arena1_message, fd);
  r->AddMessage(arena1_message, fd);
  EXPECT_EQ(3, r->FieldSize(*arena1_message, fd));
  // Message with cc_enable_arenas = true;
  fd = d->FindFieldByName("repeated_nested_message");
  r->AddMessage(arena1_message, fd);
  r->AddMessage(arena1_message, fd);
  r->AddMessage(arena1_message, fd);
  EXPECT_EQ(3, r->FieldSize(*arena1_message, fd));
}

TEST(ArenaTest, RepeatedPtrFieldAddClearedTest) {
  {
    RepeatedPtrField<TestAllTypes> repeated_field;
    EXPECT_TRUE(repeated_field.empty());
    EXPECT_EQ(0, repeated_field.size());
    // Ownership is passed to repeated_field.
    TestAllTypes* cleared = new TestAllTypes();
    repeated_field.AddCleared(cleared);
    EXPECT_TRUE(repeated_field.empty());
    EXPECT_EQ(0, repeated_field.size());
  }
  {
    RepeatedPtrField<TestAllTypes> repeated_field;
    EXPECT_TRUE(repeated_field.empty());
    EXPECT_EQ(0, repeated_field.size());
    // Ownership is passed to repeated_field.
    TestAllTypes* cleared = new TestAllTypes();
    repeated_field.AddAllocated(cleared);
    EXPECT_FALSE(repeated_field.empty());
    EXPECT_EQ(1, repeated_field.size());
  }
}

TEST(ArenaTest, AddAllocatedToRepeatedField) {
  // Heap->arena case.
  Arena arena1;
  TestAllTypes* arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  for (int i = 0; i < 10; i++) {
    TestAllTypes::NestedMessage* heap_submessage =
        new TestAllTypes::NestedMessage();
    heap_submessage->set_bb(42);
    arena1_message->mutable_repeated_nested_message()->AddAllocated(
        heap_submessage);
    // Should not copy object -- will use arena_->Own().
    EXPECT_EQ(heap_submessage, &arena1_message->repeated_nested_message(i));
    EXPECT_EQ(42, arena1_message->repeated_nested_message(i).bb());
  }

  // Arena1->Arena2 case.
  arena1_message->Clear();
  for (int i = 0; i < 10; i++) {
    Arena arena2;
    TestAllTypes::NestedMessage* arena2_submessage =
        Arena::CreateMessage<TestAllTypes::NestedMessage>(&arena2);
    arena2_submessage->set_bb(42);
    arena1_message->mutable_repeated_nested_message()->AddAllocated(
        arena2_submessage);
    // Should copy object.
    EXPECT_NE(arena2_submessage, &arena1_message->repeated_nested_message(i));
    EXPECT_EQ(42, arena1_message->repeated_nested_message(i).bb());
  }

  // Arena->heap case.
  TestAllTypes* heap_message = new TestAllTypes();
  for (int i = 0; i < 10; i++) {
    Arena arena2;
    TestAllTypes::NestedMessage* arena2_submessage =
        Arena::CreateMessage<TestAllTypes::NestedMessage>(&arena2);
    arena2_submessage->set_bb(42);
    heap_message->mutable_repeated_nested_message()->AddAllocated(
        arena2_submessage);
    // Should copy object.
    EXPECT_NE(arena2_submessage, &heap_message->repeated_nested_message(i));
    EXPECT_EQ(42, heap_message->repeated_nested_message(i).bb());
  }
  delete heap_message;

  // Heap-arena case for strings (which are not arena-allocated).
  arena1_message->Clear();
  for (int i = 0; i < 10; i++) {
    std::string* s = new std::string("Test");
    arena1_message->mutable_repeated_string()->AddAllocated(s);
    // Should not copy.
    EXPECT_EQ(s, &arena1_message->repeated_string(i));
    EXPECT_EQ("Test", arena1_message->repeated_string(i));
  }
}

TEST(ArenaTest, AddAllocatedToRepeatedFieldViaReflection) {
  // Heap->arena case.
  Arena arena1;
  TestAllTypes* arena1_message = Arena::CreateMessage<TestAllTypes>(&arena1);
  const Reflection* r = arena1_message->GetReflection();
  const Descriptor* d = arena1_message->GetDescriptor();
  const FieldDescriptor* fd = d->FindFieldByName("repeated_nested_message");
  for (int i = 0; i < 10; i++) {
    TestAllTypes::NestedMessage* heap_submessage =
        new TestAllTypes::NestedMessage;
    heap_submessage->set_bb(42);
    r->AddAllocatedMessage(arena1_message, fd, heap_submessage);
    // Should not copy object -- will use arena_->Own().
    EXPECT_EQ(heap_submessage, &arena1_message->repeated_nested_message(i));
    EXPECT_EQ(42, arena1_message->repeated_nested_message(i).bb());
  }

  // Arena1->Arena2 case.
  arena1_message->Clear();
  for (int i = 0; i < 10; i++) {
    Arena arena2;
    TestAllTypes::NestedMessage* arena2_submessage =
        Arena::CreateMessage<TestAllTypes::NestedMessage>(&arena2);
    arena2_submessage->set_bb(42);
    r->AddAllocatedMessage(arena1_message, fd, arena2_submessage);
    // Should copy object.
    EXPECT_NE(arena2_submessage, &arena1_message->repeated_nested_message(i));
    EXPECT_EQ(42, arena1_message->repeated_nested_message(i).bb());
  }

  // Arena->heap case.
  TestAllTypes* heap_message = new TestAllTypes;
  for (int i = 0; i < 10; i++) {
    Arena arena2;
    TestAllTypes::NestedMessage* arena2_submessage =
        Arena::CreateMessage<TestAllTypes::NestedMessage>(&arena2);
    arena2_submessage->set_bb(42);
    r->AddAllocatedMessage(heap_message, fd, arena2_submessage);
    // Should copy object.
    EXPECT_NE(arena2_submessage, &heap_message->repeated_nested_message(i));
    EXPECT_EQ(42, heap_message->repeated_nested_message(i).bb());
  }
  delete heap_message;
}

TEST(ArenaTest, ReleaseLastRepeatedField) {
  // Release from arena-allocated repeated field and ensure that returned object
  // is heap-allocated.
  Arena arena;
  TestAllTypes* arena_message = Arena::CreateMessage<TestAllTypes>(&arena);
  for (int i = 0; i < 10; i++) {
    TestAllTypes::NestedMessage* nested =
        Arena::CreateMessage<TestAllTypes::NestedMessage>(&arena);
    nested->set_bb(42);
    arena_message->mutable_repeated_nested_message()->AddAllocated(nested);
  }

  for (int i = 0; i < 10; i++) {
    const TestAllTypes::NestedMessage* orig_submessage =
        &arena_message->repeated_nested_message(10 - 1 - i);  // last element
    TestAllTypes::NestedMessage* released =
        arena_message->mutable_repeated_nested_message()->ReleaseLast();
    EXPECT_NE(released, orig_submessage);
    EXPECT_EQ(42, released->bb());
    delete released;
  }

  // Test UnsafeArenaReleaseLast().
  for (int i = 0; i < 10; i++) {
    TestAllTypes::NestedMessage* nested =
        Arena::CreateMessage<TestAllTypes::NestedMessage>(&arena);
    nested->set_bb(42);
    arena_message->mutable_repeated_nested_message()->AddAllocated(nested);
  }

  for (int i = 0; i < 10; i++) {
    const TestAllTypes::NestedMessage* orig_submessage =
        &arena_message->repeated_nested_message(10 - 1 - i);  // last element
    TestAllTypes::NestedMessage* released =
        arena_message->mutable_repeated_nested_message()
            ->UnsafeArenaReleaseLast();
    EXPECT_EQ(released, orig_submessage);
    EXPECT_EQ(42, released->bb());
    // no delete -- |released| is on the arena.
  }

  // Test string case as well. ReleaseLast() in this case must copy the
  // string, even though it was originally heap-allocated and its pointer
  // was simply appended to the repeated field's internal vector, because the
  // string was placed on the arena's destructor list and cannot be removed
  // from that list (so the arena permanently owns the original instance).
  arena_message->Clear();
  for (int i = 0; i < 10; i++) {
    std::string* s = new std::string("Test");
    arena_message->mutable_repeated_string()->AddAllocated(s);
  }
  for (int i = 0; i < 10; i++) {
    const std::string* orig_element =
        &arena_message->repeated_string(10 - 1 - i);
    std::string* released =
        arena_message->mutable_repeated_string()->ReleaseLast();
    EXPECT_NE(released, orig_element);
    EXPECT_EQ("Test", *released);
    delete released;
  }
}

TEST(ArenaTest, UnsafeArenaReleaseAdd) {
  // Use unsafe_arena_release() and unsafe_arena_set_allocated() to transfer an
  // arena-allocated string from one message to another.
  const char kContent[] = "Test content";

  Arena arena;
  TestAllTypes* message1 = Arena::CreateMessage<TestAllTypes>(&arena);
  TestAllTypes* message2 = Arena::CreateMessage<TestAllTypes>(&arena);
  std::string* arena_string = Arena::Create<std::string>(&arena);
  *arena_string = kContent;

  message1->unsafe_arena_set_allocated_optional_string(arena_string);
  message2->unsafe_arena_set_allocated_optional_string(
      message1->unsafe_arena_release_optional_string());
  EXPECT_EQ(kContent, message2->optional_string());
}

TEST(ArenaTest, UnsafeArenaAddAllocated) {
  Arena arena;
  TestAllTypes* message = Arena::CreateMessage<TestAllTypes>(&arena);
  for (int i = 0; i < 10; i++) {
    std::string* arena_string = Arena::Create<std::string>(&arena);
    message->mutable_repeated_string()->UnsafeArenaAddAllocated(arena_string);
    EXPECT_EQ(arena_string, message->mutable_repeated_string(i));
  }
}

TEST(ArenaTest, UnsafeArenaRelease) {
  Arena arena;
  TestAllTypes* message = Arena::CreateMessage<TestAllTypes>(&arena);

  std::string* s = new std::string("test string");
  message->unsafe_arena_set_allocated_optional_string(s);
  EXPECT_TRUE(message->has_optional_string());
  EXPECT_EQ("test string", message->optional_string());
  s = message->unsafe_arena_release_optional_string();
  EXPECT_FALSE(message->has_optional_string());
  delete s;

  s = new std::string("test string");
  message->unsafe_arena_set_allocated_oneof_string(s);
  EXPECT_TRUE(message->has_oneof_string());
  EXPECT_EQ("test string", message->oneof_string());
  s = message->unsafe_arena_release_oneof_string();
  EXPECT_FALSE(message->has_oneof_string());
  delete s;
}

TEST(ArenaTest, OneofMerge) {
  Arena arena;
  TestAllTypes* message0 = Arena::CreateMessage<TestAllTypes>(&arena);
  TestAllTypes* message1 = Arena::CreateMessage<TestAllTypes>(&arena);

  message0->unsafe_arena_set_allocated_oneof_string(new std::string("x"));
  ASSERT_TRUE(message0->has_oneof_string());
  message1->unsafe_arena_set_allocated_oneof_string(new std::string("y"));
  ASSERT_TRUE(message1->has_oneof_string());
  EXPECT_EQ("x", message0->oneof_string());
  EXPECT_EQ("y", message1->oneof_string());
  message0->MergeFrom(*message1);
  EXPECT_EQ("y", message0->oneof_string());
  EXPECT_EQ("y", message1->oneof_string());
  delete message0->unsafe_arena_release_oneof_string();
  delete message1->unsafe_arena_release_oneof_string();
}

TEST(ArenaTest, ArenaOneofReflection) {
  Arena arena;
  TestAllTypes* message = Arena::CreateMessage<TestAllTypes>(&arena);
  const Descriptor* desc = message->GetDescriptor();
  const Reflection* refl = message->GetReflection();

  const FieldDescriptor* string_field = desc->FindFieldByName("oneof_string");
  const FieldDescriptor* msg_field =
      desc->FindFieldByName("oneof_nested_message");
  const OneofDescriptor* oneof = desc->FindOneofByName("oneof_field");

  refl->SetString(message, string_field, "Test value");
  EXPECT_TRUE(refl->HasOneof(*message, oneof));
  refl->ClearOneof(message, oneof);
  EXPECT_FALSE(refl->HasOneof(*message, oneof));

  Message* submsg = refl->MutableMessage(message, msg_field);
  EXPECT_TRUE(refl->HasOneof(*message, oneof));
  refl->ClearOneof(message, oneof);
  EXPECT_FALSE(refl->HasOneof(*message, oneof));
  refl->MutableMessage(message, msg_field);
  EXPECT_TRUE(refl->HasOneof(*message, oneof));
  submsg = refl->ReleaseMessage(message, msg_field);
  EXPECT_FALSE(refl->HasOneof(*message, oneof));
  EXPECT_TRUE(submsg->GetArena() == NULL);
  delete submsg;
}

void TestSwapRepeatedField(Arena* arena1, Arena* arena2) {
  // Test "safe" (copying) semantics for direct Swap() on RepeatedPtrField
  // between arenas.
  RepeatedPtrField<TestAllTypes> field1(arena1);
  RepeatedPtrField<TestAllTypes> field2(arena2);
  for (int i = 0; i < 10; i++) {
    TestAllTypes* t = Arena::CreateMessage<TestAllTypes>(arena1);
    t->set_optional_string("field1");
    t->set_optional_int32(i);
    if (arena1 != NULL) {
      field1.UnsafeArenaAddAllocated(t);
    } else {
      field1.AddAllocated(t);
    }
  }
  for (int i = 0; i < 5; i++) {
    TestAllTypes* t = Arena::CreateMessage<TestAllTypes>(arena2);
    t->set_optional_string("field2");
    t->set_optional_int32(i);
    if (arena2 != NULL) {
      field2.UnsafeArenaAddAllocated(t);
    } else {
      field2.AddAllocated(t);
    }
  }
  field1.Swap(&field2);
  EXPECT_EQ(5, field1.size());
  EXPECT_EQ(10, field2.size());
  EXPECT_TRUE(std::string("field1") == field2.Get(0).optional_string());
  EXPECT_TRUE(std::string("field2") == field1.Get(0).optional_string());
  // Ensure that fields retained their original order:
  for (int i = 0; i < field1.size(); i++) {
    EXPECT_EQ(i, field1.Get(i).optional_int32());
  }
  for (int i = 0; i < field2.size(); i++) {
    EXPECT_EQ(i, field2.Get(i).optional_int32());
  }
}

TEST(ArenaTest, SwapRepeatedField) {
  Arena arena;
  TestSwapRepeatedField(&arena, &arena);
}

TEST(ArenaTest, SwapRepeatedFieldWithDifferentArenas) {
  Arena arena1;
  Arena arena2;
  TestSwapRepeatedField(&arena1, &arena2);
}

TEST(ArenaTest, SwapRepeatedFieldWithNoArenaOnRightHandSide) {
  Arena arena;
  TestSwapRepeatedField(&arena, NULL);
}

TEST(ArenaTest, SwapRepeatedFieldWithNoArenaOnLeftHandSide) {
  Arena arena;
  TestSwapRepeatedField(NULL, &arena);
}

TEST(ArenaTest, ExtensionsOnArena) {
  Arena arena;
  // Ensure no leaks.
  TestAllExtensions* message_ext =
      Arena::CreateMessage<TestAllExtensions>(&arena);
  message_ext->SetExtension(protobuf_unittest::optional_int32_extension, 42);
  message_ext->SetExtension(protobuf_unittest::optional_string_extension,
                            std::string("test"));
  message_ext
      ->MutableExtension(protobuf_unittest::optional_nested_message_extension)
      ->set_bb(42);
}

TEST(ArenaTest, RepeatedFieldOnArena) {
  // Preallocate an initial arena block to avoid mallocs during hooked region.
  std::vector<char> arena_block(1024 * 1024);
  ArenaOptions options;
  options.initial_block = &arena_block[0];
  options.initial_block_size = arena_block.size();
  Arena arena(options);

  {
    internal::NoHeapChecker no_heap;

    // Fill some repeated fields on the arena to test for leaks. Also verify no
    // memory allocations.
    RepeatedField<int32> repeated_int32(&arena);
    RepeatedPtrField<TestAllTypes> repeated_message(&arena);
    for (int i = 0; i < 100; i++) {
      repeated_int32.Add(42);
      repeated_message.Add()->set_optional_int32(42);
      EXPECT_EQ(&arena, repeated_message.Get(0).GetArena());
      const TestAllTypes* msg_in_repeated_field = &repeated_message.Get(0);
      TestAllTypes* msg = repeated_message.UnsafeArenaReleaseLast();
      EXPECT_EQ(msg_in_repeated_field, msg);
    }

    // UnsafeArenaExtractSubrange (i) should not leak and (ii) should return
    // on-arena pointers.
    for (int i = 0; i < 10; i++) {
      repeated_message.Add()->set_optional_int32(42);
    }
    TestAllTypes* extracted_messages[5];
    repeated_message.UnsafeArenaExtractSubrange(0, 5, extracted_messages);
    EXPECT_EQ(&arena, repeated_message.Get(0).GetArena());
    EXPECT_EQ(5, repeated_message.size());
  }

  // Now, outside the scope of the NoHeapChecker, test ExtractSubrange's copying
  // semantics.
  {
    RepeatedPtrField<TestAllTypes> repeated_message(&arena);
    for (int i = 0; i < 100; i++) {
      repeated_message.Add()->set_optional_int32(42);
    }

    TestAllTypes* extracted_messages[5];
    // ExtractSubrange should copy to the heap.
    repeated_message.ExtractSubrange(0, 5, extracted_messages);
    EXPECT_EQ(NULL, extracted_messages[0]->GetArena());
    // We need to free the heap-allocated messages to prevent a leak.
    for (int i = 0; i < 5; i++) {
      delete extracted_messages[i];
      extracted_messages[i] = NULL;
    }
  }

  // Now check that we can create RepeatedFields/RepeatedPtrFields themselves on
  // the arena. They have the necessary type traits so that they can behave like
  // messages in this way. This is useful for higher-level generic templated
  // code that may allocate messages or repeated fields of messages on an arena.
  {
    RepeatedPtrField<TestAllTypes>* repeated_ptr_on_arena =
        Arena::CreateMessage<RepeatedPtrField<TestAllTypes> >(&arena);
    for (int i = 0; i < 10; i++) {
      // Add some elements and let the leak-checker ensure that everything is
      // freed.
      repeated_ptr_on_arena->Add();
    }

    RepeatedField<int>* repeated_int_on_arena =
        Arena::CreateMessage<RepeatedField<int> >(&arena);
    for (int i = 0; i < 100; i++) {
      repeated_int_on_arena->Add(i);
    }

  }

  arena.Reset();
}


#if PROTOBUF_RTTI
TEST(ArenaTest, MutableMessageReflection) {
  Arena arena;
  TestAllTypes* message = Arena::CreateMessage<TestAllTypes>(&arena);
  const Reflection* r = message->GetReflection();
  const Descriptor* d = message->GetDescriptor();
  const FieldDescriptor* field = d->FindFieldByName("optional_nested_message");
  TestAllTypes::NestedMessage* submessage =
      static_cast<TestAllTypes::NestedMessage*>(
          r->MutableMessage(message, field));
  TestAllTypes::NestedMessage* submessage_expected =
      message->mutable_optional_nested_message();

  EXPECT_EQ(submessage_expected, submessage);
  EXPECT_EQ(&arena, submessage->GetArena());

  const FieldDescriptor* oneof_field =
      d->FindFieldByName("oneof_nested_message");
  submessage = static_cast<TestAllTypes::NestedMessage*>(
      r->MutableMessage(message, oneof_field));
  submessage_expected = message->mutable_oneof_nested_message();

  EXPECT_EQ(submessage_expected, submessage);
  EXPECT_EQ(&arena, submessage->GetArena());
}
#endif  // PROTOBUF_RTTI


void FillArenaAwareFields(TestAllTypes* message) {
  std::string test_string = "hello world";
  message->set_optional_int32(42);
  message->set_optional_string(test_string);
  message->set_optional_bytes(test_string);
  message->mutable_optional_nested_message()->set_bb(42);

  message->set_oneof_uint32(42);
  message->mutable_oneof_nested_message()->set_bb(42);
  message->set_oneof_string(test_string);
  message->set_oneof_bytes(test_string);

  message->add_repeated_int32(42);
  // No repeated string: not yet arena-aware.
  message->add_repeated_nested_message()->set_bb(42);
  message->mutable_optional_lazy_message()->set_bb(42);
}

// Test: no allocations occur on heap while touching all supported field types.
TEST(ArenaTest, NoHeapAllocationsTest) {
  // Allocate a large initial block to avoid mallocs during hooked test.
  std::vector<char> arena_block(128 * 1024);
  ArenaOptions options;
  options.initial_block = &arena_block[0];
  options.initial_block_size = arena_block.size();
  Arena arena(options);

  {

    TestAllTypes* message = Arena::CreateMessage<TestAllTypes>(&arena);
    FillArenaAwareFields(message);
  }

  arena.Reset();
}

TEST(ArenaTest, ParseCorruptedString) {
  TestAllTypes message;
  TestUtil::SetAllFields(&message);
  TestParseCorruptedString<TestAllTypes, true>(message);
  TestParseCorruptedString<TestAllTypes, false>(message);
}

#if PROTOBUF_RTTI
// Test construction on an arena via generic MessageLite interface. We should be
// able to successfully deserialize on the arena without incurring heap
// allocations, i.e., everything should still be arena-allocation-aware.
TEST(ArenaTest, MessageLiteOnArena) {
  std::vector<char> arena_block(128 * 1024);
  ArenaOptions options;
  options.initial_block = &arena_block[0];
  options.initial_block_size = arena_block.size();
  Arena arena(options);
  const MessageLite* prototype = &TestAllTypes::default_instance();

  TestAllTypes initial_message;
  FillArenaAwareFields(&initial_message);
  std::string serialized;
  initial_message.SerializeToString(&serialized);

  {

    MessageLite* generic_message = prototype->New(&arena);
    EXPECT_TRUE(generic_message != NULL);
    EXPECT_EQ(&arena, generic_message->GetArena());
    EXPECT_TRUE(generic_message->ParseFromString(serialized));
    TestAllTypes* deserialized = static_cast<TestAllTypes*>(generic_message);
    EXPECT_EQ(42, deserialized->optional_int32());
  }

  arena.Reset();
}
#endif  // PROTOBUF_RTTI


// RepeatedField should support non-POD types, and invoke constructors and
// destructors appropriately, because it's used this way by lots of other code
// (even if this was not its original intent).
TEST(ArenaTest, RepeatedFieldWithNonPODType) {
  {
    RepeatedField<std::string> field_on_heap;
    for (int i = 0; i < 100; i++) {
      *field_on_heap.Add() = "test string long enough to exceed inline buffer";
    }
  }
  {
    Arena arena;
    RepeatedField<std::string> field_on_arena(&arena);
    for (int i = 0; i < 100; i++) {
      *field_on_arena.Add() = "test string long enough to exceed inline buffer";
    }
  }
}

// Align n to next multiple of 8
uint64 Align8(uint64 n) { return (n + 7) & -8; }

TEST(ArenaTest, SpaceAllocated_and_Used) {
  ArenaOptions options;
  options.start_block_size = 256;
  options.max_block_size = 8192;
  Arena arena_1(options);
  EXPECT_EQ(0, arena_1.SpaceAllocated());
  EXPECT_EQ(0, arena_1.SpaceUsed());
  EXPECT_EQ(0, arena_1.Reset());
  Arena::CreateArray<char>(&arena_1, 320);
  // Arena will allocate slightly more than 320 for the block headers.
  EXPECT_LE(320, arena_1.SpaceAllocated());
  EXPECT_EQ(Align8(320), arena_1.SpaceUsed());
  EXPECT_LE(320, arena_1.Reset());

  // Test with initial block.
  std::vector<char> arena_block(1024);
  options.initial_block = &arena_block[0];
  options.initial_block_size = arena_block.size();
  Arena arena_2(options);
  EXPECT_EQ(1024, arena_2.SpaceAllocated());
  EXPECT_EQ(0, arena_2.SpaceUsed());
  EXPECT_EQ(1024, arena_2.Reset());
  Arena::CreateArray<char>(&arena_2, 55);
  EXPECT_EQ(1024, arena_2.SpaceAllocated());
  EXPECT_EQ(Align8(55), arena_2.SpaceUsed());
  EXPECT_EQ(1024, arena_2.Reset());

  // Reset options to test doubling policy explicitly.
  options.initial_block = NULL;
  options.initial_block_size = 0;
  Arena arena_3(options);
  EXPECT_EQ(0, arena_3.SpaceUsed());
  Arena::CreateArray<char>(&arena_3, 160);
  EXPECT_EQ(256, arena_3.SpaceAllocated());
  EXPECT_EQ(Align8(160), arena_3.SpaceUsed());
  Arena::CreateArray<char>(&arena_3, 70);
  EXPECT_EQ(256 + 512, arena_3.SpaceAllocated());
  EXPECT_EQ(Align8(160) + Align8(70), arena_3.SpaceUsed());
  EXPECT_EQ(256 + 512, arena_3.Reset());
}

TEST(ArenaTest, Alignment) {
  Arena arena;
  for (int i = 0; i < 200; i++) {
    void* p = Arena::CreateArray<char>(&arena, i);
    GOOGLE_CHECK_EQ(reinterpret_cast<uintptr_t>(p) % 8, 0) << i << ": " << p;
  }
}

TEST(ArenaTest, BlockSizeSmallerThanAllocation) {
  for (size_t i = 0; i <= 8; ++i) {
    ArenaOptions opt;
    opt.start_block_size = opt.max_block_size = i;
    Arena arena(opt);

    *Arena::Create<int64>(&arena) = 42;
    EXPECT_GE(arena.SpaceAllocated(), 8);
    EXPECT_EQ(8, arena.SpaceUsed());

    *Arena::Create<int64>(&arena) = 42;
    EXPECT_GE(arena.SpaceAllocated(), 16);
    EXPECT_EQ(16, arena.SpaceUsed());
  }
}

TEST(ArenaTest, GetArenaShouldReturnTheArenaForArenaAllocatedMessages) {
  Arena arena;
  ArenaMessage* message = Arena::CreateMessage<ArenaMessage>(&arena);
  const ArenaMessage* const_pointer_to_message = message;
  EXPECT_EQ(&arena, Arena::GetArena(message));
  EXPECT_EQ(&arena, Arena::GetArena(const_pointer_to_message));

  // Test that the Message* / MessageLite* specialization SFINAE works.
  const Message* const_pointer_to_message_type = message;
  EXPECT_EQ(&arena, Arena::GetArena(const_pointer_to_message_type));
  const MessageLite* const_pointer_to_message_lite_type = message;
  EXPECT_EQ(&arena, Arena::GetArena(const_pointer_to_message_lite_type));
}

TEST(ArenaTest, GetArenaShouldReturnNullForNonArenaAllocatedMessages) {
  ArenaMessage message;
  const ArenaMessage* const_pointer_to_message = &message;
  EXPECT_EQ(NULL, Arena::GetArena(&message));
  EXPECT_EQ(NULL, Arena::GetArena(const_pointer_to_message));
}

TEST(ArenaTest, GetArenaShouldReturnNullForNonArenaCompatibleTypes) {
  TestNoArenaMessage message;
  const TestNoArenaMessage* const_pointer_to_message = &message;
  EXPECT_EQ(nullptr, Arena::GetArena(&message));
  EXPECT_EQ(nullptr, Arena::GetArena(const_pointer_to_message));

  // Test that GetArena returns nullptr for types that have a GetArena method
  // that doesn't return Arena*.
  struct {
    int GetArena() const { return 0; }
  } has_get_arena_method_wrong_return_type;
  EXPECT_EQ(nullptr, Arena::GetArena(&has_get_arena_method_wrong_return_type));

  // Test that GetArena returns nullptr for types that have a GetArena alias.
  struct {
    using GetArena = Arena*;
    GetArena unused;
  } has_get_arena_alias;
  EXPECT_EQ(nullptr, Arena::GetArena(&has_get_arena_alias));

  // Test that GetArena returns nullptr for types that have a GetArena data
  // member.
  struct {
    Arena GetArena;
  } has_get_arena_data_member;
  EXPECT_EQ(nullptr, Arena::GetArena(&has_get_arena_data_member));
}

TEST(ArenaTest, AddCleanup) {
  Arena arena;
  for (int i = 0; i < 100; i++) {
    arena.Own(new int);
  }
}

TEST(ArenaTest, UnsafeSetAllocatedOnArena) {
  Arena arena;
  TestAllTypes* message = Arena::CreateMessage<TestAllTypes>(&arena);
  EXPECT_FALSE(message->has_optional_string());

  std::string owned_string = "test with long enough content to heap-allocate";
  message->unsafe_arena_set_allocated_optional_string(&owned_string);
  EXPECT_TRUE(message->has_optional_string());

  message->unsafe_arena_set_allocated_optional_string(NULL);
  EXPECT_FALSE(message->has_optional_string());
}

// A helper utility class to only contain static hook functions, some
// counters to be used to verify the counters have been called and a cookie
// value to be verified.
class ArenaHooksTestUtil {
 public:
  static void* on_init(Arena* arena) {
    ++num_init;
    int* cookie = new int(kCookieValue);
    return static_cast<void*>(cookie);
  }

  static void on_allocation(const std::type_info* /*unused*/, uint64 alloc_size,
                            void* cookie) {
    ++num_allocations;
    int cookie_value = *static_cast<int*>(cookie);
    EXPECT_EQ(kCookieValue, cookie_value);
  }

  static void on_reset(Arena* arena, void* cookie, uint64 space_used) {
    ++num_reset;
    int cookie_value = *static_cast<int*>(cookie);
    EXPECT_EQ(kCookieValue, cookie_value);
  }

  static void on_destruction(Arena* arena, void* cookie, uint64 space_used) {
    ++num_destruct;
    int cookie_value = *static_cast<int*>(cookie);
    EXPECT_EQ(kCookieValue, cookie_value);
    delete static_cast<int*>(cookie);
  }

  static const int kCookieValue = 999;
  static uint32 num_init;
  static uint32 num_allocations;
  static uint32 num_reset;
  static uint32 num_destruct;
};
uint32 ArenaHooksTestUtil::num_init = 0;
uint32 ArenaHooksTestUtil::num_allocations = 0;
uint32 ArenaHooksTestUtil::num_reset = 0;
uint32 ArenaHooksTestUtil::num_destruct = 0;
const int ArenaHooksTestUtil::kCookieValue;

class ArenaOptionsTestFriend {
 public:
  static void Set(ArenaOptions* options) {
    options->on_arena_init = ArenaHooksTestUtil::on_init;
    options->on_arena_allocation = ArenaHooksTestUtil::on_allocation;
    options->on_arena_reset = ArenaHooksTestUtil::on_reset;
    options->on_arena_destruction = ArenaHooksTestUtil::on_destruction;
  }
};

// Test the hooks are correctly called and that the cookie is passed.
TEST(ArenaTest, ArenaHooksSanity) {
  ArenaOptions options;
  ArenaOptionsTestFriend::Set(&options);

  // Scope for defining the arena
  {
    Arena arena(options);
    EXPECT_EQ(1, ArenaHooksTestUtil::num_init);
    EXPECT_EQ(0, ArenaHooksTestUtil::num_allocations);
    Arena::Create<uint64>(&arena);
    if (std::is_trivially_destructible<uint64>::value) {
      EXPECT_EQ(1, ArenaHooksTestUtil::num_allocations);
    } else {
      EXPECT_EQ(2, ArenaHooksTestUtil::num_allocations);
    }
    arena.Reset();
    arena.Reset();
    EXPECT_EQ(2, ArenaHooksTestUtil::num_reset);
  }
  EXPECT_EQ(3, ArenaHooksTestUtil::num_reset);
  EXPECT_EQ(1, ArenaHooksTestUtil::num_destruct);
}


}  // namespace protobuf
}  // namespace google
