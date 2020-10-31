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

// Author: tgs@google.com (Tom Szymanski)
//
// Test reflection methods for aggregate access to Repeated[Ptr]Fields.
// This test proto2 methods on a proto2 layout.

#include <google/protobuf/stubs/casts.h>
#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/reflection.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {

using unittest::ForeignMessage;
using unittest::TestAllExtensions;
using unittest::TestAllTypes;

namespace {

static int Func(int i, int j) { return i * j; }

static std::string StrFunc(int i, int j) {
  std::string str;
  SStringPrintf(&str, "%d", Func(i, 4));
  return str;
}

TEST(RepeatedFieldReflectionTest, RegularFields) {
  TestAllTypes message;
  const Reflection* refl = message.GetReflection();
  const Descriptor* desc = message.GetDescriptor();

  for (int i = 0; i < 10; ++i) {
    message.add_repeated_int32(Func(i, 1));
    message.add_repeated_double(Func(i, 2));
    message.add_repeated_string(StrFunc(i, 5));
    message.add_repeated_foreign_message()->set_c(Func(i, 6));
  }

  // Get FieldDescriptors for all the fields of interest.
  const FieldDescriptor* fd_repeated_int32 =
      desc->FindFieldByName("repeated_int32");
  const FieldDescriptor* fd_repeated_double =
      desc->FindFieldByName("repeated_double");
  const FieldDescriptor* fd_repeated_string =
      desc->FindFieldByName("repeated_string");
  const FieldDescriptor* fd_repeated_foreign_message =
      desc->FindFieldByName("repeated_foreign_message");

  // Get RepeatedField objects for all fields of interest.
  const RepeatedField<int32>& rf_int32 =
      refl->GetRepeatedField<int32>(message, fd_repeated_int32);
  const RepeatedField<double>& rf_double =
      refl->GetRepeatedField<double>(message, fd_repeated_double);

  // Get mutable RepeatedField objects for all fields of interest.
  RepeatedField<int32>* mrf_int32 =
      refl->MutableRepeatedField<int32>(&message, fd_repeated_int32);
  RepeatedField<double>* mrf_double =
      refl->MutableRepeatedField<double>(&message, fd_repeated_double);

  // Get RepeatedPtrField objects for all fields of interest.
  const RepeatedPtrField<std::string>& rpf_string =
      refl->GetRepeatedPtrField<std::string>(message, fd_repeated_string);
  const RepeatedPtrField<ForeignMessage>& rpf_foreign_message =
      refl->GetRepeatedPtrField<ForeignMessage>(message,
                                                fd_repeated_foreign_message);
  const RepeatedPtrField<Message>& rpf_message =
      refl->GetRepeatedPtrField<Message>(message, fd_repeated_foreign_message);

  // Get mutable RepeatedPtrField objects for all fields of interest.
  RepeatedPtrField<std::string>* mrpf_string =
      refl->MutableRepeatedPtrField<std::string>(&message, fd_repeated_string);
  RepeatedPtrField<ForeignMessage>* mrpf_foreign_message =
      refl->MutableRepeatedPtrField<ForeignMessage>(
          &message, fd_repeated_foreign_message);
  RepeatedPtrField<Message>* mrpf_message =
      refl->MutableRepeatedPtrField<Message>(&message,
                                             fd_repeated_foreign_message);

  // Make sure we can do gets and sets through the Repeated[Ptr]Field objects.
  for (int i = 0; i < 10; ++i) {
    // Check gets through const objects.
    EXPECT_EQ(rf_int32.Get(i), Func(i, 1));
    EXPECT_EQ(rf_double.Get(i), Func(i, 2));
    EXPECT_EQ(rpf_string.Get(i), StrFunc(i, 5));
    EXPECT_EQ(rpf_foreign_message.Get(i).c(), Func(i, 6));
    EXPECT_EQ(down_cast<const ForeignMessage*>(&rpf_message.Get(i))->c(),
              Func(i, 6));

    // Check gets through mutable objects.
    EXPECT_EQ(mrf_int32->Get(i), Func(i, 1));
    EXPECT_EQ(mrf_double->Get(i), Func(i, 2));
    EXPECT_EQ(mrpf_string->Get(i), StrFunc(i, 5));
    EXPECT_EQ(mrpf_foreign_message->Get(i).c(), Func(i, 6));
    EXPECT_EQ(down_cast<const ForeignMessage*>(&mrpf_message->Get(i))->c(),
              Func(i, 6));

    // Check sets through mutable objects.
    mrf_int32->Set(i, Func(i, -1));
    mrf_double->Set(i, Func(i, -2));
    mrpf_string->Mutable(i)->assign(StrFunc(i, -5));
    mrpf_foreign_message->Mutable(i)->set_c(Func(i, -6));
    EXPECT_EQ(message.repeated_int32(i), Func(i, -1));
    EXPECT_EQ(message.repeated_double(i), Func(i, -2));
    EXPECT_EQ(message.repeated_string(i), StrFunc(i, -5));
    EXPECT_EQ(message.repeated_foreign_message(i).c(), Func(i, -6));
    down_cast<ForeignMessage*>(mrpf_message->Mutable(i))->set_c(Func(i, 7));
    EXPECT_EQ(message.repeated_foreign_message(i).c(), Func(i, 7));
  }

#ifdef PROTOBUF_HAS_DEATH_TEST
  // Make sure types are checked correctly at runtime.
  const FieldDescriptor* fd_optional_int32 =
      desc->FindFieldByName("optional_int32");
  EXPECT_DEATH(refl->GetRepeatedField<int32>(message, fd_optional_int32),
               "requires a repeated field");
  EXPECT_DEATH(refl->GetRepeatedField<double>(message, fd_repeated_int32),
               "not the right type");
  EXPECT_DEATH(refl->GetRepeatedPtrField<TestAllTypes>(
                   message, fd_repeated_foreign_message),
               "wrong submessage type");
#endif  // PROTOBUF_HAS_DEATH_TEST
}


TEST(RepeatedFieldReflectionTest, ExtensionFields) {
  TestAllExtensions extended_message;
  const Reflection* refl = extended_message.GetReflection();
  const Descriptor* desc = extended_message.GetDescriptor();

  for (int i = 0; i < 10; ++i) {
    extended_message.AddExtension(unittest::repeated_int64_extension,
                                  Func(i, 1));
  }

  const FieldDescriptor* fd_repeated_int64_extension =
      desc->file()->FindExtensionByName("repeated_int64_extension");
  GOOGLE_CHECK(fd_repeated_int64_extension != NULL);

  const RepeatedField<int64>& rf_int64_extension =
      refl->GetRepeatedField<int64>(extended_message,
                                    fd_repeated_int64_extension);

  RepeatedField<int64>* mrf_int64_extension = refl->MutableRepeatedField<int64>(
      &extended_message, fd_repeated_int64_extension);

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(Func(i, 1), rf_int64_extension.Get(i));
    mrf_int64_extension->Set(i, Func(i, -1));
    EXPECT_EQ(Func(i, -1), extended_message.GetExtension(
                               unittest::repeated_int64_extension, i));
  }
}

