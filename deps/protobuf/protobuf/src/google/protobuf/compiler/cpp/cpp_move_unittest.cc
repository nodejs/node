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

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/unittest.pb.h>
#include <gtest/gtest.h>

#if LANG_CXX11
#include <type_traits>
#endif

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

// Can't use an anonymous namespace here due to brokenness of Tru64 compiler.
namespace cpp_unittest {

// Moves are enabled only when compiling with a C++11 compiler or newer.
#if LANG_CXX11

TEST(MovableMessageTest, MoveConstructor) {
  protobuf_unittest::TestAllTypes message1;
  TestUtil::SetAllFields(&message1);
  const auto* nested = &message1.optional_nested_message();

  protobuf_unittest::TestAllTypes message2(std::move(message1));
  TestUtil::ExpectAllFieldsSet(message2);

  // Check if the optional_nested_message was actually moved (and not just
  // copied).
  EXPECT_EQ(nested, &message2.optional_nested_message());
  EXPECT_NE(nested, &message1.optional_nested_message());
}

TEST(MovableMessageTest, MoveAssignmentOperator) {
  protobuf_unittest::TestAllTypes message1;
  TestUtil::SetAllFields(&message1);
  const auto* nested = &message1.optional_nested_message();

  protobuf_unittest::TestAllTypes message2;
  message2 = std::move(message1);
  TestUtil::ExpectAllFieldsSet(message2);

  // Check if the optional_nested_message was actually moved (and not just
  // copied).
  EXPECT_EQ(nested, &message2.optional_nested_message());
  EXPECT_NE(nested, &message1.optional_nested_message());
}

TEST(MovableMessageTest, SelfMoveAssignment) {
  // The `self` reference is necessary to defeat -Wself-move.
  protobuf_unittest::TestAllTypes message, &self = message;
  TestUtil::SetAllFields(&message);
  message = std::move(self);
  TestUtil::ExpectAllFieldsSet(message);
}

TEST(MovableMessageTest, MoveSameArena) {
  Arena arena;

  auto* message1_on_arena =
      Arena::CreateMessage<protobuf_unittest::TestAllTypes>(&arena);
  TestUtil::SetAllFields(message1_on_arena);
  const auto* nested = &message1_on_arena->optional_nested_message();

  auto* message2_on_arena =
      Arena::CreateMessage<protobuf_unittest::TestAllTypes>(&arena);

  // Moving messages on the same arena should lead to swapped pointers.
  *message2_on_arena = std::move(*message1_on_arena);
  EXPECT_EQ(nested, &message2_on_arena->optional_nested_message());
}

TEST(MovableMessageTest, MoveDifferentArenas) {
  Arena arena1, arena2;

  auto* message1_on_arena =
      Arena::CreateMessage<protobuf_unittest::TestAllTypes>(&arena1);
  TestUtil::SetAllFields(message1_on_arena);
  const auto* nested = &message1_on_arena->optional_nested_message();

  auto* message2_on_arena =
      Arena::CreateMessage<protobuf_unittest::TestAllTypes>(&arena2);

  // Moving messages on two different arenas should lead to a copy.
  *message2_on_arena = std::move(*message1_on_arena);
  EXPECT_NE(nested, &message2_on_arena->optional_nested_message());
  TestUtil::ExpectAllFieldsSet(*message1_on_arena);
  TestUtil::ExpectAllFieldsSet(*message2_on_arena);
}

TEST(MovableMessageTest, MoveFromArena) {
  Arena arena;

  auto* message1_on_arena =
      Arena::CreateMessage<protobuf_unittest::TestAllTypes>(&arena);
  TestUtil::SetAllFields(message1_on_arena);
  const auto* nested = &message1_on_arena->optional_nested_message();

  protobuf_unittest::TestAllTypes message2;

  // Moving from a message on the arena should lead to a copy.
  message2 = std::move(*message1_on_arena);
  EXPECT_NE(nested, &message2.optional_nested_message());
  TestUtil::ExpectAllFieldsSet(*message1_on_arena);
  TestUtil::ExpectAllFieldsSet(message2);
}

TEST(MovableMessageTest, MoveToArena) {
  Arena arena;

  protobuf_unittest::TestAllTypes message1;
  TestUtil::SetAllFields(&message1);
  const auto* nested = &message1.optional_nested_message();

  auto* message2_on_arena =
      Arena::CreateMessage<protobuf_unittest::TestAllTypes>(&arena);

  // Moving to a message on the arena should lead to a copy.
  *message2_on_arena = std::move(message1);
  EXPECT_NE(nested, &message2_on_arena->optional_nested_message());
  TestUtil::ExpectAllFieldsSet(message1);
  TestUtil::ExpectAllFieldsSet(*message2_on_arena);
}

TEST(MovableMessageTest, Noexcept) {
  EXPECT_TRUE(
      std::is_nothrow_move_constructible<protobuf_unittest::TestAllTypes>());
  EXPECT_TRUE(std::is_nothrow_move_assignable<protobuf_unittest::TestAllTypes>());
}

#endif  // LANG_CXX11

}  // namespace cpp_unittest

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
