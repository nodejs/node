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

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/stubs/casts.h>

#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/test_util2.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_mset.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/wire_format.h>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/stl_util.h>


namespace google {
namespace protobuf {
namespace internal {
namespace {

using TestUtil::EqualsToSerialized;

// This test closely mirrors net/proto2/compiler/cpp/internal/unittest.cc
// except that it uses extensions rather than regular fields.

TEST(ExtensionSetTest, Defaults) {
  // Check that all default values are set correctly in the initial message.
  unittest::TestAllExtensions message;

  TestUtil::ExpectExtensionsClear(message);

  // Messages should return pointers to default instances until first use.
  // (This is not checked by ExpectClear() since it is not actually true after
  // the fields have been set and then cleared.)
  EXPECT_EQ(&unittest::OptionalGroup_extension::default_instance(),
            &message.GetExtension(unittest::optionalgroup_extension));
  EXPECT_EQ(&unittest::TestAllTypes::NestedMessage::default_instance(),
            &message.GetExtension(unittest::optional_nested_message_extension));
  EXPECT_EQ(
      &unittest::ForeignMessage::default_instance(),
      &message.GetExtension(unittest::optional_foreign_message_extension));
  EXPECT_EQ(&unittest_import::ImportMessage::default_instance(),
            &message.GetExtension(unittest::optional_import_message_extension));
}

TEST(ExtensionSetTest, Accessors) {
  // Set every field to a unique value then go back and check all those
  // values.
  unittest::TestAllExtensions message;

  TestUtil::SetAllExtensions(&message);
  TestUtil::ExpectAllExtensionsSet(message);

  TestUtil::ModifyRepeatedExtensions(&message);
  TestUtil::ExpectRepeatedExtensionsModified(message);
}

TEST(ExtensionSetTest, Clear) {
  // Set every field to a unique value, clear the message, then check that
  // it is cleared.
  unittest::TestAllExtensions message;

  TestUtil::SetAllExtensions(&message);
  message.Clear();
  TestUtil::ExpectExtensionsClear(message);

  // Unlike with the defaults test, we do NOT expect that requesting embedded
  // messages will return a pointer to the default instance.  Instead, they
  // should return the objects that were created when mutable_blah() was
  // called.
  EXPECT_NE(&unittest::OptionalGroup_extension::default_instance(),
            &message.GetExtension(unittest::optionalgroup_extension));
  EXPECT_NE(&unittest::TestAllTypes::NestedMessage::default_instance(),
            &message.GetExtension(unittest::optional_nested_message_extension));
  EXPECT_NE(
      &unittest::ForeignMessage::default_instance(),
      &message.GetExtension(unittest::optional_foreign_message_extension));
  EXPECT_NE(&unittest_import::ImportMessage::default_instance(),
            &message.GetExtension(unittest::optional_import_message_extension));

  // Make sure setting stuff again after clearing works.  (This takes slightly
  // different code paths since the objects are reused.)
  TestUtil::SetAllExtensions(&message);
  TestUtil::ExpectAllExtensionsSet(message);
}

TEST(ExtensionSetTest, ClearOneField) {
  // Set every field to a unique value, then clear one value and insure that
  // only that one value is cleared.
  unittest::TestAllExtensions message;

  TestUtil::SetAllExtensions(&message);
  int64 original_value =
      message.GetExtension(unittest::optional_int64_extension);

  // Clear the field and make sure it shows up as cleared.
  message.ClearExtension(unittest::optional_int64_extension);
  EXPECT_FALSE(message.HasExtension(unittest::optional_int64_extension));
  EXPECT_EQ(0, message.GetExtension(unittest::optional_int64_extension));

  // Other adjacent fields should not be cleared.
  EXPECT_TRUE(message.HasExtension(unittest::optional_int32_extension));
  EXPECT_TRUE(message.HasExtension(unittest::optional_uint32_extension));

  // Make sure if we set it again, then all fields are set.
  message.SetExtension(unittest::optional_int64_extension, original_value);
  TestUtil::ExpectAllExtensionsSet(message);
}

TEST(ExtensionSetTest, SetAllocatedExtension) {
  unittest::TestAllExtensions message;
  EXPECT_FALSE(
      message.HasExtension(unittest::optional_foreign_message_extension));
  // Add a extension using SetAllocatedExtension
  unittest::ForeignMessage* foreign_message = new unittest::ForeignMessage();
  message.SetAllocatedExtension(unittest::optional_foreign_message_extension,
                                foreign_message);
  EXPECT_TRUE(
      message.HasExtension(unittest::optional_foreign_message_extension));
  EXPECT_EQ(foreign_message, message.MutableExtension(
                                 unittest::optional_foreign_message_extension));
  EXPECT_EQ(foreign_message, &message.GetExtension(
                                 unittest::optional_foreign_message_extension));

  // SetAllocatedExtension should delete the previously existing extension.
  // (We reply on unittest to check memory leaks for this case)
  message.SetAllocatedExtension(unittest::optional_foreign_message_extension,
                                new unittest::ForeignMessage());

  // SetAllocatedExtension with a NULL parameter is equivalent to ClearExtenion.
  message.SetAllocatedExtension(unittest::optional_foreign_message_extension,
                                NULL);
  EXPECT_FALSE(
      message.HasExtension(unittest::optional_foreign_message_extension));
}

TEST(ExtensionSetTest, ReleaseExtension) {
  proto2_wireformat_unittest::TestMessageSet message;
  EXPECT_FALSE(message.HasExtension(
      unittest::TestMessageSetExtension1::message_set_extension));
  // Add a extension using SetAllocatedExtension
  unittest::TestMessageSetExtension1* extension =
      new unittest::TestMessageSetExtension1();
  message.SetAllocatedExtension(
      unittest::TestMessageSetExtension1::message_set_extension, extension);
  EXPECT_TRUE(message.HasExtension(
      unittest::TestMessageSetExtension1::message_set_extension));
  // Release the extension using ReleaseExtension
  unittest::TestMessageSetExtension1* released_extension =
      message.ReleaseExtension(
          unittest::TestMessageSetExtension1::message_set_extension);
  EXPECT_EQ(extension, released_extension);
  EXPECT_FALSE(message.HasExtension(
      unittest::TestMessageSetExtension1::message_set_extension));
  // ReleaseExtension will return the underlying object even after
  // ClearExtension is called.
  message.SetAllocatedExtension(
      unittest::TestMessageSetExtension1::message_set_extension, extension);
  message.ClearExtension(
      unittest::TestMessageSetExtension1::message_set_extension);
  released_extension = message.ReleaseExtension(
      unittest::TestMessageSetExtension1::message_set_extension);
  EXPECT_TRUE(released_extension != NULL);
  delete released_extension;
}

TEST(ExtensionSetTest, ArenaUnsafeArenaSetAllocatedAndRelease) {
  Arena arena;
  unittest::TestAllExtensions* message =
      Arena::CreateMessage<unittest::TestAllExtensions>(&arena);
  unittest::ForeignMessage extension;
  message->UnsafeArenaSetAllocatedExtension(
      unittest::optional_foreign_message_extension, &extension);
  // No copy when set.
  unittest::ForeignMessage* mutable_extension =
      message->MutableExtension(unittest::optional_foreign_message_extension);
  EXPECT_EQ(&extension, mutable_extension);
  // No copy when unsafe released.
  unittest::ForeignMessage* released_extension =
      message->UnsafeArenaReleaseExtension(
          unittest::optional_foreign_message_extension);
  EXPECT_EQ(&extension, released_extension);
  EXPECT_FALSE(
      message->HasExtension(unittest::optional_foreign_message_extension));
  // Set the ownership back and let the destructors run.  It should not take
  // ownership, so this should not crash.
  message->UnsafeArenaSetAllocatedExtension(
      unittest::optional_foreign_message_extension, &extension);
}

TEST(ExtensionSetTest, UnsafeArenaSetAllocatedAndRelease) {
  unittest::TestAllExtensions message;
  unittest::ForeignMessage* extension = new unittest::ForeignMessage();
  message.UnsafeArenaSetAllocatedExtension(
      unittest::optional_foreign_message_extension, extension);
  // No copy when set.
  unittest::ForeignMessage* mutable_extension =
      message.MutableExtension(unittest::optional_foreign_message_extension);
  EXPECT_EQ(extension, mutable_extension);
  // No copy when unsafe released.
  unittest::ForeignMessage* released_extension =
      message.UnsafeArenaReleaseExtension(
          unittest::optional_foreign_message_extension);
  EXPECT_EQ(extension, released_extension);
  EXPECT_FALSE(
      message.HasExtension(unittest::optional_foreign_message_extension));
  // Set the ownership back and let the destructors run.  It should take
  // ownership, so this should not leak.
  message.UnsafeArenaSetAllocatedExtension(
      unittest::optional_foreign_message_extension, extension);
}

TEST(ExtensionSetTest, ArenaUnsafeArenaReleaseOfHeapAlloc) {
  Arena arena;
  unittest::TestAllExtensions* message =
      Arena::CreateMessage<unittest::TestAllExtensions>(&arena);
  unittest::ForeignMessage* extension = new unittest::ForeignMessage;
  message->SetAllocatedExtension(unittest::optional_foreign_message_extension,
                                 extension);
  // The arena should maintain ownership of the heap allocated proto because we
  // used UnsafeArenaReleaseExtension.  The leak checker will ensure this.
  unittest::ForeignMessage* released_extension =
      message->UnsafeArenaReleaseExtension(
          unittest::optional_foreign_message_extension);
  EXPECT_EQ(extension, released_extension);
  EXPECT_FALSE(
      message->HasExtension(unittest::optional_foreign_message_extension));
}


TEST(ExtensionSetTest, CopyFrom) {
  unittest::TestAllExtensions message1, message2;

  TestUtil::SetAllExtensions(&message1);
  message2.CopyFrom(message1);
  TestUtil::ExpectAllExtensionsSet(message2);
  message2.CopyFrom(message1);  // exercise copy when fields already exist
  TestUtil::ExpectAllExtensionsSet(message2);
}

TEST(ExtensionSetTest, CopyFromPacked) {
  unittest::TestPackedExtensions message1, message2;

  TestUtil::SetPackedExtensions(&message1);
  message2.CopyFrom(message1);
  TestUtil::ExpectPackedExtensionsSet(message2);
  message2.CopyFrom(message1);  // exercise copy when fields already exist
  TestUtil::ExpectPackedExtensionsSet(message2);
}

TEST(ExtensionSetTest, CopyFromUpcasted) {
  unittest::TestAllExtensions message1, message2;
  const Message& upcasted_message = message1;

  TestUtil::SetAllExtensions(&message1);
  message2.CopyFrom(upcasted_message);
  TestUtil::ExpectAllExtensionsSet(message2);
  // exercise copy when fields already exist
  message2.CopyFrom(upcasted_message);
  TestUtil::ExpectAllExtensionsSet(message2);
}

TEST(ExtensionSetTest, SwapWithEmpty) {
  unittest::TestAllExtensions message1, message2;
  TestUtil::SetAllExtensions(&message1);

  TestUtil::ExpectAllExtensionsSet(message1);
  TestUtil::ExpectExtensionsClear(message2);
  message1.Swap(&message2);
  TestUtil::ExpectAllExtensionsSet(message2);
  TestUtil::ExpectExtensionsClear(message1);
}

TEST(ExtensionSetTest, SwapWithSelf) {
  unittest::TestAllExtensions message;
  TestUtil::SetAllExtensions(&message);

  TestUtil::ExpectAllExtensionsSet(message);
  message.Swap(&message);
  TestUtil::ExpectAllExtensionsSet(message);
}

TEST(ExtensionSetTest, SwapExtension) {
  unittest::TestAllExtensions message1;
  unittest::TestAllExtensions message2;

  TestUtil::SetAllExtensions(&message1);
  std::vector<const FieldDescriptor*> fields;

  // Swap empty fields.
  const Reflection* reflection = message1.GetReflection();
  reflection->SwapFields(&message1, &message2, fields);
  TestUtil::ExpectAllExtensionsSet(message1);
  TestUtil::ExpectExtensionsClear(message2);

  // Swap two extensions.
  fields.push_back(reflection->FindKnownExtensionByNumber(12));
  fields.push_back(reflection->FindKnownExtensionByNumber(25));
  reflection->SwapFields(&message1, &message2, fields);

  EXPECT_TRUE(message1.HasExtension(unittest::optional_int32_extension));
  EXPECT_FALSE(message1.HasExtension(unittest::optional_double_extension));
  EXPECT_FALSE(message1.HasExtension(unittest::optional_cord_extension));

  EXPECT_FALSE(message2.HasExtension(unittest::optional_int32_extension));
  EXPECT_TRUE(message2.HasExtension(unittest::optional_double_extension));
  EXPECT_TRUE(message2.HasExtension(unittest::optional_cord_extension));
}

TEST(ExtensionSetTest, SwapExtensionWithEmpty) {
  unittest::TestAllExtensions message1;
  unittest::TestAllExtensions message2;
  unittest::TestAllExtensions message3;

  TestUtil::SetAllExtensions(&message3);

  const Reflection* reflection = message3.GetReflection();
  std::vector<const FieldDescriptor*> fields;
  reflection->ListFields(message3, &fields);

  reflection->SwapFields(&message1, &message2, fields);

  TestUtil::ExpectExtensionsClear(message1);
  TestUtil::ExpectExtensionsClear(message2);
}

TEST(ExtensionSetTest, SwapExtensionBothFull) {
  unittest::TestAllExtensions message1;
  unittest::TestAllExtensions message2;

  TestUtil::SetAllExtensions(&message1);
  TestUtil::SetAllExtensions(&message2);

  const Reflection* reflection = message1.GetReflection();
  std::vector<const FieldDescriptor*> fields;
  reflection->ListFields(message1, &fields);

  reflection->SwapFields(&message1, &message2, fields);

  TestUtil::ExpectAllExtensionsSet(message1);
  TestUtil::ExpectAllExtensionsSet(message2);
}

TEST(ExtensionSetTest, ArenaSetAllExtension) {
  Arena arena1;
  unittest::TestAllExtensions* message1 =
      Arena::CreateMessage<unittest::TestAllExtensions>(&arena1);
  TestUtil::SetAllExtensions(message1);
  TestUtil::ExpectAllExtensionsSet(*message1);
}

TEST(ExtensionSetTest, ArenaCopyConstructor) {
  Arena arena1;
  unittest::TestAllExtensions* message1 =
      Arena::CreateMessage<unittest::TestAllExtensions>(&arena1);
  TestUtil::SetAllExtensions(message1);
  unittest::TestAllExtensions message2(*message1);
  arena1.Reset();
  TestUtil::ExpectAllExtensionsSet(message2);
}

TEST(ExtensionSetTest, ArenaMergeFrom) {
  Arena arena1;
  unittest::TestAllExtensions* message1 =
      Arena::CreateMessage<unittest::TestAllExtensions>(&arena1);
  TestUtil::SetAllExtensions(message1);
  unittest::TestAllExtensions message2;
  message2.MergeFrom(*message1);
  arena1.Reset();
  TestUtil::ExpectAllExtensionsSet(message2);
}

TEST(ExtensionSetTest, ArenaSetAllocatedMessageAndRelease) {
  Arena arena;
  unittest::TestAllExtensions* message =
      Arena::CreateMessage<unittest::TestAllExtensions>(&arena);
  EXPECT_FALSE(
      message->HasExtension(unittest::optional_foreign_message_extension));
  // Add a extension using SetAllocatedExtension
  unittest::ForeignMessage* foreign_message = new unittest::ForeignMessage();
  message->SetAllocatedExtension(unittest::optional_foreign_message_extension,
                                 foreign_message);
  // foreign_message is now owned by the arena.
  EXPECT_EQ(foreign_message, message->MutableExtension(
                                 unittest::optional_foreign_message_extension));

  // Underlying message is copied, and returned.
  unittest::ForeignMessage* released_message =
      message->ReleaseExtension(unittest::optional_foreign_message_extension);
  delete released_message;
  EXPECT_FALSE(
      message->HasExtension(unittest::optional_foreign_message_extension));
}

TEST(ExtensionSetTest, SwapExtensionBothFullWithArena) {
  Arena arena1;
  std::unique_ptr<Arena> arena2(new Arena());

  unittest::TestAllExtensions* message1 =
      Arena::CreateMessage<unittest::TestAllExtensions>(&arena1);
  unittest::TestAllExtensions* message2 =
      Arena::CreateMessage<unittest::TestAllExtensions>(arena2.get());

  TestUtil::SetAllExtensions(message1);
  TestUtil::SetAllExtensions(message2);
  message1->SetExtension(unittest::optional_int32_extension, 1);
  message2->SetExtension(unittest::optional_int32_extension, 2);
  message1->Swap(message2);
  EXPECT_EQ(2, message1->GetExtension(unittest::optional_int32_extension));
  EXPECT_EQ(1, message2->GetExtension(unittest::optional_int32_extension));
  // Re-set the original values so ExpectAllExtensionsSet is happy.
  message1->SetExtension(unittest::optional_int32_extension, 101);
  message2->SetExtension(unittest::optional_int32_extension, 101);
  TestUtil::ExpectAllExtensionsSet(*message1);
  TestUtil::ExpectAllExtensionsSet(*message2);
  arena2.reset(NULL);
  TestUtil::ExpectAllExtensionsSet(*message1);
  // Test corner cases, when one is empty and other is not.
  Arena arena3, arena4;

  unittest::TestAllExtensions* message3 =
      Arena::CreateMessage<unittest::TestAllExtensions>(&arena3);
  unittest::TestAllExtensions* message4 =
      Arena::CreateMessage<unittest::TestAllExtensions>(&arena4);
  TestUtil::SetAllExtensions(message3);
  message3->Swap(message4);
  arena3.Reset();
  TestUtil::ExpectAllExtensionsSet(*message4);
}

TEST(ExtensionSetTest, SwapFieldsOfExtensionBothFullWithArena) {
  Arena arena1;
  Arena* arena2 = new Arena();

  unittest::TestAllExtensions* message1 =
      Arena::CreateMessage<unittest::TestAllExtensions>(&arena1);
  unittest::TestAllExtensions* message2 =
      Arena::CreateMessage<unittest::TestAllExtensions>(arena2);

  TestUtil::SetAllExtensions(message1);
  TestUtil::SetAllExtensions(message2);

  const Reflection* reflection = message1->GetReflection();
  std::vector<const FieldDescriptor*> fields;
  reflection->ListFields(*message1, &fields);
  reflection->SwapFields(message1, message2, fields);
  TestUtil::ExpectAllExtensionsSet(*message1);
  TestUtil::ExpectAllExtensionsSet(*message2);
  delete arena2;
  TestUtil::ExpectAllExtensionsSet(*message1);
}

TEST(ExtensionSetTest, SwapExtensionWithSelf) {
  unittest::TestAllExtensions message1;

  TestUtil::SetAllExtensions(&message1);

  std::vector<const FieldDescriptor*> fields;
  const Reflection* reflection = message1.GetReflection();
  reflection->ListFields(message1, &fields);
  reflection->SwapFields(&message1, &message1, fields);

  TestUtil::ExpectAllExtensionsSet(message1);
}

TEST(ExtensionSetTest, SerializationToArray) {
  // Serialize as TestAllExtensions and parse as TestAllTypes to insure wire
  // compatibility of extensions.
  //
  // This checks serialization to a flat array by explicitly reserving space in
  // the string and calling the generated message's
  // SerializeWithCachedSizesToArray.
  unittest::TestAllExtensions source;
  unittest::TestAllTypes destination;
  TestUtil::SetAllExtensions(&source);
  int size = source.ByteSize();
  std::string data;
  data.resize(size);
  uint8* target = reinterpret_cast<uint8*>(::google::protobuf::string_as_array(&data));
  uint8* end = source.SerializeWithCachedSizesToArray(target);
  EXPECT_EQ(size, end - target);
  EXPECT_TRUE(destination.ParseFromString(data));
  TestUtil::ExpectAllFieldsSet(destination);
}

TEST(ExtensionSetTest, SerializationToStream) {
  // Serialize as TestAllExtensions and parse as TestAllTypes to insure wire
  // compatibility of extensions.
  //
  // This checks serialization to an output stream by creating an array output
  // stream that can only buffer 1 byte at a time - this prevents the message
  // from ever jumping to the fast path, ensuring that serialization happens via
  // the CodedOutputStream.
  unittest::TestAllExtensions source;
  unittest::TestAllTypes destination;
  TestUtil::SetAllExtensions(&source);
  int size = source.ByteSize();
  std::string data;
  data.resize(size);
  {
    io::ArrayOutputStream array_stream(::google::protobuf::string_as_array(&data), size, 1);
    io::CodedOutputStream output_stream(&array_stream);
    source.SerializeWithCachedSizes(&output_stream);
    ASSERT_FALSE(output_stream.HadError());
  }
  EXPECT_TRUE(destination.ParseFromString(data));
  TestUtil::ExpectAllFieldsSet(destination);
}

TEST(ExtensionSetTest, PackedSerializationToArray) {
  // Serialize as TestPackedExtensions and parse as TestPackedTypes to insure
  // wire compatibility of extensions.
  //
  // This checks serialization to a flat array by explicitly reserving space in
  // the string and calling the generated message's
  // SerializeWithCachedSizesToArray.
  unittest::TestPackedExtensions source;
  unittest::TestPackedTypes destination;
  TestUtil::SetPackedExtensions(&source);
  int size = source.ByteSize();
  std::string data;
  data.resize(size);
  uint8* target = reinterpret_cast<uint8*>(::google::protobuf::string_as_array(&data));
  uint8* end = source.SerializeWithCachedSizesToArray(target);
  EXPECT_EQ(size, end - target);
  EXPECT_TRUE(destination.ParseFromString(data));
  TestUtil::ExpectPackedFieldsSet(destination);
}

TEST(ExtensionSetTest, PackedSerializationToStream) {
  // Serialize as TestPackedExtensions and parse as TestPackedTypes to insure
  // wire compatibility of extensions.
  //
  // This checks serialization to an output stream by creating an array output
  // stream that can only buffer 1 byte at a time - this prevents the message
  // from ever jumping to the fast path, ensuring that serialization happens via
  // the CodedOutputStream.
  unittest::TestPackedExtensions source;
  unittest::TestPackedTypes destination;
  TestUtil::SetPackedExtensions(&source);
  int size = source.ByteSize();
  std::string data;
  data.resize(size);
  {
    io::ArrayOutputStream array_stream(::google::protobuf::string_as_array(&data), size, 1);
    io::CodedOutputStream output_stream(&array_stream);
    source.SerializeWithCachedSizes(&output_stream);
    ASSERT_FALSE(output_stream.HadError());
  }
  EXPECT_TRUE(destination.ParseFromString(data));
  TestUtil::ExpectPackedFieldsSet(destination);
}

TEST(ExtensionSetTest, NestedExtensionGroup) {
  // Serialize as TestGroup and parse as TestGroupExtension.
  unittest::TestGroup source;
  unittest::TestGroupExtension destination;
  std::string data;

  source.mutable_optionalgroup()->set_a(117);
  source.set_optional_foreign_enum(unittest::FOREIGN_BAZ);
  source.SerializeToString(&data);
  EXPECT_TRUE(destination.ParseFromString(data));
  EXPECT_TRUE(
      destination
          .GetExtension(unittest::TestNestedExtension::optionalgroup_extension)
          .has_a());
  EXPECT_EQ(117, destination
                     .GetExtension(
                         unittest::TestNestedExtension::optionalgroup_extension)
                     .a());
  EXPECT_TRUE(destination.HasExtension(
      unittest::TestNestedExtension::optional_foreign_enum_extension));
  EXPECT_EQ(
      unittest::FOREIGN_BAZ,
      destination.GetExtension(
          unittest::TestNestedExtension::optional_foreign_enum_extension));
}

TEST(ExtensionSetTest, Parsing) {
  // Serialize as TestAllTypes and parse as TestAllExtensions.
  unittest::TestAllTypes source;
  unittest::TestAllExtensions destination;
  std::string data;

  TestUtil::SetAllFields(&source);
  source.SerializeToString(&data);
  EXPECT_TRUE(destination.ParseFromString(data));
  TestUtil::SetOneofFields(&destination);
  TestUtil::ExpectAllExtensionsSet(destination);
}

TEST(ExtensionSetTest, PackedParsing) {
  // Serialize as TestPackedTypes and parse as TestPackedExtensions.
  unittest::TestPackedTypes source;
  unittest::TestPackedExtensions destination;
  std::string data;

  TestUtil::SetPackedFields(&source);
  source.SerializeToString(&data);
  EXPECT_TRUE(destination.ParseFromString(data));
  TestUtil::ExpectPackedExtensionsSet(destination);
}

TEST(ExtensionSetTest, PackedToUnpackedParsing) {
  unittest::TestPackedTypes source;
  unittest::TestUnpackedExtensions destination;
  std::string data;

  TestUtil::SetPackedFields(&source);
  source.SerializeToString(&data);
  EXPECT_TRUE(destination.ParseFromString(data));
  TestUtil::ExpectUnpackedExtensionsSet(destination);

  // Reserialize
  unittest::TestUnpackedTypes unpacked;
  TestUtil::SetUnpackedFields(&unpacked);
  // Serialized proto has to be the same size and parsed to the same message.
  EXPECT_EQ(unpacked.SerializeAsString().size(),
            destination.SerializeAsString().size());
  EXPECT_TRUE(EqualsToSerialized(unpacked, destination.SerializeAsString()));

  // Make sure we can add extensions.
  destination.AddExtension(unittest::unpacked_int32_extension, 1);
  destination.AddExtension(unittest::unpacked_enum_extension,
                           protobuf_unittest::FOREIGN_BAR);
}

TEST(ExtensionSetTest, UnpackedToPackedParsing) {
  unittest::TestUnpackedTypes source;
  unittest::TestPackedExtensions destination;
  std::string data;

  TestUtil::SetUnpackedFields(&source);
  source.SerializeToString(&data);
  EXPECT_TRUE(destination.ParseFromString(data));
  TestUtil::ExpectPackedExtensionsSet(destination);

  // Reserialize
  unittest::TestPackedTypes packed;
  TestUtil::SetPackedFields(&packed);
  // Serialized proto has to be the same size and parsed to the same message.
  EXPECT_EQ(packed.SerializeAsString().size(),
            destination.SerializeAsString().size());
  EXPECT_TRUE(EqualsToSerialized(packed, destination.SerializeAsString()));

  // Make sure we can add extensions.
  destination.AddExtension(unittest::packed_int32_extension, 1);
  destination.AddExtension(unittest::packed_enum_extension,
                           protobuf_unittest::FOREIGN_BAR);
}

TEST(ExtensionSetTest, IsInitialized) {
  // Test that IsInitialized() returns false if required fields in nested
  // extensions are missing.
  unittest::TestAllExtensions message;

  EXPECT_TRUE(message.IsInitialized());

  message.MutableExtension(unittest::TestRequired::single);
  EXPECT_FALSE(message.IsInitialized());

  message.MutableExtension(unittest::TestRequired::single)->set_a(1);
  EXPECT_FALSE(message.IsInitialized());
  message.MutableExtension(unittest::TestRequired::single)->set_b(2);
  EXPECT_FALSE(message.IsInitialized());
  message.MutableExtension(unittest::TestRequired::single)->set_c(3);
  EXPECT_TRUE(message.IsInitialized());

  message.AddExtension(unittest::TestRequired::multi);
  EXPECT_FALSE(message.IsInitialized());

  message.MutableExtension(unittest::TestRequired::multi, 0)->set_a(1);
  EXPECT_FALSE(message.IsInitialized());
  message.MutableExtension(unittest::TestRequired::multi, 0)->set_b(2);
  EXPECT_FALSE(message.IsInitialized());
  message.MutableExtension(unittest::TestRequired::multi, 0)->set_c(3);
  EXPECT_TRUE(message.IsInitialized());
}

TEST(ExtensionSetTest, MutableString) {
  // Test the mutable string accessors.
  unittest::TestAllExtensions message;

  message.MutableExtension(unittest::optional_string_extension)->assign("foo");
  EXPECT_TRUE(message.HasExtension(unittest::optional_string_extension));
  EXPECT_EQ("foo", message.GetExtension(unittest::optional_string_extension));

  message.AddExtension(unittest::repeated_string_extension)->assign("bar");
  ASSERT_EQ(1, message.ExtensionSize(unittest::repeated_string_extension));
  EXPECT_EQ("bar",
            message.GetExtension(unittest::repeated_string_extension, 0));
}

TEST(ExtensionSetTest, SpaceUsedExcludingSelf) {
  // Scalar primitive extensions should increase the extension set size by a
  // minimum of the size of the primitive type.
#define TEST_SCALAR_EXTENSIONS_SPACE_USED(type, value)                       \
  do {                                                                       \
    unittest::TestAllExtensions message;                                     \
    const int base_size = message.SpaceUsed();                               \
    message.SetExtension(unittest::optional_##type##_extension, value);      \
    int min_expected_size =                                                  \
        base_size +                                                          \
        sizeof(message.GetExtension(unittest::optional_##type##_extension)); \
    EXPECT_LE(min_expected_size, message.SpaceUsed());                       \
  } while (0)

  TEST_SCALAR_EXTENSIONS_SPACE_USED(int32, 101);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(int64, 102);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(uint32, 103);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(uint64, 104);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(sint32, 105);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(sint64, 106);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(fixed32, 107);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(fixed64, 108);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(sfixed32, 109);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(sfixed64, 110);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(float, 111);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(double, 112);
  TEST_SCALAR_EXTENSIONS_SPACE_USED(bool, true);
#undef TEST_SCALAR_EXTENSIONS_SPACE_USED
  {
    unittest::TestAllExtensions message;
    const int base_size = message.SpaceUsed();
    message.SetExtension(unittest::optional_nested_enum_extension,
                         unittest::TestAllTypes::FOO);
    int min_expected_size =
        base_size +
        sizeof(message.GetExtension(unittest::optional_nested_enum_extension));
    EXPECT_LE(min_expected_size, message.SpaceUsed());
  }
  {
    // Strings may cause extra allocations depending on their length; ensure
    // that gets included as well.
    unittest::TestAllExtensions message;
    const int base_size = message.SpaceUsed();
    const std::string s(
        "this is a fairly large string that will cause some "
        "allocation in order to store it in the extension");
    message.SetExtension(unittest::optional_string_extension, s);
    int min_expected_size = base_size + s.length();
    EXPECT_LE(min_expected_size, message.SpaceUsed());
  }
  {
    // Messages also have additional allocation that need to be counted.
    unittest::TestAllExtensions message;
    const int base_size = message.SpaceUsed();
    unittest::ForeignMessage foreign;
    foreign.set_c(42);
    message.MutableExtension(unittest::optional_foreign_message_extension)
        ->CopyFrom(foreign);
    int min_expected_size = base_size + foreign.SpaceUsed();
    EXPECT_LE(min_expected_size, message.SpaceUsed());
  }

  // Repeated primitive extensions will increase space used by at least a
  // RepeatedField<T>, and will cause additional allocations when the array
  // gets too big for the initial space.
  // This macro:
  //   - Adds a value to the repeated extension, then clears it, establishing
  //     the base size.
  //   - Adds a small number of values, testing that it doesn't increase the
  //     SpaceUsed()
  //   - Adds a large number of values (requiring allocation in the repeated
  //     field), and ensures that that allocation is included in SpaceUsed()
#define TEST_REPEATED_EXTENSIONS_SPACE_USED(type, cpptype, value)             \
  do {                                                                        \
    unittest::TestAllExtensions message;                                      \
    const int base_size = message.SpaceUsed();                                \
    int min_expected_size = sizeof(RepeatedField<cpptype>) + base_size;       \
    message.AddExtension(unittest::repeated_##type##_extension, value);       \
    message.ClearExtension(unittest::repeated_##type##_extension);            \
    const int empty_repeated_field_size = message.SpaceUsed();                \
    EXPECT_LE(min_expected_size, empty_repeated_field_size) << #type;         \
    message.AddExtension(unittest::repeated_##type##_extension, value);       \
    message.AddExtension(unittest::repeated_##type##_extension, value);       \
    EXPECT_EQ(empty_repeated_field_size, message.SpaceUsed()) << #type;       \
    message.ClearExtension(unittest::repeated_##type##_extension);            \
    const int old_capacity =                                                  \
        message.GetRepeatedExtension(unittest::repeated_##type##_extension)   \
            .Capacity();                                                      \
    EXPECT_GE(old_capacity, kMinRepeatedFieldAllocationSize);                 \
    for (int i = 0; i < 16; ++i) {                                            \
      message.AddExtension(unittest::repeated_##type##_extension, value);     \
    }                                                                         \
    int expected_size =                                                       \
        sizeof(cpptype) *                                                     \
            (message                                                          \
                 .GetRepeatedExtension(unittest::repeated_##type##_extension) \
                 .Capacity() -                                                \
             old_capacity) +                                                  \
        empty_repeated_field_size;                                            \
    EXPECT_LE(expected_size, message.SpaceUsed()) << #type;                   \
  } while (0)

  TEST_REPEATED_EXTENSIONS_SPACE_USED(int32, int32, 101);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(int64, int64, 102);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(uint32, uint32, 103);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(uint64, uint64, 104);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(sint32, int32, 105);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(sint64, int64, 106);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(fixed32, uint32, 107);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(fixed64, uint64, 108);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(sfixed32, int32, 109);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(sfixed64, int64, 110);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(float, float, 111);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(double, double, 112);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(bool, bool, true);
  TEST_REPEATED_EXTENSIONS_SPACE_USED(nested_enum, int,
                                      unittest::TestAllTypes::FOO);
#undef TEST_REPEATED_EXTENSIONS_SPACE_USED
  // Repeated strings
  {
    unittest::TestAllExtensions message;
    const int base_size = message.SpaceUsed();
    int min_expected_size = sizeof(RepeatedPtrField<std::string>) + base_size;
    const std::string value(256, 'x');
    // Once items are allocated, they may stick around even when cleared so
    // without the hardcore memory management accessors there isn't a notion of
    // the empty repeated field memory usage as there is with primitive types.
    for (int i = 0; i < 16; ++i) {
      message.AddExtension(unittest::repeated_string_extension, value);
    }
    min_expected_size +=
        (sizeof(value) + value.size()) * (16 - kMinRepeatedFieldAllocationSize);
    EXPECT_LE(min_expected_size, message.SpaceUsed());
  }
  // Repeated messages
  {
    unittest::TestAllExtensions message;
    const int base_size = message.SpaceUsed();
    int min_expected_size =
        sizeof(RepeatedPtrField<unittest::ForeignMessage>) + base_size;
    unittest::ForeignMessage prototype;
    prototype.set_c(2);
    for (int i = 0; i < 16; ++i) {
      message.AddExtension(unittest::repeated_foreign_message_extension)
          ->CopyFrom(prototype);
    }
    min_expected_size +=
        (16 - kMinRepeatedFieldAllocationSize) * prototype.SpaceUsed();
    EXPECT_LE(min_expected_size, message.SpaceUsed());
  }
}

// N.B.: We do not test range-based for here because we remain C++03 compatible.
template <typename T, typename M, typename ID>
inline T SumAllExtensions(const M& message, ID extension, T zero) {
  T sum = zero;
  typename RepeatedField<T>::const_iterator iter =
      message.GetRepeatedExtension(extension).begin();
  typename RepeatedField<T>::const_iterator end =
      message.GetRepeatedExtension(extension).end();
  for (; iter != end; ++iter) {
    sum += *iter;
  }
  return sum;
}

template <typename T, typename M, typename ID>
inline void IncAllExtensions(M* message, ID extension, T val) {
  typename RepeatedField<T>::iterator iter =
      message->MutableRepeatedExtension(extension)->begin();
  typename RepeatedField<T>::iterator end =
      message->MutableRepeatedExtension(extension)->end();
  for (; iter != end; ++iter) {
    *iter += val;
  }
}

TEST(ExtensionSetTest, RepeatedFields) {
  unittest::TestAllExtensions message;

  // Test empty repeated-field case (b/12926163)
  ASSERT_EQ(
      0,
      message.GetRepeatedExtension(unittest::repeated_int32_extension).size());
  ASSERT_EQ(
      0, message.GetRepeatedExtension(unittest::repeated_nested_enum_extension)
             .size());
  ASSERT_EQ(
      0,
      message.GetRepeatedExtension(unittest::repeated_string_extension).size());
  ASSERT_EQ(
      0,
      message.GetRepeatedExtension(unittest::repeated_nested_message_extension)
          .size());

  unittest::TestAllTypes::NestedMessage nested_message;
  nested_message.set_bb(42);
  unittest::TestAllTypes::NestedEnum nested_enum =
      unittest::TestAllTypes::NestedEnum_MIN;

  for (int i = 0; i < 10; ++i) {
    message.AddExtension(unittest::repeated_int32_extension, 1);
    message.AddExtension(unittest::repeated_int64_extension, 2);
    message.AddExtension(unittest::repeated_uint32_extension, 3);
    message.AddExtension(unittest::repeated_uint64_extension, 4);
    message.AddExtension(unittest::repeated_sint32_extension, 5);
    message.AddExtension(unittest::repeated_sint64_extension, 6);
    message.AddExtension(unittest::repeated_fixed32_extension, 7);
    message.AddExtension(unittest::repeated_fixed64_extension, 8);
    message.AddExtension(unittest::repeated_sfixed32_extension, 7);
    message.AddExtension(unittest::repeated_sfixed64_extension, 8);
    message.AddExtension(unittest::repeated_float_extension, 9.0);
    message.AddExtension(unittest::repeated_double_extension, 10.0);
    message.AddExtension(unittest::repeated_bool_extension, true);
    message.AddExtension(unittest::repeated_nested_enum_extension, nested_enum);
    message.AddExtension(unittest::repeated_string_extension,
                         std::string("test"));
    message.AddExtension(unittest::repeated_bytes_extension,
                         std::string("test\xFF"));
    message.AddExtension(unittest::repeated_nested_message_extension)
        ->CopyFrom(nested_message);
    message.AddExtension(unittest::repeated_nested_enum_extension, nested_enum);
  }

  ASSERT_EQ(10, SumAllExtensions<int32>(message,
                                        unittest::repeated_int32_extension, 0));
  IncAllExtensions<int32>(&message, unittest::repeated_int32_extension, 1);
  ASSERT_EQ(20, SumAllExtensions<int32>(message,
                                        unittest::repeated_int32_extension, 0));

  ASSERT_EQ(20, SumAllExtensions<int64>(message,
                                        unittest::repeated_int64_extension, 0));
  IncAllExtensions<int64>(&message, unittest::repeated_int64_extension, 1);
  ASSERT_EQ(30, SumAllExtensions<int64>(message,
                                        unittest::repeated_int64_extension, 0));

  ASSERT_EQ(30, SumAllExtensions<uint32>(
                    message, unittest::repeated_uint32_extension, 0));
  IncAllExtensions<uint32>(&message, unittest::repeated_uint32_extension, 1);
  ASSERT_EQ(40, SumAllExtensions<uint32>(
                    message, unittest::repeated_uint32_extension, 0));

  ASSERT_EQ(40, SumAllExtensions<uint64>(
                    message, unittest::repeated_uint64_extension, 0));
  IncAllExtensions<uint64>(&message, unittest::repeated_uint64_extension, 1);
  ASSERT_EQ(50, SumAllExtensions<uint64>(
                    message, unittest::repeated_uint64_extension, 0));

  ASSERT_EQ(50, SumAllExtensions<int32>(
                    message, unittest::repeated_sint32_extension, 0));
  IncAllExtensions<int32>(&message, unittest::repeated_sint32_extension, 1);
  ASSERT_EQ(60, SumAllExtensions<int32>(
                    message, unittest::repeated_sint32_extension, 0));

  ASSERT_EQ(60, SumAllExtensions<int64>(
                    message, unittest::repeated_sint64_extension, 0));
  IncAllExtensions<int64>(&message, unittest::repeated_sint64_extension, 1);
  ASSERT_EQ(70, SumAllExtensions<int64>(
                    message, unittest::repeated_sint64_extension, 0));

  ASSERT_EQ(70, SumAllExtensions<uint32>(
                    message, unittest::repeated_fixed32_extension, 0));
  IncAllExtensions<uint32>(&message, unittest::repeated_fixed32_extension, 1);
  ASSERT_EQ(80, SumAllExtensions<uint32>(
                    message, unittest::repeated_fixed32_extension, 0));

  ASSERT_EQ(80, SumAllExtensions<uint64>(
                    message, unittest::repeated_fixed64_extension, 0));
  IncAllExtensions<uint64>(&message, unittest::repeated_fixed64_extension, 1);
  ASSERT_EQ(90, SumAllExtensions<uint64>(
                    message, unittest::repeated_fixed64_extension, 0));

  // Usually, floating-point arithmetic cannot be trusted to be exact, so it is
  // a Bad Idea to assert equality in a test like this. However, we're dealing
  // with integers with a small number of significant mantissa bits, so we
  // should actually have exact precision here.
  ASSERT_EQ(90, SumAllExtensions<float>(message,
                                        unittest::repeated_float_extension, 0));
  IncAllExtensions<float>(&message, unittest::repeated_float_extension, 1);
  ASSERT_EQ(100, SumAllExtensions<float>(
                     message, unittest::repeated_float_extension, 0));

  ASSERT_EQ(100, SumAllExtensions<double>(
                     message, unittest::repeated_double_extension, 0));
  IncAllExtensions<double>(&message, unittest::repeated_double_extension, 1);
  ASSERT_EQ(110, SumAllExtensions<double>(
                     message, unittest::repeated_double_extension, 0));

  RepeatedPtrField<std::string>::iterator string_iter;
  RepeatedPtrField<std::string>::iterator string_end;
  for (string_iter =
           message
               .MutableRepeatedExtension(unittest::repeated_string_extension)
               ->begin(),
      string_end =
           message
               .MutableRepeatedExtension(unittest::repeated_string_extension)
               ->end();
       string_iter != string_end; ++string_iter) {
    *string_iter += "test";
  }
  RepeatedPtrField<std::string>::const_iterator string_const_iter;
  RepeatedPtrField<std::string>::const_iterator string_const_end;
  for (string_const_iter =
           message.GetRepeatedExtension(unittest::repeated_string_extension)
               .begin(),
      string_const_end =
           message.GetRepeatedExtension(unittest::repeated_string_extension)
               .end();
       string_iter != string_end; ++string_iter) {
    ASSERT_TRUE(*string_iter == "testtest");
  }

  RepeatedField<unittest::TestAllTypes_NestedEnum>::iterator enum_iter;
  RepeatedField<unittest::TestAllTypes_NestedEnum>::iterator enum_end;
  for (enum_iter = message
                       .MutableRepeatedExtension(
                           unittest::repeated_nested_enum_extension)
                       ->begin(),
      enum_end = message
                     .MutableRepeatedExtension(
                         unittest::repeated_nested_enum_extension)
                     ->end();
       enum_iter != enum_end; ++enum_iter) {
    *enum_iter = unittest::TestAllTypes::NestedEnum_MAX;
  }
  RepeatedField<unittest::TestAllTypes_NestedEnum>::const_iterator
      enum_const_iter;
  RepeatedField<unittest::TestAllTypes_NestedEnum>::const_iterator
      enum_const_end;
  for (enum_const_iter =
           message
               .GetRepeatedExtension(unittest::repeated_nested_enum_extension)
               .begin(),
      enum_const_end =
           message
               .GetRepeatedExtension(unittest::repeated_nested_enum_extension)
               .end();
       enum_const_iter != enum_const_end; ++enum_const_iter) {
    ASSERT_EQ(*enum_const_iter, unittest::TestAllTypes::NestedEnum_MAX);
  }

  RepeatedPtrField<unittest::TestAllTypes_NestedMessage>::iterator msg_iter;
  RepeatedPtrField<unittest::TestAllTypes_NestedMessage>::iterator msg_end;
  for (msg_iter = message
                      .MutableRepeatedExtension(
                          unittest::repeated_nested_message_extension)
                      ->begin(),
      msg_end = message
                    .MutableRepeatedExtension(
                        unittest::repeated_nested_message_extension)
                    ->end();
       msg_iter != msg_end; ++msg_iter) {
    msg_iter->set_bb(1234);
  }
  RepeatedPtrField<unittest::TestAllTypes_NestedMessage>::const_iterator
      msg_const_iter;
  RepeatedPtrField<unittest::TestAllTypes_NestedMessage>::const_iterator
      msg_const_end;
  for (msg_const_iter = message
                            .GetRepeatedExtension(
                                unittest::repeated_nested_message_extension)
                            .begin(),
      msg_const_end = message
                          .GetRepeatedExtension(
                              unittest::repeated_nested_message_extension)
                          .end();
       msg_const_iter != msg_const_end; ++msg_const_iter) {
    ASSERT_EQ(msg_const_iter->bb(), 1234);
  }

  // Test range-based for as well, but only if compiled as C++11.
#if __cplusplus >= 201103L
  // Test one primitive field.
  for (auto& x :
       *message.MutableRepeatedExtension(unittest::repeated_int32_extension)) {
    x = 4321;
  }
  for (const auto& x :
       message.GetRepeatedExtension(unittest::repeated_int32_extension)) {
    ASSERT_EQ(x, 4321);
  }
  // Test one string field.
  for (auto& x :
       *message.MutableRepeatedExtension(unittest::repeated_string_extension)) {
    x = "test_range_based_for";
  }
  for (const auto& x :
       message.GetRepeatedExtension(unittest::repeated_string_extension)) {
    ASSERT_TRUE(x == "test_range_based_for");
  }
  // Test one message field.
  for (auto& x : *message.MutableRepeatedExtension(
           unittest::repeated_nested_message_extension)) {
    x.set_bb(4321);
  }
  for (const auto& x : *message.MutableRepeatedExtension(
           unittest::repeated_nested_message_extension)) {
    ASSERT_EQ(x.bb(), 4321);
  }
#endif
}

// From b/12926163
TEST(ExtensionSetTest, AbsentExtension) {
  unittest::TestAllExtensions message;
  message.MutableRepeatedExtension(unittest::repeated_nested_message_extension)
      ->Add()
      ->set_bb(123);
  ASSERT_EQ(1,
            message.ExtensionSize(unittest::repeated_nested_message_extension));
  EXPECT_EQ(123,
            message.GetExtension(unittest::repeated_nested_message_extension, 0)
                .bb());
}

#ifdef PROTOBUF_HAS_DEATH_TEST

TEST(ExtensionSetTest, InvalidEnumDeath) {
  unittest::TestAllExtensions message;
  EXPECT_DEBUG_DEATH(
      message.SetExtension(unittest::optional_foreign_enum_extension,
                           static_cast<unittest::ForeignEnum>(53)),
      "IsValid");
}

#endif  // PROTOBUF_HAS_DEATH_TEST

TEST(ExtensionSetTest, DynamicExtensions) {
  // Test adding a dynamic extension to a compiled-in message object.

  FileDescriptorProto dynamic_proto;
  dynamic_proto.set_name("dynamic_extensions_test.proto");
  dynamic_proto.add_dependency(
      unittest::TestAllExtensions::descriptor()->file()->name());
  dynamic_proto.set_package("dynamic_extensions");

  // Copy the fields and nested types from TestDynamicExtensions into our new
  // proto, converting the fields into extensions.
  const Descriptor* template_descriptor =
      unittest::TestDynamicExtensions::descriptor();
  DescriptorProto template_descriptor_proto;
  template_descriptor->CopyTo(&template_descriptor_proto);
  dynamic_proto.mutable_message_type()->MergeFrom(
      template_descriptor_proto.nested_type());
  dynamic_proto.mutable_enum_type()->MergeFrom(
      template_descriptor_proto.enum_type());
  dynamic_proto.mutable_extension()->MergeFrom(
      template_descriptor_proto.field());

  // For each extension that we added...
  for (int i = 0; i < dynamic_proto.extension_size(); i++) {
    // Set its extendee to TestAllExtensions.
    FieldDescriptorProto* extension = dynamic_proto.mutable_extension(i);
    extension->set_extendee(
        unittest::TestAllExtensions::descriptor()->full_name());

    // If the field refers to one of the types nested in TestDynamicExtensions,
    // make it refer to the type in our dynamic proto instead.
    std::string prefix = "." + template_descriptor->full_name() + ".";
    if (extension->has_type_name()) {
      std::string* type_name = extension->mutable_type_name();
      if (HasPrefixString(*type_name, prefix)) {
        type_name->replace(0, prefix.size(), ".dynamic_extensions.");
      }
    }
  }

  // Now build the file, using the generated pool as an underlay.
  DescriptorPool dynamic_pool(DescriptorPool::generated_pool());
  const FileDescriptor* file = dynamic_pool.BuildFile(dynamic_proto);
  ASSERT_TRUE(file != NULL);
  DynamicMessageFactory dynamic_factory(&dynamic_pool);
  dynamic_factory.SetDelegateToGeneratedFactory(true);

  // Construct a message that we can parse with the extensions we defined.
  // Since the extensions were based off of the fields of TestDynamicExtensions,
  // we can use that message to create this test message.
  std::string data;
  unittest::TestDynamicExtensions dynamic_extension;
  {
    unittest::TestDynamicExtensions message;
    message.set_scalar_extension(123);
    message.set_enum_extension(unittest::FOREIGN_BAR);
    message.set_dynamic_enum_extension(
        unittest::TestDynamicExtensions::DYNAMIC_BAZ);
    message.mutable_message_extension()->set_c(456);
    message.mutable_dynamic_message_extension()->set_dynamic_field(789);
    message.add_repeated_extension("foo");
    message.add_repeated_extension("bar");
    message.add_packed_extension(12);
    message.add_packed_extension(-34);
    message.add_packed_extension(56);
    message.add_packed_extension(-78);

    // Also add some unknown fields.

    // An unknown enum value (for a known field).
    message.mutable_unknown_fields()->AddVarint(
        unittest::TestDynamicExtensions::kDynamicEnumExtensionFieldNumber,
        12345);
    // A regular unknown field.
    message.mutable_unknown_fields()->AddLengthDelimited(54321, "unknown");

    message.SerializeToString(&data);
    dynamic_extension = message;
  }

  // Now we can parse this using our dynamic extension definitions...
  unittest::TestAllExtensions message;
  {
    io::ArrayInputStream raw_input(data.data(), data.size());
    io::CodedInputStream input(&raw_input);
    input.SetExtensionRegistry(&dynamic_pool, &dynamic_factory);
    ASSERT_TRUE(message.ParseFromCodedStream(&input));
    ASSERT_TRUE(input.ConsumedEntireMessage());
  }

  // Can we print it?
  EXPECT_EQ(
      "[dynamic_extensions.scalar_extension]: 123\n"
      "[dynamic_extensions.enum_extension]: FOREIGN_BAR\n"
      "[dynamic_extensions.dynamic_enum_extension]: DYNAMIC_BAZ\n"
      "[dynamic_extensions.message_extension] {\n"
      "  c: 456\n"
      "}\n"
      "[dynamic_extensions.dynamic_message_extension] {\n"
      "  dynamic_field: 789\n"
      "}\n"
      "[dynamic_extensions.repeated_extension]: \"foo\"\n"
      "[dynamic_extensions.repeated_extension]: \"bar\"\n"
      "[dynamic_extensions.packed_extension]: 12\n"
      "[dynamic_extensions.packed_extension]: -34\n"
      "[dynamic_extensions.packed_extension]: 56\n"
      "[dynamic_extensions.packed_extension]: -78\n"
      "2002: 12345\n"
      "54321: \"unknown\"\n",
      message.DebugString());

  // Can we serialize it?
  EXPECT_TRUE(
      EqualsToSerialized(dynamic_extension, message.SerializeAsString()));

  // What if we parse using the reflection-based parser?
  {
    unittest::TestAllExtensions message2;
    io::ArrayInputStream raw_input(data.data(), data.size());
    io::CodedInputStream input(&raw_input);
    input.SetExtensionRegistry(&dynamic_pool, &dynamic_factory);
    ASSERT_TRUE(WireFormat::ParseAndMergePartial(&input, &message2));
    ASSERT_TRUE(input.ConsumedEntireMessage());
    EXPECT_EQ(message.DebugString(), message2.DebugString());
  }

  // Are the embedded generated types actually using the generated objects?
  {
    const FieldDescriptor* message_extension =
        file->FindExtensionByName("message_extension");
    ASSERT_TRUE(message_extension != NULL);
    const Message& sub_message =
        message.GetReflection()->GetMessage(message, message_extension);
    const unittest::ForeignMessage* typed_sub_message =
#if PROTOBUF_RTTI
        dynamic_cast<const unittest::ForeignMessage*>(&sub_message);
#else
        static_cast<const unittest::ForeignMessage*>(&sub_message);
#endif
    ASSERT_TRUE(typed_sub_message != NULL);
    EXPECT_EQ(456, typed_sub_message->c());
  }

  // What does GetMessage() return for the embedded dynamic type if it isn't
  // present?
  {
    const FieldDescriptor* dynamic_message_extension =
        file->FindExtensionByName("dynamic_message_extension");
    ASSERT_TRUE(dynamic_message_extension != NULL);
    const Message& parent = unittest::TestAllExtensions::default_instance();
    const Message& sub_message = parent.GetReflection()->GetMessage(
        parent, dynamic_message_extension, &dynamic_factory);
    const Message* prototype =
        dynamic_factory.GetPrototype(dynamic_message_extension->message_type());
    EXPECT_EQ(prototype, &sub_message);
  }
}

}  // namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google