template <typename Ref, typename MessageType, typename ValueType>
void TestRepeatedFieldRefIteratorForPrimitive(
    const Ref& handle, const MessageType& message,
    ValueType (MessageType::*GetFunc)(int) const) {
  int index = 0;
  for (typename Ref::const_iterator it = handle.begin(); it != handle.end();
       ++it) {
    EXPECT_EQ((message.*GetFunc)(index), *it);
    ++index;
  }
  EXPECT_EQ(handle.size(), index);
}

template <typename MessageType, typename ValueType>
void TestRepeatedFieldRefIteratorForString(
    const RepeatedFieldRef<std::string>& handle, const MessageType& message,
    ValueType (MessageType::*GetFunc)(int) const) {
  int index = 0;
  for (typename RepeatedFieldRef<std::string>::const_iterator it =
           handle.begin();
       it != handle.end(); ++it) {
    // Test both operator* and operator->
    EXPECT_EQ((message.*GetFunc)(index), *it);
    EXPECT_EQ((message.*GetFunc)(index).size(), it->size());
    ++index;
  }
  EXPECT_EQ(handle.size(), index);
}

TEST(RepeatedFieldReflectionTest, RepeatedFieldRefForRegularFields) {
  TestAllTypes message;
  const Reflection* refl = message.GetReflection();
  const Descriptor* desc = message.GetDescriptor();

  for (int i = 0; i < 10; ++i) {
    message.add_repeated_int32(Func(i, 1));
    message.add_repeated_double(Func(i, 2));
    message.add_repeated_string(StrFunc(i, 5));
    message.add_repeated_foreign_message()->set_c(Func(i, 6));
  }

  // Get FieldDescriptors for all the fields of interest.
  const FieldDescriptor* fd_repeated_int32 =
      desc->FindFieldByName("repeated_int32");
  const FieldDescriptor* fd_repeated_double =
      desc->FindFieldByName("repeated_double");
  const FieldDescriptor* fd_repeated_string =
      desc->FindFieldByName("repeated_string");
  const FieldDescriptor* fd_repeated_foreign_message =
      desc->FindFieldByName("repeated_foreign_message");

  // Get RepeatedFieldRef objects for all fields of interest.
  const RepeatedFieldRef<int32> rf_int32 =
      refl->GetRepeatedFieldRef<int32>(message, fd_repeated_int32);
  const RepeatedFieldRef<double> rf_double =
      refl->GetRepeatedFieldRef<double>(message, fd_repeated_double);
  const RepeatedFieldRef<std::string> rf_string =
      refl->GetRepeatedFieldRef<std::string>(message, fd_repeated_string);
  const RepeatedFieldRef<ForeignMessage> rf_foreign_message =
      refl->GetRepeatedFieldRef<ForeignMessage>(message,
                                                fd_repeated_foreign_message);
  const RepeatedFieldRef<Message> rf_message =
      refl->GetRepeatedFieldRef<Message>(message, fd_repeated_foreign_message);

  // Get MutableRepeatedFieldRef objects for all fields of interest.
  const MutableRepeatedFieldRef<int32> mrf_int32 =
      refl->GetMutableRepeatedFieldRef<int32>(&message, fd_repeated_int32);
  const MutableRepeatedFieldRef<double> mrf_double =
      refl->GetMutableRepeatedFieldRef<double>(&message, fd_repeated_double);
  const MutableRepeatedFieldRef<std::string> mrf_string =
      refl->GetMutableRepeatedFieldRef<std::string>(&message,
                                                    fd_repeated_string);
  const MutableRepeatedFieldRef<ForeignMessage> mrf_foreign_message =
      refl->GetMutableRepeatedFieldRef<ForeignMessage>(
          &message, fd_repeated_foreign_message);
  const MutableRepeatedFieldRef<Message> mrf_message =
      refl->GetMutableRepeatedFieldRef<Message>(&message,
                                                fd_repeated_foreign_message);

  EXPECT_EQ(message.repeated_int32_size(), rf_int32.size());
  EXPECT_EQ(message.repeated_int32_size(), mrf_int32.size());
  EXPECT_EQ(message.repeated_double_size(), rf_double.size());
  EXPECT_EQ(message.repeated_double_size(), mrf_double.size());
  EXPECT_EQ(message.repeated_string_size(), rf_string.size());
  EXPECT_EQ(message.repeated_string_size(), mrf_string.size());
  EXPECT_EQ(message.repeated_foreign_message_size(), rf_foreign_message.size());
  EXPECT_EQ(message.repeated_foreign_message_size(),
            mrf_foreign_message.size());
  EXPECT_EQ(message.repeated_foreign_message_size(), rf_message.size());
  EXPECT_EQ(message.repeated_foreign_message_size(), mrf_message.size());

  EXPECT_FALSE(rf_int32.empty());
  EXPECT_FALSE(mrf_int32.empty());
  EXPECT_FALSE(rf_double.empty());
  EXPECT_FALSE(mrf_double.empty());
  EXPECT_FALSE(rf_string.empty());
  EXPECT_FALSE(mrf_string.empty());
  EXPECT_FALSE(rf_foreign_message.empty());
  EXPECT_FALSE(mrf_foreign_message.empty());
  EXPECT_FALSE(rf_message.empty());
  EXPECT_FALSE(mrf_message.empty());

  // Make sure we can do gets and sets through the RepeatedFieldRef objects.
  for (int i = 0; i < 10; ++i) {
    // Check gets through const objects.
    EXPECT_EQ(rf_int32.Get(i), Func(i, 1));
    EXPECT_EQ(rf_double.Get(i), Func(i, 2));
    EXPECT_EQ(rf_string.Get(i), StrFunc(i, 5));
    ForeignMessage scratch_space;
    EXPECT_EQ(rf_foreign_message.Get(i, &scratch_space).c(), Func(i, 6));
    EXPECT_EQ(
        down_cast<const ForeignMessage&>(rf_message.Get(i, &scratch_space)).c(),
        Func(i, 6));

    // Check gets through mutable objects.
    EXPECT_EQ(mrf_int32.Get(i), Func(i, 1));
    EXPECT_EQ(mrf_double.Get(i), Func(i, 2));
    EXPECT_EQ(mrf_string.Get(i), StrFunc(i, 5));
    EXPECT_EQ(mrf_foreign_message.Get(i, &scratch_space).c(), Func(i, 6));
    EXPECT_EQ(
        down_cast<const ForeignMessage&>(mrf_message.Get(i, &scratch_space))
            .c(),
        Func(i, 6));

    // Check sets through mutable objects.
    mrf_int32.Set(i, Func(i, -1));
    mrf_double.Set(i, Func(i, -2));
    mrf_string.Set(i, StrFunc(i, -5));
    ForeignMessage foreign_message;
    foreign_message.set_c(Func(i, -6));
    mrf_foreign_message.Set(i, foreign_message);
    EXPECT_EQ(message.repeated_int32(i), Func(i, -1));
    EXPECT_EQ(message.repeated_double(i), Func(i, -2));
    EXPECT_EQ(message.repeated_string(i), StrFunc(i, -5));
    EXPECT_EQ(message.repeated_foreign_message(i).c(), Func(i, -6));
    foreign_message.set_c(Func(i, 7));
    mrf_message.Set(i, foreign_message);
    EXPECT_EQ(message.repeated_foreign_message(i).c(), Func(i, 7));
  }

  // Test iterators.
  TestRepeatedFieldRefIteratorForPrimitive(rf_int32, message,
                                           &TestAllTypes::repeated_int32);
  TestRepeatedFieldRefIteratorForPrimitive(rf_double, message,
                                           &TestAllTypes::repeated_double);
  TestRepeatedFieldRefIteratorForString(rf_string, message,
                                        &TestAllTypes::repeated_string);

  // Test iterators for message fields.
  typedef RepeatedFieldRef<ForeignMessage>::iterator MessageIterator;
  int index = 0;
  for (MessageIterator it = rf_foreign_message.begin();
       it != rf_foreign_message.end(); ++it) {
    EXPECT_EQ(message.repeated_foreign_message(index).c(), it->c());
    ++index;
  }
  EXPECT_EQ(10, index);

  // Test iterator operators that are not ususally used in regular for-loops.
  // Including: post increment, assign, ==.
  MessageIterator old_it = rf_foreign_message.begin();
  MessageIterator new_it = old_it++;
  EXPECT_FALSE(old_it == new_it);
  // Check that old_it++ increments old_it once.
  for (index = 1; old_it != rf_foreign_message.end(); ++old_it, ++index) {
    EXPECT_EQ(message.repeated_foreign_message(index).c(), old_it->c());
  }
  EXPECT_EQ(10, index);
  // Test assign operator.
  old_it = new_it;
  for (index = 0; old_it != rf_foreign_message.end(); ++old_it, ++index) {
    EXPECT_EQ(message.repeated_foreign_message(index).c(), old_it->c());
  }
  EXPECT_EQ(10, index);
  // Check that the returned value of old_it++ is the one before increment.
  for (index = 0; new_it != rf_foreign_message.end(); ++new_it, ++index) {
    EXPECT_EQ(message.repeated_foreign_message(index).c(), new_it->c());
  }
  EXPECT_EQ(10, index);

  // Test MutableRepeatedFieldRef::Add()
  mrf_int32.Add(1234);
  mrf_double.Add(1234.0);
  mrf_string.Add("1234");
  ForeignMessage foreign_message;
  foreign_message.set_c(1234);
  mrf_foreign_message.Add(foreign_message);
  EXPECT_EQ(1234, message.repeated_int32(10));
  EXPECT_EQ(1234.0, message.repeated_double(10));
  EXPECT_EQ("1234", message.repeated_string(10));
  EXPECT_EQ(1234, message.repeated_foreign_message(10).c());

  // Test MutableRepeatedFieldRef::RemoveLast()
  mrf_int32.RemoveLast();
  mrf_double.RemoveLast();
  mrf_string.RemoveLast();
  mrf_foreign_message.RemoveLast();
  EXPECT_EQ(10, message.repeated_int32_size());
  EXPECT_EQ(10, message.repeated_double_size());
  EXPECT_EQ(10, message.repeated_string_size());
  EXPECT_EQ(10, message.repeated_foreign_message_size());

  // Test MutableRepeatedFieldRef::SwapElements()
  mrf_int32.SwapElements(0, 9);
  mrf_double.SwapElements(0, 9);
  mrf_string.SwapElements(0, 9);
  mrf_foreign_message.SwapElements(0, 9);
  EXPECT_EQ(Func(9, -1), message.repeated_int32(0));
  EXPECT_EQ(Func(0, -1), message.repeated_int32(9));
  EXPECT_EQ(Func(9, -2), message.repeated_double(0));
  EXPECT_EQ(Func(0, -2), message.repeated_double(9));
  EXPECT_EQ(StrFunc(9, -5), message.repeated_string(0));
  EXPECT_EQ(StrFunc(0, -5), message.repeated_string(9));
  EXPECT_EQ(Func(9, 7), message.repeated_foreign_message(0).c());
  EXPECT_EQ(Func(0, 7), message.repeated_foreign_message(9).c());

  // Test MutableRepeatedFieldRef::Clear()
  mrf_int32.Clear();
  mrf_double.Clear();
  mrf_string.Clear();
  mrf_foreign_message.Clear();
  EXPECT_EQ(0, message.repeated_int32_size());
  EXPECT_EQ(0, message.repeated_double_size());
  EXPECT_EQ(0, message.repeated_string_size());
  EXPECT_EQ(0, message.repeated_foreign_message_size());

  // Test (Mutable)RepeatedFieldRef::empty()
  EXPECT_TRUE(rf_int32.empty());
  EXPECT_TRUE(mrf_int32.empty());
  EXPECT_TRUE(rf_double.empty());
  EXPECT_TRUE(mrf_double.empty());
  EXPECT_TRUE(rf_string.empty());
  EXPECT_TRUE(mrf_string.empty());
  EXPECT_TRUE(rf_foreign_message.empty());
  EXPECT_TRUE(mrf_foreign_message.empty());
  EXPECT_TRUE(rf_message.empty());
  EXPECT_TRUE(mrf_message.empty());

#ifdef PROTOBUF_HAS_DEATH_TEST

  // Make sure types are checked correctly at runtime.
  const FieldDescriptor* fd_optional_int32 =
      desc->FindFieldByName("optional_int32");
  EXPECT_DEATH(refl->GetRepeatedFieldRef<int32>(message, fd_optional_int32),
               "");
  EXPECT_DEATH(refl->GetRepeatedFieldRef<double>(message, fd_repeated_int32),
               "");
  EXPECT_DEATH(refl->GetRepeatedFieldRef<TestAllTypes>(
                   message, fd_repeated_foreign_message),
               "");

#endif  // PROTOBUF_HAS_DEATH_TEST
}

TEST(RepeatedFieldReflectionTest, RepeatedFieldRefForEnums) {
  TestAllTypes message;
  const Reflection* refl = message.GetReflection();
  const Descriptor* desc = message.GetDescriptor();

  for (int i = 0; i < 10; ++i) {
    message.add_repeated_nested_enum(TestAllTypes::BAR);
  }

  const FieldDescriptor* fd_repeated_nested_enum =
      desc->FindFieldByName("repeated_nested_enum");
  const RepeatedFieldRef<TestAllTypes::NestedEnum> enum_ref =
      refl->GetRepeatedFieldRef<TestAllTypes::NestedEnum>(
          message, fd_repeated_nested_enum);
  const MutableRepeatedFieldRef<TestAllTypes::NestedEnum> mutable_enum_ref =
      refl->GetMutableRepeatedFieldRef<TestAllTypes::NestedEnum>(
          &message, fd_repeated_nested_enum);
  const RepeatedFieldRef<int32> int32_ref =
      refl->GetRepeatedFieldRef<int32>(message, fd_repeated_nested_enum);
  const MutableRepeatedFieldRef<int32> mutable_int32_ref =
      refl->GetMutableRepeatedFieldRef<int32>(&message,
                                              fd_repeated_nested_enum);

  EXPECT_EQ(message.repeated_nested_enum_size(), enum_ref.size());
  EXPECT_EQ(message.repeated_nested_enum_size(), mutable_enum_ref.size());
  EXPECT_EQ(message.repeated_nested_enum_size(), int32_ref.size());
  EXPECT_EQ(message.repeated_nested_enum_size(), mutable_int32_ref.size());

  EXPECT_FALSE(enum_ref.empty());
  EXPECT_FALSE(mutable_enum_ref.empty());
  EXPECT_FALSE(int32_ref.empty());
  EXPECT_FALSE(mutable_int32_ref.empty());

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(TestAllTypes::BAR, enum_ref.Get(i));
    EXPECT_EQ(TestAllTypes::BAR, mutable_enum_ref.Get(i));
    mutable_enum_ref.Set(i, TestAllTypes::BAZ);
    EXPECT_EQ(TestAllTypes::BAZ, enum_ref.Get(i));
    EXPECT_EQ(TestAllTypes::BAZ, message.repeated_nested_enum(i));

    message.set_repeated_nested_enum(i, TestAllTypes::BAR);
    EXPECT_EQ(TestAllTypes::BAR, int32_ref.Get(i));
    EXPECT_EQ(TestAllTypes::BAR, mutable_int32_ref.Get(i));
    mutable_int32_ref.Set(i, TestAllTypes::BAZ);
    EXPECT_EQ(TestAllTypes::BAZ, int32_ref.Get(i));
    EXPECT_EQ(TestAllTypes::BAZ, message.repeated_nested_enum(i));
  }

  TestRepeatedFieldRefIteratorForPrimitive(enum_ref, message,
                                           &TestAllTypes::repeated_nested_enum);
  TestRepeatedFieldRefIteratorForPrimitive(int32_ref, message,
                                           &TestAllTypes::repeated_nested_enum);

  // Test Add()
  mutable_enum_ref.Add(TestAllTypes::FOO);
  EXPECT_EQ(TestAllTypes::FOO, message.repeated_nested_enum(10));
  mutable_int32_ref.Add(TestAllTypes::BAR);
  EXPECT_EQ(TestAllTypes::BAR, message.repeated_nested_enum(11));

  // Test RemoveLast()
  mutable_enum_ref.RemoveLast();
  EXPECT_EQ(11, message.repeated_nested_enum_size());
  mutable_int32_ref.RemoveLast();
  EXPECT_EQ(10, message.repeated_nested_enum_size());

  // Test SwapElements()
  mutable_enum_ref.Set(0, TestAllTypes::BAR);
  mutable_enum_ref.Set(9, TestAllTypes::BAZ);
  mutable_enum_ref.SwapElements(0, 9);
  EXPECT_EQ(TestAllTypes::BAZ, enum_ref.Get(0));
  EXPECT_EQ(TestAllTypes::BAR, enum_ref.Get(9));
  mutable_int32_ref.SwapElements(0, 9);
  EXPECT_EQ(TestAllTypes::BAR, enum_ref.Get(0));
  EXPECT_EQ(TestAllTypes::BAZ, enum_ref.Get(9));

  // Test Clear()
  mutable_enum_ref.Clear();
  EXPECT_EQ(0, message.repeated_nested_enum_size());
  mutable_enum_ref.Add(TestAllTypes::FOO);
  EXPECT_EQ(1, message.repeated_nested_enum_size());
  mutable_int32_ref.Clear();
  EXPECT_EQ(0, message.repeated_nested_enum_size());

  // Test empty()
  EXPECT_TRUE(enum_ref.empty());
  EXPECT_TRUE(mutable_enum_ref.empty());
  EXPECT_TRUE(int32_ref.empty());
  EXPECT_TRUE(mutable_int32_ref.empty());
}

TEST(RepeatedFieldReflectionTest, RepeatedFieldRefForExtensionFields) {
  TestAllExtensions extended_message;
  const Reflection* refl = extended_message.GetReflection();
  const Descriptor* desc = extended_message.GetDescriptor();

  for (int i = 0; i < 10; ++i) {
    extended_message.AddExtension(unittest::repeated_int64_extension,
                                  Func(i, 1));
  }

  const FieldDescriptor* fd_repeated_int64_extension =
      desc->file()->FindExtensionByName("repeated_int64_extension");
  GOOGLE_CHECK(fd_repeated_int64_extension != NULL);

  const RepeatedFieldRef<int64> rf_int64_extension =
      refl->GetRepeatedFieldRef<int64>(extended_message,
                                       fd_repeated_int64_extension);

  const MutableRepeatedFieldRef<int64> mrf_int64_extension =
      refl->GetMutableRepeatedFieldRef<int64>(&extended_message,
                                              fd_repeated_int64_extension);

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(Func(i, 1), rf_int64_extension.Get(i));
    mrf_int64_extension.Set(i, Func(i, -1));
    EXPECT_EQ(Func(i, -1), extended_message.GetExtension(
                               unittest::repeated_int64_extension, i));
  }
}


TEST(RepeatedFieldReflectionTest, RepeatedFieldRefMergeFromAndSwap) {
  // Set-up message content.
  TestAllTypes m0, m1, m2;
  for (int i = 0; i < 10; ++i) {
    m0.add_repeated_int32(Func(i, 1));
    m0.add_repeated_double(Func(i, 2));
    m0.add_repeated_string(StrFunc(i, 5));
    m0.add_repeated_foreign_message()->set_c(Func(i, 6));
    m0.add_repeated_nested_enum(TestAllTypes::FOO);
    m1.add_repeated_int32(Func(i, 11));
    m1.add_repeated_double(Func(i, 12));
    m1.add_repeated_string(StrFunc(i, 15));
    m1.add_repeated_foreign_message()->set_c(Func(i, 16));
    m1.add_repeated_nested_enum(TestAllTypes::BAR);
    m2.add_repeated_int32(Func(i, 21));
    m2.add_repeated_double(Func(i, 22));
    m2.add_repeated_string(StrFunc(i, 25));
    m2.add_repeated_foreign_message()->set_c(Func(i, 26));
    m2.add_repeated_nested_enum(TestAllTypes::BAZ);
  }

  const Reflection* refl = m0.GetReflection();
  const Descriptor* desc = m0.GetDescriptor();

  // Get FieldDescriptors for all the fields of interest.
  const FieldDescriptor* fd_repeated_int32 =
      desc->FindFieldByName("repeated_int32");
  const FieldDescriptor* fd_repeated_double =
      desc->FindFieldByName("repeated_double");
  const FieldDescriptor* fd_repeated_string =
      desc->FindFieldByName("repeated_string");
  const FieldDescriptor* fd_repeated_foreign_message =
      desc->FindFieldByName("repeated_foreign_message");
  const FieldDescriptor* fd_repeated_nested_enum =
      desc->FindFieldByName("repeated_nested_enum");

  // Get MutableRepeatedFieldRef objects for all fields of interest.
  const MutableRepeatedFieldRef<int32> mrf_int32 =
      refl->GetMutableRepeatedFieldRef<int32>(&m0, fd_repeated_int32);
  const MutableRepeatedFieldRef<double> mrf_double =
      refl->GetMutableRepeatedFieldRef<double>(&m0, fd_repeated_double);
  const MutableRepeatedFieldRef<std::string> mrf_string =
      refl->GetMutableRepeatedFieldRef<std::string>(&m0, fd_repeated_string);
  const MutableRepeatedFieldRef<ForeignMessage> mrf_foreign_message =
      refl->GetMutableRepeatedFieldRef<ForeignMessage>(
          &m0, fd_repeated_foreign_message);
  const MutableRepeatedFieldRef<TestAllTypes::NestedEnum> mrf_nested_enum =
      refl->GetMutableRepeatedFieldRef<TestAllTypes::NestedEnum>(
          &m0, fd_repeated_nested_enum);

  // Test MutableRepeatedRef::CopyFrom
  mrf_int32.CopyFrom(refl->GetRepeatedFieldRef<int32>(m1, fd_repeated_int32));
  mrf_double.CopyFrom(
      refl->GetRepeatedFieldRef<double>(m1, fd_repeated_double));
  mrf_string.CopyFrom(
      refl->GetRepeatedFieldRef<std::string>(m1, fd_repeated_string));
  mrf_foreign_message.CopyFrom(refl->GetRepeatedFieldRef<ForeignMessage>(
      m1, fd_repeated_foreign_message));
  mrf_nested_enum.CopyFrom(refl->GetRepeatedFieldRef<TestAllTypes::NestedEnum>(
      m1, fd_repeated_nested_enum));
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(Func(i, 11), m0.repeated_int32(i));
    EXPECT_EQ(Func(i, 12), m0.repeated_double(i));
    EXPECT_EQ(StrFunc(i, 15), m0.repeated_string(i));
    EXPECT_EQ(Func(i, 16), m0.repeated_foreign_message(i).c());
    EXPECT_EQ(TestAllTypes::BAR, m0.repeated_nested_enum(i));
  }

  // Test MutableRepeatedRef::MergeFrom
  mrf_int32.MergeFrom(refl->GetRepeatedFieldRef<int32>(m2, fd_repeated_int32));
  mrf_double.MergeFrom(
      refl->GetRepeatedFieldRef<double>(m2, fd_repeated_double));
  mrf_string.MergeFrom(
      refl->GetRepeatedFieldRef<std::string>(m2, fd_repeated_string));
  mrf_foreign_message.MergeFrom(refl->GetRepeatedFieldRef<ForeignMessage>(
      m2, fd_repeated_foreign_message));
  mrf_nested_enum.MergeFrom(refl->GetRepeatedFieldRef<TestAllTypes::NestedEnum>(
      m2, fd_repeated_nested_enum));
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(Func(i, 21), m0.repeated_int32(i + 10));
    EXPECT_EQ(Func(i, 22), m0.repeated_double(i + 10));
    EXPECT_EQ(StrFunc(i, 25), m0.repeated_string(i + 10));
    EXPECT_EQ(Func(i, 26), m0.repeated_foreign_message(i + 10).c());
    EXPECT_EQ(TestAllTypes::BAZ, m0.repeated_nested_enum(i + 10));
  }

  // Test MutableRepeatedRef::Swap
  // Swap between m0 and m2.
  mrf_int32.Swap(
      refl->GetMutableRepeatedFieldRef<int32>(&m2, fd_repeated_int32));
  mrf_double.Swap(
      refl->GetMutableRepeatedFieldRef<double>(&m2, fd_repeated_double));
  mrf_string.Swap(
      refl->GetMutableRepeatedFieldRef<std::string>(&m2, fd_repeated_string));
  mrf_foreign_message.Swap(refl->GetMutableRepeatedFieldRef<ForeignMessage>(
      &m2, fd_repeated_foreign_message));
  mrf_nested_enum.Swap(
      refl->GetMutableRepeatedFieldRef<TestAllTypes::NestedEnum>(
          &m2, fd_repeated_nested_enum));
  for (int i = 0; i < 10; ++i) {
    // Check the content of m0.
    EXPECT_EQ(Func(i, 21), m0.repeated_int32(i));
    EXPECT_EQ(Func(i, 22), m0.repeated_double(i));
    EXPECT_EQ(StrFunc(i, 25), m0.repeated_string(i));
    EXPECT_EQ(Func(i, 26), m0.repeated_foreign_message(i).c());
    EXPECT_EQ(TestAllTypes::BAZ, m0.repeated_nested_enum(i));

    // Check the content of m2.
    EXPECT_EQ(Func(i, 11), m2.repeated_int32(i));
    EXPECT_EQ(Func(i, 12), m2.repeated_double(i));
    EXPECT_EQ(StrFunc(i, 15), m2.repeated_string(i));
    EXPECT_EQ(Func(i, 16), m2.repeated_foreign_message(i).c());
    EXPECT_EQ(TestAllTypes::BAR, m2.repeated_nested_enum(i));
    EXPECT_EQ(Func(i, 21), m2.repeated_int32(i + 10));
    EXPECT_EQ(Func(i, 22), m2.repeated_double(i + 10));
    EXPECT_EQ(StrFunc(i, 25), m2.repeated_string(i + 10));
    EXPECT_EQ(Func(i, 26), m2.repeated_foreign_message(i + 10).c());
    EXPECT_EQ(TestAllTypes::BAZ, m2.repeated_nested_enum(i + 10));
  }
}

// Test that GetRepeatedFieldRef/MutableRepeatedFieldRef works with
// DynamicMessage.
TEST(RepeatedFieldReflectionTest, RepeatedFieldRefDynamicMessage) {
  // DynamicMessage shares the same memory layout as generated message
  // and use the same GeneratedMessageReflection code for reflection.
  // All code paths should already be covered by the other tests for
  // generated messages. Here we just test one field.

  const Descriptor* desc = TestAllTypes::descriptor();
  const FieldDescriptor* fd_repeated_int32 =
      desc->FindFieldByName("repeated_int32");

  DynamicMessageFactory factory;
  std::unique_ptr<Message> dynamic_message(factory.GetPrototype(desc)->New());
  const Reflection* refl = dynamic_message->GetReflection();

  MutableRepeatedFieldRef<int32> rf_int32 =
      refl->GetMutableRepeatedFieldRef<int32>(dynamic_message.get(),
                                              fd_repeated_int32);
  rf_int32.Add(1234);
  EXPECT_EQ(1, refl->FieldSize(*dynamic_message, fd_repeated_int32));
  EXPECT_EQ(1234,
            refl->GetRepeatedInt32(*dynamic_message, fd_repeated_int32, 0));
}

}  // namespace
}  // namespace protobuf
}  // namespace google
