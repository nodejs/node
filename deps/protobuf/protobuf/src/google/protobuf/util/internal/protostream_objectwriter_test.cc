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

#include <google/protobuf/util/internal/protostream_objectwriter.h>

#include <stddef.h>  // For size_t

#include <google/protobuf/field_mask.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/wrappers.pb.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/internal/mock_error_listener.h>
#include <google/protobuf/util/internal/testdata/anys.pb.h>
#include <google/protobuf/util/internal/testdata/books.pb.h>
#include <google/protobuf/util/internal/testdata/field_mask.pb.h>
#include <google/protobuf/util/internal/testdata/maps.pb.h>
#include <google/protobuf/util/internal/testdata/oneofs.pb.h>
#include <google/protobuf/util/internal/testdata/proto3.pb.h>
#include <google/protobuf/util/internal/testdata/struct.pb.h>
#include <google/protobuf/util/internal/testdata/timestamp_duration.pb.h>
#include <google/protobuf/util/internal/testdata/wrappers.pb.h>
#include <google/protobuf/util/internal/type_info_test_helper.h>
#include <google/protobuf/util/internal/constants.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/protobuf/stubs/bytestream.h>
#include <google/protobuf/stubs/strutil.h>
#include <gtest/gtest.h>


namespace google {
namespace protobuf {
namespace util {
namespace converter {

using proto_util_converter::testing::AnyM;
using proto_util_converter::testing::AnyOut;
using proto_util_converter::testing::Author;
using proto_util_converter::testing::Book;
using proto_util_converter::testing::FieldMaskTest;
using proto_util_converter::testing::Int32Wrapper;
using proto_util_converter::testing::MapIn;
using proto_util_converter::testing::Primitive;
using proto_util_converter::testing::Proto3Message;
using proto_util_converter::testing::Publisher;
using proto_util_converter::testing::StructType;
using proto_util_converter::testing::TestJsonName1;
using proto_util_converter::testing::TestJsonName2;
using proto_util_converter::testing::TimestampDuration;
using proto_util_converter::testing::ValueWrapper;
using proto_util_converter::testing::oneofs::OneOfsRequest;
using strings::GrowingArrayByteSink;
using ::testing::_;
using ::testing::Args;


namespace {
std::string GetTypeUrl(const Descriptor* descriptor) {
  return std::string(kTypeServiceBaseUrl) + "/" + descriptor->full_name();
}
}  // namespace

#if __cplusplus >= 201103L
using std::get;
#else
using std::tr1::get;
#endif

class BaseProtoStreamObjectWriterTest
    : public ::testing::TestWithParam<testing::TypeInfoSource> {
 protected:
  BaseProtoStreamObjectWriterTest()
      : helper_(GetParam()),
        listener_(),
        output_(new GrowingArrayByteSink(1000)),
        ow_() {}

  explicit BaseProtoStreamObjectWriterTest(const Descriptor* descriptor)
      : helper_(GetParam()),
        listener_(),
        output_(new GrowingArrayByteSink(1000)),
        ow_() {
    std::vector<const Descriptor*> descriptors;
    descriptors.push_back(descriptor);
    ResetTypeInfo(descriptors);
  }

  explicit BaseProtoStreamObjectWriterTest(
      std::vector<const Descriptor*> descriptors)
      : helper_(GetParam()),
        listener_(),
        output_(new GrowingArrayByteSink(1000)),
        ow_() {
    ResetTypeInfo(descriptors);
  }

  void ResetTypeInfo(std::vector<const Descriptor*> descriptors) {
    GOOGLE_CHECK(!descriptors.empty()) << "Must have at least one descriptor!";
    helper_.ResetTypeInfo(descriptors);
    ow_.reset(helper_.NewProtoWriter(GetTypeUrl(descriptors[0]), output_.get(),
                                     &listener_, options_));
  }

  void ResetTypeInfo(const Descriptor* descriptor) {
    std::vector<const Descriptor*> descriptors;
    descriptors.push_back(descriptor);
    ResetTypeInfo(descriptors);
  }

  virtual ~BaseProtoStreamObjectWriterTest() {}

  void CheckOutput(const Message& expected, int expected_length) {
    size_t nbytes;
    std::unique_ptr<char[]> buffer(output_->GetBuffer(&nbytes));
    if (expected_length >= 0) {
      EXPECT_EQ(expected_length, nbytes);
    }
    std::string str(buffer.get(), nbytes);

    std::stringbuf str_buf(str, std::ios_base::in);
    std::istream istream(&str_buf);
    std::unique_ptr<Message> message(expected.New());
    message->ParsePartialFromIstream(&istream);

    if (!MessageDifferencer::Equivalent(expected, *message)) {
      EXPECT_EQ(expected.DebugString(), message->DebugString());
    }
  }

  void CheckOutput(const Message& expected) { CheckOutput(expected, -1); }

  testing::TypeInfoTestHelper helper_;
  MockErrorListener listener_;
  std::unique_ptr<GrowingArrayByteSink> output_;
  std::unique_ptr<ProtoStreamObjectWriter> ow_;
  ProtoStreamObjectWriter::Options options_;
};

MATCHER_P(HasObjectLocation, expected,
          "Verifies the expected object location") {
  std::string actual = get<0>(arg).ToString();
  if (actual.compare(expected) == 0) return true;
  *result_listener << "actual location is: " << actual;
  return false;
}

class ProtoStreamObjectWriterTest : public BaseProtoStreamObjectWriterTest {
 protected:
  ProtoStreamObjectWriterTest()
      : BaseProtoStreamObjectWriterTest(Book::descriptor()) {}

  void ResetProtoWriter() { ResetTypeInfo(Book::descriptor()); }

  virtual ~ProtoStreamObjectWriterTest() {}
};

INSTANTIATE_TEST_SUITE_P(DifferentTypeInfoSourceTest,
                         ProtoStreamObjectWriterTest,
                         ::testing::Values(
                             testing::USE_TYPE_RESOLVER));

TEST_P(ProtoStreamObjectWriterTest, EmptyObject) {
  Book empty;
  ow_->StartObject("")->EndObject();
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, SimpleObject) {
  std::string content("My content");

  Book book;
  book.set_title("My Title");
  book.set_length(222);
  book.set_content(content);

  ow_->StartObject("")
      ->RenderString("title", "My Title")
      ->RenderInt32("length", 222)
      ->RenderBytes("content", content)
      ->EndObject();
  CheckOutput(book);
}

TEST_P(ProtoStreamObjectWriterTest, SimpleMessage) {
  Book book;
  book.set_title("Some Book");
  book.set_length(102);
  Publisher* publisher = book.mutable_publisher();
  publisher->set_name("My Publisher");
  Author* robert = book.mutable_author();
  robert->set_alive(true);
  robert->set_name("robert");
  robert->add_pseudonym("bob");
  robert->add_pseudonym("bobby");
  robert->add_friend_()->set_name("john");

  ow_->StartObject("")
      ->RenderString("title", "Some Book")
      ->RenderInt32("length", 102)
      ->StartObject("publisher")
      ->RenderString("name", "My Publisher")
      ->EndObject()
      ->StartObject("author")
      ->RenderBool("alive", true)
      ->RenderString("name", "robert")
      ->StartList("pseudonym")
      ->RenderString("", "bob")
      ->RenderString("", "bobby")
      ->EndList()
      ->StartList("friend")
      ->StartObject("")
      ->RenderString("name", "john")
      ->EndObject()
      ->EndList()
      ->EndObject()
      ->EndObject();
  CheckOutput(book);
}

TEST_P(ProtoStreamObjectWriterTest, CustomJsonName) {
  Book book;
  Author* robert = book.mutable_author();
  robert->set_id(12345);
  robert->set_name("robert");

  ow_->StartObject("")
      ->StartObject("author")
      ->RenderUint64("@id", 12345)
      ->RenderString("name", "robert")
      ->EndObject()
      ->EndObject();
  CheckOutput(book);
}

// Test that two messages can have different fields mapped to the same JSON
// name. See: https://github.com/protocolbuffers/protobuf/issues/1415
TEST_P(ProtoStreamObjectWriterTest, ConflictingJsonName) {
  ResetTypeInfo(TestJsonName1::descriptor());
  TestJsonName1 message1;
  message1.set_one_value(12345);
  ow_->StartObject("")->RenderInt32("value", 12345)->EndObject();
  CheckOutput(message1);

  ResetTypeInfo(TestJsonName2::descriptor());
  TestJsonName2 message2;
  message2.set_another_value(12345);
  ow_->StartObject("")->RenderInt32("value", 12345)->EndObject();
  CheckOutput(message2);
}

TEST_P(ProtoStreamObjectWriterTest, IntEnumValuesAreAccepted) {
  Book book;
  book.set_title("Some Book");
  book.set_type(proto_util_converter::testing::Book_Type_KIDS);
  Author* robert = book.mutable_author();
  robert->set_name("robert");

  ow_->StartObject("")
      ->RenderString("title", "Some Book")
      ->RenderString("type", "2")
      ->StartObject("author")
      ->RenderString("name", "robert")
      ->EndObject()
      ->EndObject();
  CheckOutput(book);
}

TEST_P(ProtoStreamObjectWriterTest, EnumValuesWithDifferentCaseIsRejected) {
  Book book;
  book.set_title("Some Book");
  Author* robert = book.mutable_author();
  robert->set_name("robert");

  options_.case_insensitive_enum_parsing = false;
  ResetProtoWriter();

  ow_->StartObject("")
      ->RenderString("title", "Some Book")
      ->RenderString("type", "action_and_adventure")
      ->StartObject("author")
      ->RenderString("name", "robert")
      ->EndObject()
      ->EndObject();
  CheckOutput(book);
}

TEST_P(ProtoStreamObjectWriterTest, EnumValuesWithSameCaseIsAccepted) {
  Book book;
  book.set_title("Some Book");
  book.set_type(proto_util_converter::testing::Book_Type_ACTION_AND_ADVENTURE);
  Author* robert = book.mutable_author();
  robert->set_name("robert");

  options_.case_insensitive_enum_parsing = false;
  ResetProtoWriter();

  ow_->StartObject("")
      ->RenderString("title", "Some Book")
      ->RenderString("type", "ACTION_AND_ADVENTURE")
      ->StartObject("author")
      ->RenderString("name", "robert")
      ->EndObject()
      ->EndObject();
  CheckOutput(book);
}

TEST_P(ProtoStreamObjectWriterTest, EnumValuesWithDifferentCaseIsAccepted) {
  Book book;
  book.set_title("Some Book");
  book.set_type(proto_util_converter::testing::Book_Type_ACTION_AND_ADVENTURE);
  Author* robert = book.mutable_author();
  robert->set_name("robert");

  options_.case_insensitive_enum_parsing = true;
  ResetProtoWriter();

  ow_->StartObject("")
      ->RenderString("title", "Some Book")
      ->RenderString("type", "action_AND_adventure")
      ->StartObject("author")
      ->RenderString("name", "robert")
      ->EndObject()
      ->EndObject();
  CheckOutput(book);
}

TEST_P(ProtoStreamObjectWriterTest, EnumValuesWithoutUnderscoreAreAccepted) {
  Book book;
  book.set_title("Some Book");
  book.set_type(proto_util_converter::testing::Book_Type_ACTION_AND_ADVENTURE);
  Author* robert = book.mutable_author();
  robert->set_name("robert");

  options_.use_lower_camel_for_enums = true;
  ResetProtoWriter();

  ow_->StartObject("")
      ->RenderString("title", "Some Book")
      ->RenderString("type", "ACTIONANDADVENTURE")
      ->StartObject("author")
      ->RenderString("name", "robert")
      ->EndObject()
      ->EndObject();
  CheckOutput(book);
}

TEST_P(ProtoStreamObjectWriterTest, EnumValuesInCamelCaseAreAccepted) {
  Book book;
  book.set_title("Some Book");
  book.set_type(proto_util_converter::testing::Book_Type_ACTION_AND_ADVENTURE);
  Author* robert = book.mutable_author();
  robert->set_name("robert");

  options_.use_lower_camel_for_enums = true;
  ResetProtoWriter();

  ow_->StartObject("")
      ->RenderString("title", "Some Book")
      ->RenderString("type", "actionAndAdventure")
      ->StartObject("author")
      ->RenderString("name", "robert")
      ->EndObject()
      ->EndObject();
  CheckOutput(book);
}

TEST_P(ProtoStreamObjectWriterTest,
       EnumValuesInCamelCaseRemoveDashAndUnderscoreAreAccepted) {
  Book book;
  book.set_title("Some Book");
  book.set_type(proto_util_converter::testing::Book_Type_ACTION_AND_ADVENTURE);
  Author* robert = book.mutable_author();
  robert->set_name("robert");

  options_.use_lower_camel_for_enums = true;
  options_.case_insensitive_enum_parsing = false;
  ResetProtoWriter();

  ow_->StartObject("")
      ->RenderString("title", "Some Book")
      ->RenderString("type", "action-And_Adventure")
      ->StartObject("author")
      ->RenderString("name", "robert")
      ->EndObject()
      ->EndObject();
  CheckOutput(book);
}

TEST_P(ProtoStreamObjectWriterTest,
       EnumValuesInCamelCaseWithNameNotUppercaseAreAccepted) {
  Book book;
  book.set_title("Some Book");
  book.set_type(proto_util_converter::testing::Book_Type_arts_and_photography);
  Author* robert = book.mutable_author();
  robert->set_name("robert");

  options_.use_lower_camel_for_enums = true;
  ResetProtoWriter();

  ow_->StartObject("")
      ->RenderString("title", "Some Book")
      ->RenderString("type", "artsAndPhotography")
      ->StartObject("author")
      ->RenderString("name", "robert")
      ->EndObject()
      ->EndObject();
  CheckOutput(book);
}

TEST_P(ProtoStreamObjectWriterTest, PrimitiveFromStringConversion) {
  Primitive full;
  full.set_fix32(101);
  full.set_u32(102);
  full.set_i32(-103);
  full.set_sf32(-104);
  full.set_s32(-105);
  full.set_fix64(40000000001L);
  full.set_u64(40000000002L);
  full.set_i64(-40000000003L);
  full.set_sf64(-40000000004L);
  full.set_s64(-40000000005L);
  full.set_str("string1");
  full.set_bytes("Some Bytes");
  full.set_float_(3.14f);
  full.set_double_(-4.05L);
  full.set_bool_(true);
  full.add_rep_fix32(201);
  full.add_rep_u32(202);
  full.add_rep_i32(-203);
  full.add_rep_sf32(-204);
  full.add_rep_s32(-205);
  full.add_rep_fix64(80000000001L);
  full.add_rep_u64(80000000002L);
  full.add_rep_i64(-80000000003L);
  full.add_rep_sf64(-80000000004L);
  full.add_rep_s64(-80000000005L);
  full.add_rep_str("string2");
  full.add_rep_bytes("More Bytes");
  full.add_rep_float(6.14f);
  full.add_rep_double(-8.05L);
  full.add_rep_bool(false);

  ResetTypeInfo(Primitive::descriptor());

  ow_->StartObject("")
      ->RenderString("fix32", "101")
      ->RenderString("u32", "102")
      ->RenderString("i32", "-103")
      ->RenderString("sf32", "-104")
      ->RenderString("s32", "-105")
      ->RenderString("fix64", "40000000001")
      ->RenderString("u64", "40000000002")
      ->RenderString("i64", "-40000000003")
      ->RenderString("sf64", "-40000000004")
      ->RenderString("s64", "-40000000005")
      ->RenderString("str", "string1")
      ->RenderString("bytes", "U29tZSBCeXRlcw==")  // "Some Bytes"
      ->RenderString("float", "3.14")
      ->RenderString("double", "-4.05")
      ->RenderString("bool", "true")
      ->StartList("rep_fix32")
      ->RenderString("", "201")
      ->EndList()
      ->StartList("rep_u32")
      ->RenderString("", "202")
      ->EndList()
      ->StartList("rep_i32")
      ->RenderString("", "-203")
      ->EndList()
      ->StartList("rep_sf32")
      ->RenderString("", "-204")
      ->EndList()
      ->StartList("rep_s32")
      ->RenderString("", "-205")
      ->EndList()
      ->StartList("rep_fix64")
      ->RenderString("", "80000000001")
      ->EndList()
      ->StartList("rep_u64")
      ->RenderString("", "80000000002")
      ->EndList()
      ->StartList("rep_i64")
      ->RenderString("", "-80000000003")
      ->EndList()
      ->StartList("rep_sf64")
      ->RenderString("", "-80000000004")
      ->EndList()
      ->StartList("rep_s64")
      ->RenderString("", "-80000000005")
      ->EndList()
      ->StartList("rep_str")
      ->RenderString("", "string2")
      ->EndList()
      ->StartList("rep_bytes")
      ->RenderString("", "TW9yZSBCeXRlcw==")  // "More Bytes"
      ->EndList()
      ->StartList("rep_float")
      ->RenderString("", "6.14")
      ->EndList()
      ->StartList("rep_double")
      ->RenderString("", "-8.05")
      ->EndList()
      ->StartList("rep_bool")
      ->RenderString("", "false")
      ->EndList()
      ->EndObject();
  CheckOutput(full);
}

TEST_P(ProtoStreamObjectWriterTest, InfinityInputTest) {
  Primitive full;
  full.set_double_(std::numeric_limits<double>::infinity());
  full.set_float_(std::numeric_limits<float>::infinity());
  full.set_str("-Infinity");

  ResetTypeInfo(Primitive::descriptor());

  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("TYPE_INT32"),
                                      StringPiece("\"Infinity\"")))
      .With(Args<0>(HasObjectLocation("i32")));
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("TYPE_UINT32"),
                                      StringPiece("\"Infinity\"")))
      .With(Args<0>(HasObjectLocation("u32")));
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("TYPE_SFIXED64"),
                                      StringPiece("\"-Infinity\"")))
      .With(Args<0>(HasObjectLocation("sf64")));
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("TYPE_BOOL"),
                                      StringPiece("\"Infinity\"")))
      .With(Args<0>(HasObjectLocation("bool")));

  ow_->StartObject("")
      ->RenderString("double", "Infinity")
      ->RenderString("float", "Infinity")
      ->RenderString("i32", "Infinity")
      ->RenderString("u32", "Infinity")
      ->RenderString("sf64", "-Infinity")
      ->RenderString("str", "-Infinity")
      ->RenderString("bool", "Infinity")
      ->EndObject();
  CheckOutput(full);
}

TEST_P(ProtoStreamObjectWriterTest, NaNInputTest) {
  Primitive full;
  full.set_double_(std::numeric_limits<double>::quiet_NaN());
  full.set_float_(std::numeric_limits<float>::quiet_NaN());
  full.set_str("NaN");

  ResetTypeInfo(Primitive::descriptor());

  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("TYPE_INT32"),
                                      StringPiece("\"NaN\"")))
      .With(Args<0>(HasObjectLocation("i32")));
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("TYPE_UINT32"),
                                      StringPiece("\"NaN\"")))
      .With(Args<0>(HasObjectLocation("u32")));
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("TYPE_SFIXED64"),
                                      StringPiece("\"NaN\"")))
      .With(Args<0>(HasObjectLocation("sf64")));
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("TYPE_BOOL"),
                                      StringPiece("\"NaN\"")))
      .With(Args<0>(HasObjectLocation("bool")));

  ow_->StartObject("")
      ->RenderString("double", "NaN")
      ->RenderString("float", "NaN")
      ->RenderString("i32", "NaN")
      ->RenderString("u32", "NaN")
      ->RenderString("sf64", "NaN")
      ->RenderString("str", "NaN")
      ->RenderString("bool", "NaN")
      ->EndObject();

  CheckOutput(full);
}

TEST_P(ProtoStreamObjectWriterTest, ImplicitPrimitiveList) {
  Book expected;
  Author* author = expected.mutable_author();
  author->set_name("The Author");
  author->add_pseudonym("first");
  author->add_pseudonym("second");

  ow_->StartObject("")
      ->StartObject("author")
      ->RenderString("name", "The Author")
      ->RenderString("pseudonym", "first")
      ->RenderString("pseudonym", "second")
      ->EndObject()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest,
       LastWriteWinsOnNonRepeatedPrimitiveFieldWithDuplicates) {
  Book expected;
  Author* author = expected.mutable_author();
  author->set_name("second");

  ow_->StartObject("")
      ->StartObject("author")
      ->RenderString("name", "first")
      ->RenderString("name", "second")
      ->EndObject()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, ExplicitPrimitiveList) {
  Book expected;
  Author* author = expected.mutable_author();
  author->set_name("The Author");
  author->add_pseudonym("first");
  author->add_pseudonym("second");

  ow_->StartObject("")
      ->StartObject("author")
      ->RenderString("name", "The Author")
      ->StartList("pseudonym")
      ->RenderString("", "first")
      ->RenderString("", "second")
      ->EndList()
      ->EndObject()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, NonRepeatedExplicitPrimitiveList) {
  Book expected;
  expected.set_allocated_author(new Author());

  EXPECT_CALL(
      listener_,
      InvalidName(_, StringPiece("name"),
                  StringPiece(
                      "Proto field is not repeating, cannot start list.")))
      .With(Args<0>(HasObjectLocation("author")));
  ow_->StartObject("")
      ->StartObject("author")
      ->StartList("name")
      ->RenderString("", "first")
      ->RenderString("", "second")
      ->EndList()
      ->EndObject()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, ImplicitMessageList) {
  Book expected;
  Author* outer = expected.mutable_author();
  outer->set_name("outer");
  outer->set_alive(true);
  Author* first = outer->add_friend_();
  first->set_name("first");
  Author* second = outer->add_friend_();
  second->set_name("second");

  ow_->StartObject("")
      ->StartObject("author")
      ->RenderString("name", "outer")
      ->RenderBool("alive", true)
      ->StartObject("friend")
      ->RenderString("name", "first")
      ->EndObject()
      ->StartObject("friend")
      ->RenderString("name", "second")
      ->EndObject()
      ->EndObject()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest,
       LastWriteWinsOnNonRepeatedMessageFieldWithDuplicates) {
  Book expected;
  Author* author = expected.mutable_author();
  author->set_name("The Author");
  Publisher* publisher = expected.mutable_publisher();
  publisher->set_name("second");

  ow_->StartObject("")
      ->StartObject("author")
      ->RenderString("name", "The Author")
      ->EndObject()
      ->StartObject("publisher")
      ->RenderString("name", "first")
      ->EndObject()
      ->StartObject("publisher")
      ->RenderString("name", "second")
      ->EndObject()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, ExplicitMessageList) {
  Book expected;
  Author* outer = expected.mutable_author();
  outer->set_name("outer");
  outer->set_alive(true);
  Author* first = outer->add_friend_();
  first->set_name("first");
  Author* second = outer->add_friend_();
  second->set_name("second");

  ow_->StartObject("")
      ->StartObject("author")
      ->RenderString("name", "outer")
      ->RenderBool("alive", true)
      ->StartList("friend")
      ->StartObject("")
      ->RenderString("name", "first")
      ->EndObject()
      ->StartObject("")
      ->RenderString("name", "second")
      ->EndObject()
      ->EndList()
      ->EndObject()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, NonRepeatedExplicitMessageList) {
  Book expected;
  Author* author = expected.mutable_author();
  author->set_name("The Author");

  EXPECT_CALL(
      listener_,
      InvalidName(_, StringPiece("publisher"),
                  StringPiece(
                      "Proto field is not repeating, cannot start list.")))
      .With(Args<0>(HasObjectLocation("")));
  ow_->StartObject("")
      ->StartObject("author")
      ->RenderString("name", "The Author")
      ->EndObject()
      ->StartList("publisher")
      ->StartObject("")
      ->RenderString("name", "first")
      ->EndObject()
      ->StartObject("")
      ->RenderString("name", "second")
      ->EndObject()
      ->EndList()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, UnknownFieldAtRoot) {
  Book empty;

  EXPECT_CALL(listener_, InvalidName(_, StringPiece("unknown"),
                                     StringPiece("Cannot find field.")))
      .With(Args<0>(HasObjectLocation("")));
  ow_->StartObject("")->RenderString("unknown", "Nope!")->EndObject();
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, UnknownFieldAtAuthorFriend) {
  Book expected;
  Author* paul = expected.mutable_author();
  paul->set_name("Paul");
  Author* mark = paul->add_friend_();
  mark->set_name("Mark");
  Author* john = paul->add_friend_();
  john->set_name("John");
  Author* luke = paul->add_friend_();
  luke->set_name("Luke");

  EXPECT_CALL(listener_, InvalidName(_, StringPiece("address"),
                                     StringPiece("Cannot find field.")))
      .With(Args<0>(HasObjectLocation("author.friend[1]")));
  ow_->StartObject("")
      ->StartObject("author")
      ->RenderString("name", "Paul")
      ->StartList("friend")
      ->StartObject("")
      ->RenderString("name", "Mark")
      ->EndObject()
      ->StartObject("")
      ->RenderString("name", "John")
      ->RenderString("address", "Patmos")
      ->EndObject()
      ->StartObject("")
      ->RenderString("name", "Luke")
      ->EndObject()
      ->EndList()
      ->EndObject()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, UnknownObjectAtRoot) {
  Book empty;

  EXPECT_CALL(listener_, InvalidName(_, StringPiece("unknown"),
                                     StringPiece("Cannot find field.")))
      .With(Args<0>(HasObjectLocation("")));
  ow_->StartObject("")->StartObject("unknown")->EndObject()->EndObject();
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, UnknownObjectAtAuthor) {
  Book expected;
  Author* author = expected.mutable_author();
  author->set_name("William");
  author->add_pseudonym("Bill");

  EXPECT_CALL(listener_, InvalidName(_, StringPiece("wife"),
                                     StringPiece("Cannot find field.")))
      .With(Args<0>(HasObjectLocation("author")));
  ow_->StartObject("")
      ->StartObject("author")
      ->RenderString("name", "William")
      ->StartObject("wife")
      ->RenderString("name", "Hilary")
      ->EndObject()
      ->RenderString("pseudonym", "Bill")
      ->EndObject()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, UnknownListAtRoot) {
  Book empty;

  EXPECT_CALL(listener_, InvalidName(_, StringPiece("unknown"),
                                     StringPiece("Cannot find field.")))
      .With(Args<0>(HasObjectLocation("")));
  ow_->StartObject("")->StartList("unknown")->EndList()->EndObject();
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, UnknownListAtPublisher) {
  Book expected;
  expected.set_title("Brainwashing");
  Publisher* publisher = expected.mutable_publisher();
  publisher->set_name("propaganda");

  EXPECT_CALL(listener_, InvalidName(_, StringPiece("alliance"),
                                     StringPiece("Cannot find field.")))
      .With(Args<0>(HasObjectLocation("publisher")));
  ow_->StartObject("")
      ->StartObject("publisher")
      ->RenderString("name", "propaganda")
      ->StartList("alliance")
      ->EndList()
      ->EndObject()
      ->RenderString("title", "Brainwashing")
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, IgnoreUnknownFieldAtRoot) {
  Book empty;

  options_.ignore_unknown_fields = true;
  ResetProtoWriter();

  EXPECT_CALL(listener_, InvalidName(_, _, _)).Times(0);
  ow_->StartObject("")->RenderString("unknown", "Nope!")->EndObject();
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, IgnoreUnknownFieldAtAuthorFriend) {
  Book expected;
  Author* paul = expected.mutable_author();
  paul->set_name("Paul");
  Author* mark = paul->add_friend_();
  mark->set_name("Mark");
  Author* john = paul->add_friend_();
  john->set_name("John");
  Author* luke = paul->add_friend_();
  luke->set_name("Luke");

  options_.ignore_unknown_fields = true;
  ResetProtoWriter();

  EXPECT_CALL(listener_, InvalidName(_, _, _)).Times(0);
  ow_->StartObject("")
      ->StartObject("author")
      ->RenderString("name", "Paul")
      ->StartList("friend")
      ->StartObject("")
      ->RenderString("name", "Mark")
      ->EndObject()
      ->StartObject("")
      ->RenderString("name", "John")
      ->RenderString("address", "Patmos")
      ->EndObject()
      ->StartObject("")
      ->RenderString("name", "Luke")
      ->EndObject()
      ->EndList()
      ->EndObject()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, IgnoreUnknownObjectAtRoot) {
  Book empty;

  options_.ignore_unknown_fields = true;
  ResetProtoWriter();

  EXPECT_CALL(listener_, InvalidName(_, StringPiece("unknown"),
                                     StringPiece("Cannot find field.")))
      .Times(0);
  ow_->StartObject("")->StartObject("unknown")->EndObject()->EndObject();
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, IgnoreUnknownObjectAtAuthor) {
  Book expected;
  Author* author = expected.mutable_author();
  author->set_name("William");
  author->add_pseudonym("Bill");

  options_.ignore_unknown_fields = true;
  ResetProtoWriter();

  EXPECT_CALL(listener_, InvalidName(_, _, _)).Times(0);
  ow_->StartObject("")
      ->StartObject("author")
      ->RenderString("name", "William")
      ->StartObject("wife")
      ->RenderString("name", "Hilary")
      ->EndObject()
      ->RenderString("pseudonym", "Bill")
      ->EndObject()
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, IgnoreUnknownListAtRoot) {
  Book empty;

  options_.ignore_unknown_fields = true;
  ResetProtoWriter();

  EXPECT_CALL(listener_, InvalidName(_, _, _)).Times(0);
  ow_->StartObject("")->StartList("unknown")->EndList()->EndObject();
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, IgnoreUnknownListAtPublisher) {
  Book expected;
  expected.set_title("Brainwashing");
  Publisher* publisher = expected.mutable_publisher();
  publisher->set_name("propaganda");

  options_.ignore_unknown_fields = true;
  ResetProtoWriter();

  EXPECT_CALL(listener_, InvalidName(_, _, _)).Times(0);
  ow_->StartObject("")
      ->StartObject("publisher")
      ->RenderString("name", "propaganda")
      ->StartList("alliance")
      ->EndList()
      ->EndObject()
      ->RenderString("title", "Brainwashing")
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest,
       IgnoreUnknownFieldsDontIgnoreUnknownEnumValues) {
  ResetTypeInfo(Proto3Message::descriptor());

  Proto3Message expected;
  EXPECT_CALL(
      listener_,
      InvalidValue(_, StringPiece("TYPE_ENUM"),
                   StringPiece("\"someunknownvalueyouwillneverknow\"")))
      .With(Args<0>(HasObjectLocation("enum_value")));
  ow_->StartObject("")
      ->RenderString("enumValue", "someunknownvalueyouwillneverknow")
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, AcceptUnknownEnumValue) {
  ResetTypeInfo(Proto3Message::descriptor());

  Proto3Message expected;
  expected.set_enum_value(static_cast<Proto3Message::NestedEnum>(12345));

  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);
  ow_->StartObject("")->RenderInt32("enumValue", 12345)->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, MissingRequiredField) {
  Book expected;
  expected.set_title("My Title");
  expected.set_allocated_publisher(new Publisher());

  EXPECT_CALL(listener_, MissingField(_, StringPiece("name")))
      .With(Args<0>(HasObjectLocation("publisher")));
  ow_->StartObject("")
      ->StartObject("publisher")
      ->EndObject()
      ->RenderString("title", "My Title")
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, InvalidFieldValueAtRoot) {
  Book empty;

  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("TYPE_UINT32"),
                                      StringPiece("\"garbage\"")))
      .With(Args<0>(HasObjectLocation("length")));
  ow_->StartObject("")->RenderString("length", "garbage")->EndObject();
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, MultipleInvalidFieldValues) {
  Book expected;
  expected.set_title("My Title");

  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("TYPE_UINT32"),
                                      StringPiece("\"-400\"")))
      .With(Args<0>(HasObjectLocation("length")));
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("TYPE_INT64"),
                                      StringPiece("\"3.14\"")))
      .With(Args<0>(HasObjectLocation("published")));
  ow_->StartObject("")
      ->RenderString("length", "-400")
      ->RenderString("published", "3.14")
      ->RenderString("title", "My Title")
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, UnnamedFieldAtRoot) {
  Book empty;

  EXPECT_CALL(listener_,
              InvalidName(_, StringPiece(""),
                          StringPiece("Proto fields must have a name.")))
      .With(Args<0>(HasObjectLocation("")));
  ow_->StartObject("")->RenderFloat("", 3.14)->EndObject();
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, UnnamedFieldAtAuthor) {
  Book expected;
  expected.set_title("noname");
  expected.set_allocated_author(new Author());

  EXPECT_CALL(listener_,
              InvalidName(_, StringPiece(""),
                          StringPiece("Proto fields must have a name.")))
      .With(Args<0>(HasObjectLocation("author")));
  ow_->StartObject("")
      ->StartObject("author")
      ->RenderInt32("", 123)
      ->EndObject()
      ->RenderString("title", "noname")
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, UnnamedListAtRoot) {
  Book expected;
  expected.set_title("noname");

  EXPECT_CALL(listener_,
              InvalidName(_, StringPiece(""),
                          StringPiece("Proto fields must have a name.")))
      .With(Args<0>(HasObjectLocation("")));
  ow_->StartObject("")
      ->StartList("")
      ->EndList()
      ->RenderString("title", "noname")
      ->EndObject();
  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterTest, RootNamedObject) {
  Book expected;
  expected.set_title("Annie");

  EXPECT_CALL(
      listener_,
      InvalidName(_, StringPiece("oops"),
                  StringPiece("Root element should not be named.")))
      .With(Args<0>(HasObjectLocation("")));
  ow_->StartObject("oops")->RenderString("title", "Annie")->EndObject();
  CheckOutput(expected, 7);
}

TEST_P(ProtoStreamObjectWriterTest, RootNamedList) {
  Book empty;

  EXPECT_CALL(
      listener_,
      InvalidName(_, StringPiece("oops"),
                  StringPiece("Root element should not be named.")))
      .With(Args<0>(HasObjectLocation("")));
  ow_->StartList("oops")->RenderString("", "item")->EndList();
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, RootUnnamedField) {
  Book empty;

  EXPECT_CALL(listener_,
              InvalidName(_, StringPiece(""),
                          StringPiece("Root element must be a message.")))
      .With(Args<0>(HasObjectLocation("")));
  ow_->RenderBool("", true);
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, RootNamedField) {
  Book empty;

  EXPECT_CALL(listener_,
              InvalidName(_, StringPiece("oops"),
                          StringPiece("Root element must be a message.")))
      .With(Args<0>(HasObjectLocation("")));
  ow_->RenderBool("oops", true);
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, NullValue) {
  Book empty;

  ow_->RenderNull("");
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, NullValueForMessageField) {
  Book empty;

  ow_->RenderNull("author");
  CheckOutput(empty, 0);
}

TEST_P(ProtoStreamObjectWriterTest, NullValueForPrimitiveField) {
  Book empty;

  ow_->RenderNull("length");
  CheckOutput(empty, 0);
}

class ProtoStreamObjectWriterTimestampDurationTest
    : public BaseProtoStreamObjectWriterTest {
 protected:
  ProtoStreamObjectWriterTimestampDurationTest() {
    std::vector<const Descriptor*> descriptors;
    descriptors.push_back(TimestampDuration::descriptor());
    descriptors.push_back(google::protobuf::Timestamp::descriptor());
    descriptors.push_back(google::protobuf::Duration::descriptor());
    ResetTypeInfo(descriptors);
  }
};

INSTANTIATE_TEST_SUITE_P(DifferentTypeInfoSourceTest,
                         ProtoStreamObjectWriterTimestampDurationTest,
                         ::testing::Values(
                             testing::USE_TYPE_RESOLVER));

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, ParseTimestamp) {
  TimestampDuration timestamp;
  google::protobuf::Timestamp* ts = timestamp.mutable_ts();
  ts->set_seconds(1448249855);
  ts->set_nanos(33155000);

  ow_->StartObject("")
      ->RenderString("ts", "2015-11-23T03:37:35.033155Z")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest,
       ParseTimestampYearNotZeroPadded) {
  TimestampDuration timestamp;
  google::protobuf::Timestamp* ts = timestamp.mutable_ts();
  ts->set_seconds(-61665654145);
  ts->set_nanos(33155000);

  ow_->StartObject("")
      ->RenderString("ts", "15-11-23T03:37:35.033155Z")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest,
       ParseTimestampYearZeroPadded) {
  TimestampDuration timestamp;
  google::protobuf::Timestamp* ts = timestamp.mutable_ts();
  ts->set_seconds(-61665654145);
  ts->set_nanos(33155000);

  ow_->StartObject("")
      ->RenderString("ts", "0015-11-23T03:37:35.033155Z")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest,
       ParseTimestampWithPositiveOffset) {
  TimestampDuration timestamp;
  google::protobuf::Timestamp* ts = timestamp.mutable_ts();
  ts->set_seconds(1448249855);
  ts->set_nanos(33155000);

  ow_->StartObject("")
      ->RenderString("ts", "2015-11-23T11:47:35.033155+08:10")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest,
       ParseTimestampWithNegativeOffset) {
  TimestampDuration timestamp;
  google::protobuf::Timestamp* ts = timestamp.mutable_ts();
  ts->set_seconds(1448249855);
  ts->set_nanos(33155000);

  ow_->StartObject("")
      ->RenderString("ts", "2015-11-22T19:47:35.033155-07:50")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest,
       TimestampWithInvalidOffset1) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: "
                      "2016-03-07T15:14:23+")));

  ow_->StartObject("")->RenderString("ts", "2016-03-07T15:14:23+")->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest,
       TimestampWithInvalidOffset2) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: "
                      "2016-03-07T15:14:23+08-10")));

  ow_->StartObject("")
      ->RenderString("ts", "2016-03-07T15:14:23+08-10")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest,
       TimestampWithInvalidOffset3) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: "
                      "2016-03-07T15:14:23+24:10")));

  ow_->StartObject("")
      ->RenderString("ts", "2016-03-07T15:14:23+24:10")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest,
       TimestampWithInvalidOffset4) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: "
                      "2016-03-07T15:14:23+04:60")));

  ow_->StartObject("")
      ->RenderString("ts", "2016-03-07T15:14:23+04:60")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidTimestampError1) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: ")));

  ow_->StartObject("")->RenderString("ts", "")->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidTimestampError2) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: Z")));

  ow_->StartObject("")->RenderString("ts", "Z")->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidTimestampError3) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: "
                      "1970-01-01T00:00:00.ABZ")));

  ow_->StartObject("")
      ->RenderString("ts", "1970-01-01T00:00:00.ABZ")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidTimestampError4) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: "
                      "-8031-10-18T00:00:00.000Z")));

  ow_->StartObject("")
      ->RenderString("ts", "-8031-10-18T00:00:00.000Z")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidTimestampError5) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: "
                      "2015-11-23T03:37:35.033155   Z")));

  ow_->StartObject("")
      // Whitespace in the Timestamp nanos is not allowed.
      ->RenderString("ts", "2015-11-23T03:37:35.033155   Z")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidTimestampError6) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: "
                      "2015-11-23T03:37:35.033155 1234Z")));

  ow_->StartObject("")
      // Whitespace in the Timestamp nanos is not allowed.
      ->RenderString("ts", "2015-11-23T03:37:35.033155 1234Z")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidTimestampError7) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: "
                      "2015-11-23T03:37:35.033abc155Z")));

  ow_->StartObject("")
      // Non-numeric characters in the Timestamp nanos is not allowed.
      ->RenderString("ts", "2015-11-23T03:37:35.033abc155Z")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidTimestampError8) {
  TimestampDuration timestamp;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece("Field 'ts', Invalid time format: "
                      "0-12-31T23:59:59.000Z")));

  ow_->StartObject("")
      ->RenderString("ts", "0-12-31T23:59:59.000Z")
      ->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, ParseDuration) {
  TimestampDuration duration;
  google::protobuf::Duration* dur = duration.mutable_dur();
  dur->set_seconds(1448216930);
  dur->set_nanos(132262000);

  ow_->StartObject("")->RenderString("dur", "1448216930.132262s")->EndObject();
  CheckOutput(duration);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidDurationError1) {
  TimestampDuration duration;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Duration"),
          StringPiece(
              "Field 'dur', Illegal duration format; duration must "
              "end with 's'")));

  ow_->StartObject("")->RenderString("dur", "")->EndObject();
  CheckOutput(duration);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidDurationError2) {
  TimestampDuration duration;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Duration"),
          StringPiece(
              "Field 'dur', Invalid duration format, failed to parse "
              "seconds")));

  ow_->StartObject("")->RenderString("dur", "s")->EndObject();
  CheckOutput(duration);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidDurationError3) {
  TimestampDuration duration;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Duration"),
          StringPiece("Field 'dur', Invalid duration format, failed to "
                            "parse nano seconds")));

  ow_->StartObject("")->RenderString("dur", "123.DEFs")->EndObject();
  CheckOutput(duration);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidDurationError4) {
  TimestampDuration duration;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Duration"),
          StringPiece("Field 'dur', Duration value exceeds limits")));

  ow_->StartObject("")->RenderString("dur", "315576000002s")->EndObject();
  CheckOutput(duration);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, InvalidDurationError5) {
  TimestampDuration duration;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Duration"),
          StringPiece("Field 'dur', Duration value exceeds limits")));

  ow_->StartObject("")->RenderString("dur", "0.1000000001s")->EndObject();
  CheckOutput(duration);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest,
       MismatchedTimestampTypeInput) {
  TimestampDuration timestamp;
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Timestamp"),
          StringPiece(
              "Field 'ts', Invalid data type for timestamp, value is 1")))
      .With(Args<0>(HasObjectLocation("ts")));
  ow_->StartObject("")->RenderInt32("ts", 1)->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest,
       MismatchedDurationTypeInput) {
  TimestampDuration duration;
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Duration"),
          StringPiece(
              "Field 'dur', Invalid data type for duration, value is 1")))
      .With(Args<0>(HasObjectLocation("dur")));
  ow_->StartObject("")->RenderInt32("dur", 1)->EndObject();
  CheckOutput(duration);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, TimestampAcceptsNull) {
  TimestampDuration timestamp;
  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);
  ow_->StartObject("")->RenderNull("ts")->EndObject();
  CheckOutput(timestamp);
}

TEST_P(ProtoStreamObjectWriterTimestampDurationTest, DurationAcceptsNull) {
  TimestampDuration duration;
  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);
  ow_->StartObject("")->RenderNull("dur")->EndObject();
  CheckOutput(duration);
}

class ProtoStreamObjectWriterStructTest
    : public BaseProtoStreamObjectWriterTest {
 protected:
  ProtoStreamObjectWriterStructTest() { ResetProtoWriter(); }

  // Resets ProtoWriter with current set of options and other state.
  void ResetProtoWriter() {
    std::vector<const Descriptor*> descriptors;
    descriptors.push_back(StructType::descriptor());
    descriptors.push_back(google::protobuf::Struct::descriptor());
    ResetTypeInfo(descriptors);
  }
};

INSTANTIATE_TEST_SUITE_P(DifferentTypeInfoSourceTest,
                         ProtoStreamObjectWriterStructTest,
                         ::testing::Values(
                             testing::USE_TYPE_RESOLVER));

// TODO(skarvaje): Write tests for failure cases.
TEST_P(ProtoStreamObjectWriterStructTest, StructRenderSuccess) {
  StructType struct_type;
  google::protobuf::Struct* s = struct_type.mutable_object();
  s->mutable_fields()->operator[]("k1").set_number_value(123);
  s->mutable_fields()->operator[]("k2").set_bool_value(true);

  ow_->StartObject("")
      ->StartObject("object")
      ->RenderDouble("k1", 123)
      ->RenderBool("k2", true)
      ->EndObject()
      ->EndObject();
  CheckOutput(struct_type);
}

TEST_P(ProtoStreamObjectWriterStructTest, StructNullInputSuccess) {
  StructType struct_type;
  EXPECT_CALL(listener_,
              InvalidName(_, StringPiece(""),
                          StringPiece("Proto fields must have a name.")))
      .With(Args<0>(HasObjectLocation("")));
  ow_->StartObject("")->RenderNull("")->EndObject();
  CheckOutput(struct_type);
}

TEST_P(ProtoStreamObjectWriterStructTest, StructInvalidInputFailure) {
  StructType struct_type;
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.Struct"),
          StringPiece("true")))
      .With(Args<0>(HasObjectLocation("object")));

  ow_->StartObject("")->RenderBool("object", true)->EndObject();
  CheckOutput(struct_type);
}

TEST_P(ProtoStreamObjectWriterStructTest, StructAcceptsNull) {
  StructType struct_type;
  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);

  ow_->StartObject("")->RenderNull("object")->EndObject();
  CheckOutput(struct_type);
}

TEST_P(ProtoStreamObjectWriterStructTest, StructValuePreservesNull) {
  StructType struct_type;
  (*struct_type.mutable_object()->mutable_fields())["key"].set_null_value(
      google::protobuf::NULL_VALUE);
  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);

  ow_->StartObject("")
      ->StartObject("object")
      ->RenderNull("key")
      ->EndObject()
      ->EndObject();
  CheckOutput(struct_type);
}

TEST_P(ProtoStreamObjectWriterStructTest, SimpleRepeatedStructMapKeyTest) {
  EXPECT_CALL(listener_,
              InvalidName(_, StringPiece("gBike"),
                          StringPiece(
                              "Repeated map key: 'gBike' is already set.")));
  ow_->StartObject("")
      ->StartObject("object")
      ->RenderString("gBike", "v1")
      ->RenderString("gBike", "v2")
      ->EndObject()
      ->EndObject();
}

TEST_P(ProtoStreamObjectWriterStructTest, RepeatedStructMapListKeyTest) {
  EXPECT_CALL(
      listener_,
      InvalidName(_, StringPiece("k1"),
                  StringPiece("Repeated map key: 'k1' is already set.")));
  ow_->StartObject("")
      ->StartObject("object")
      ->RenderString("k1", "v1")
      ->StartList("k1")
      ->RenderString("", "v2")
      ->EndList()
      ->EndObject()
      ->EndObject();
}

TEST_P(ProtoStreamObjectWriterStructTest, RepeatedStructMapObjectKeyTest) {
  EXPECT_CALL(
      listener_,
      InvalidName(_, StringPiece("k1"),
                  StringPiece("Repeated map key: 'k1' is already set.")));
  ow_->StartObject("")
      ->StartObject("object")
      ->StartObject("k1")
      ->RenderString("sub_k1", "v1")
      ->EndObject()
      ->StartObject("k1")
      ->RenderString("sub_k2", "v2")
      ->EndObject()
      ->EndObject()
      ->EndObject();
}

TEST_P(ProtoStreamObjectWriterStructTest, OptionStructIntAsStringsTest) {
  StructType struct_type;
  google::protobuf::Struct* s = struct_type.mutable_object();
  s->mutable_fields()->operator[]("k1").set_string_value("123");
  s->mutable_fields()->operator[]("k2").set_bool_value(true);
  s->mutable_fields()->operator[]("k3").set_string_value("-222222222");
  s->mutable_fields()->operator[]("k4").set_string_value("33333333");

  options_.struct_integers_as_strings = true;
  ResetProtoWriter();

  ow_->StartObject("")
      ->StartObject("object")
      ->RenderDouble("k1", 123)
      ->RenderBool("k2", true)
      ->RenderInt64("k3", -222222222)
      ->RenderUint64("k4", 33333333)
      ->EndObject()
      ->EndObject();
  CheckOutput(struct_type);
}

TEST_P(ProtoStreamObjectWriterStructTest, ValuePreservesNull) {
  ValueWrapper value;
  value.mutable_value()->set_null_value(google::protobuf::NULL_VALUE);

  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);
  ow_->StartObject("")->RenderNull("value")->EndObject();
  CheckOutput(value);
}

class ProtoStreamObjectWriterMapTest : public BaseProtoStreamObjectWriterTest {
 protected:
  ProtoStreamObjectWriterMapTest() {
    std::vector<const Descriptor*> descriptors;
    descriptors.push_back(MapIn::descriptor());
    descriptors.push_back(google::protobuf::DoubleValue::descriptor());
    ResetTypeInfo(descriptors);
  }
};

INSTANTIATE_TEST_SUITE_P(DifferentTypeInfoSourceTest,
                         ProtoStreamObjectWriterMapTest,
                         ::testing::Values(
                             testing::USE_TYPE_RESOLVER));

TEST_P(ProtoStreamObjectWriterMapTest, MapShouldNotAcceptList) {
  MapIn mm;
  EXPECT_CALL(
      listener_,
      InvalidValue(_, StringPiece("Map"),
                   StringPiece(
                       "Cannot bind a list to map for field 'map_input'.")));
  ow_->StartObject("")
      ->StartList("map_input")
      ->RenderString("a", "b")
      ->EndList()
      ->EndObject();
  CheckOutput(mm);
}

TEST_P(ProtoStreamObjectWriterMapTest, MapAcceptsNullValue) {
  // Null should not be a valid map value.
  // See http://go/proto3-json-spec#heading=h.r2ddatp7y4vi
  // This test is added for backward compatibility.
  MapIn mm;
  (*mm.mutable_map_input())["a"] = "b";
  (*mm.mutable_map_input())["x"] = "";
  ow_->StartObject("")
      ->StartObject("map_input")
      ->RenderString("a", "b")
      ->RenderNull("x")
      ->EndObject()
      ->EndObject();
  CheckOutput(mm);
}

TEST_P(ProtoStreamObjectWriterMapTest, MapShouldIgnoreNullValueEntry) {
  options_.ignore_null_value_map_entry = true;
  ResetTypeInfo(MapIn::descriptor());
  MapIn mm;
  (*mm.mutable_map_input())["a"] = "b";
  ow_->StartObject("")
      ->StartObject("map_input")
      ->RenderString("a", "b")
      ->RenderNull("x")
      ->EndObject()
      ->EndObject();
  CheckOutput(mm);
}

TEST_P(ProtoStreamObjectWriterMapTest, RepeatedMapKeyTest) {
  EXPECT_CALL(
      listener_,
      InvalidName(_, StringPiece("k1"),
                  StringPiece("Repeated map key: 'k1' is already set.")));
  ow_->StartObject("")
      ->RenderString("other", "test")
      ->StartObject("map_input")
      ->RenderString("k1", "v1")
      ->RenderString("k1", "v2")
      ->EndObject()
      ->EndObject();
}

TEST_P(ProtoStreamObjectWriterMapTest, AnyInMap) {
  MapIn mm;
  google::protobuf::DoubleValue d;
  d.set_value(40.2);
  (*mm.mutable_map_any())["foo"].PackFrom(d);
  ow_->StartObject("")
      ->StartObject("map_any")
      ->StartObject("foo")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.DoubleValue")
      ->RenderDouble("value", 40.2)
      ->EndObject()
      ->EndObject()
      ->EndObject();
  CheckOutput(mm);
}

class ProtoStreamObjectWriterAnyTest : public BaseProtoStreamObjectWriterTest {
 protected:
  ProtoStreamObjectWriterAnyTest() {
    std::vector<const Descriptor*> descriptors;
    descriptors.push_back(AnyOut::descriptor());
    descriptors.push_back(Book::descriptor());
    descriptors.push_back(google::protobuf::Any::descriptor());
    descriptors.push_back(google::protobuf::DoubleValue::descriptor());
    descriptors.push_back(google::protobuf::FieldMask::descriptor());
    descriptors.push_back(google::protobuf::Int32Value::descriptor());
    descriptors.push_back(google::protobuf::Struct::descriptor());
    descriptors.push_back(google::protobuf::Timestamp::descriptor());
    descriptors.push_back(google::protobuf::Value::descriptor());
    ResetTypeInfo(descriptors);
  }
};

INSTANTIATE_TEST_SUITE_P(DifferentTypeInfoSourceTest,
                         ProtoStreamObjectWriterAnyTest,
                         ::testing::Values(
                             testing::USE_TYPE_RESOLVER));

TEST_P(ProtoStreamObjectWriterAnyTest, AnyRenderSuccess) {
  AnyOut any;
  google::protobuf::Any* any_type = any.mutable_any();
  any_type->set_type_url("type.googleapis.com/google.protobuf.DoubleValue");
  google::protobuf::DoubleValue d;
  d.set_value(40.2);
  any_type->set_value(d.SerializeAsString());

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.DoubleValue")
      ->RenderDouble("value", 40.2)
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

TEST_P(ProtoStreamObjectWriterAnyTest, RecursiveAny) {
  AnyOut out;
  ::google::protobuf::Any* any = out.mutable_any();
  any->set_type_url("type.googleapis.com/google.protobuf.Any");

  ::google::protobuf::Any nested_any;
  nested_any.set_type_url(
      "type.googleapis.com/proto_util_converter.testing.AnyM");

  AnyM m;
  m.set_foo("foovalue");
  nested_any.set_value(m.SerializeAsString());

  any->set_value(nested_any.SerializeAsString());

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Any")
      ->StartObject("value")
      ->RenderString("@type",
                     "type.googleapis.com/proto_util_converter.testing.AnyM")
      ->RenderString("foo", "foovalue")
      ->EndObject()
      ->EndObject()
      ->EndObject();
  CheckOutput(out, 112);
}

TEST_P(ProtoStreamObjectWriterAnyTest, DoubleRecursiveAny) {
  AnyOut out;
  ::google::protobuf::Any* any = out.mutable_any();
  any->set_type_url("type.googleapis.com/google.protobuf.Any");

  ::google::protobuf::Any nested_any;
  nested_any.set_type_url("type.googleapis.com/google.protobuf.Any");

  ::google::protobuf::Any second_nested_any;
  second_nested_any.set_type_url(
      "type.googleapis.com/proto_util_converter.testing.AnyM");

  AnyM m;
  m.set_foo("foovalue");
  second_nested_any.set_value(m.SerializeAsString());

  nested_any.set_value(second_nested_any.SerializeAsString());
  any->set_value(nested_any.SerializeAsString());

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Any")
      ->StartObject("value")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Any")
      ->StartObject("value")
      ->RenderString("@type",
                     "type.googleapis.com/proto_util_converter.testing.AnyM")
      ->RenderString("foo", "foovalue")
      ->EndObject()
      ->EndObject()
      ->EndObject()
      ->EndObject();
  CheckOutput(out, 156);
}

TEST_P(ProtoStreamObjectWriterAnyTest, TypeUrlAtEnd) {
  Book book;
  book.set_title("C++");
  book.set_length(1234);
  book.set_content("Hello World!");

  ::google::protobuf::Any any;
  any.PackFrom(book);

  ::google::protobuf::Any outer_any;
  outer_any.PackFrom(any);

  AnyOut out;
  out.mutable_any()->PackFrom(outer_any);

  // Put the @type field at the end of each Any message. Parsers should
  // be able to accept that.
  ow_->StartObject("")
      ->StartObject("any")
      ->StartObject("value")
      ->StartObject("value")
      ->RenderString("title", "C++")
      ->RenderInt32("length", 1234)
      ->RenderBytes("content", "Hello World!")
      ->RenderString("@type",
                     "type.googleapis.com/proto_util_converter.testing.Book")
      ->EndObject()
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Any")
      ->EndObject()
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Any")
      ->EndObject()
      ->EndObject();
  CheckOutput(out);
}

// Same as TypeUrlAtEnd, but use temporary string values to make sure we don't
// mistakenly store StringPiece objects pointing to invalid memory.
TEST_P(ProtoStreamObjectWriterAnyTest, TypeUrlAtEndWithTemporaryStrings) {
  Book book;
  book.set_title("C++");
  book.set_length(1234);
  book.set_content("Hello World!");

  ::google::protobuf::Any any;
  any.PackFrom(book);

  ::google::protobuf::Any outer_any;
  outer_any.PackFrom(any);

  AnyOut out;
  out.mutable_any()->PackFrom(outer_any);

  std::string name, value;
  // Put the @type field at the end of each Any message. Parsers should
  // be able to accept that.
  ow_->StartObject("")->StartObject("any");
  {
    ow_->StartObject("value");
    {
      ow_->StartObject("value");
      {
        name = "title";
        value = "C++";
        ow_->RenderString(name, value);
        name = "length";
        ow_->RenderInt32(name, 1234);
        name = "content";
        value = "Hello World!";
        ow_->RenderBytes(name, value);
        name = "@type";
        value = "type.googleapis.com/proto_util_converter.testing.Book";
        ow_->RenderString(name, value);
      }
      ow_->EndObject();

      name = "@type";
      value = "type.googleapis.com/google.protobuf.Any";
      ow_->RenderString(name, value);
    }
    ow_->EndObject();

    name = "@type";
    value = "type.googleapis.com/google.protobuf.Any";
    ow_->RenderString(name, value);
  }
  ow_->EndObject()->EndObject();
  CheckOutput(out);
}

TEST_P(ProtoStreamObjectWriterAnyTest, EmptyAnyFromEmptyObject) {
  AnyOut out;
  out.mutable_any();

  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);

  ow_->StartObject("")->StartObject("any")->EndObject()->EndObject();

  CheckOutput(out, 2);
}

TEST_P(ProtoStreamObjectWriterAnyTest, AnyWithoutTypeUrlFails1) {
  AnyOut any;

  EXPECT_CALL(
      listener_,
      InvalidValue(_, StringPiece("Any"),
                   StringPiece("Missing @type for any field in "
                                     "proto_util_converter.testing.AnyOut")));

  ow_->StartObject("")
      ->StartObject("any")
      ->StartObject("another")
      ->EndObject()
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

TEST_P(ProtoStreamObjectWriterAnyTest, AnyWithoutTypeUrlFails2) {
  AnyOut any;

  EXPECT_CALL(
      listener_,
      InvalidValue(_, StringPiece("Any"),
                   StringPiece("Missing @type for any field in "
                                     "proto_util_converter.testing.AnyOut")));

  ow_->StartObject("")
      ->StartObject("any")
      ->StartList("another")
      ->EndObject()
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

TEST_P(ProtoStreamObjectWriterAnyTest, AnyWithoutTypeUrlFails3) {
  AnyOut any;

  EXPECT_CALL(
      listener_,
      InvalidValue(_, StringPiece("Any"),
                   StringPiece("Missing @type for any field in "
                                     "proto_util_converter.testing.AnyOut")));

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("value", "somevalue")
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

TEST_P(ProtoStreamObjectWriterAnyTest, AnyWithInvalidTypeUrlFails) {
  AnyOut any;

  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("Any"),
          StringPiece("Invalid type URL, type URLs must be of the form "
                            "'type.googleapis.com/<typename>', got: "
                            "type.other.com/some.Type")));

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.other.com/some.Type")
      ->RenderDouble("value", 40.2)
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

TEST_P(ProtoStreamObjectWriterAnyTest, AnyWithUnknownTypeFails) {
  AnyOut any;

  EXPECT_CALL(listener_,
              InvalidValue(_, StringPiece("Any"),
                           StringPiece(
                               "Invalid type URL, unknown type: some.Type")));
  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/some.Type")
      ->RenderDouble("value", 40.2)
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

TEST_P(ProtoStreamObjectWriterAnyTest, AnyIncorrectInputTypeFails) {
  AnyOut any;

  EXPECT_CALL(
      listener_,
      InvalidValue(_,
                   StringPiece("type.googleapis.com/google.protobuf.Any"),
                   StringPiece("1")));
  ow_->StartObject("")->RenderInt32("any", 1)->EndObject();
  CheckOutput(any);
}

TEST_P(ProtoStreamObjectWriterAnyTest, AnyAcceptsNull) {
  AnyOut any;

  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);
  ow_->StartObject("")->RenderNull("any")->EndObject();
  CheckOutput(any);
}

TEST_P(ProtoStreamObjectWriterAnyTest, AnyWellKnownTypeErrorTest) {
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("Any"),
                                      StringPiece("Invalid time format: ")));

  AnyOut any;
  google::protobuf::Any* any_type = any.mutable_any();
  any_type->set_type_url("type.googleapis.com/google.protobuf.Timestamp");

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Timestamp")
      ->RenderString("value", "")
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

// Test the following case:
//
// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Value",
//     "value": "abc"
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, AnyWithNestedPrimitiveValue) {
  AnyOut out;
  ::google::protobuf::Any* any = out.mutable_any();

  ::google::protobuf::Value value;
  value.set_string_value("abc");
  any->PackFrom(value);

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Value")
      ->RenderString("value", "abc")
      ->EndObject()
      ->EndObject();
  CheckOutput(out);
}

// Test the following case:
//
// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Value",
//     "value": {
//       "foo": "abc"
//     }
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, AnyWithNestedObjectValue) {
  AnyOut out;
  ::google::protobuf::Any* any = out.mutable_any();

  ::google::protobuf::Value value;
  (*value.mutable_struct_value()->mutable_fields())["foo"].set_string_value(
      "abc");
  any->PackFrom(value);

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Value")
      ->StartObject("value")
      ->RenderString("foo", "abc")
      ->EndObject()
      ->EndObject()
      ->EndObject();
  CheckOutput(out);
}

// Test the following case:
//
// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Value",
//     "value": ["hello"],
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, AnyWithNestedArrayValue) {
  AnyOut out;
  ::google::protobuf::Any* any = out.mutable_any();

  ::google::protobuf::Value value;
  value.mutable_list_value()->add_values()->set_string_value("hello");
  any->PackFrom(value);

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Value")
      ->StartList("value")
      ->RenderString("", "hello")
      ->EndList()
      ->EndObject()
      ->EndObject()
      ->EndObject();
  CheckOutput(out);
}

// Test the following case:
//
// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Value",
//     "not_value": ""
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest,
       AnyWellKnownTypesNoValueFieldForPrimitive) {
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("Any"),
          StringPiece("Expect a \"value\" field for well-known types.")));
  AnyOut any;
  google::protobuf::Any* any_type = any.mutable_any();
  any_type->set_type_url("type.googleapis.com/google.protobuf.Value");

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Value")
      ->RenderString("not_value", "")
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

// Test the following case:
//
// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Value",
//     "not_value": {}
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, AnyWellKnownTypesNoValueFieldForObject) {
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("Any"),
          StringPiece("Expect a \"value\" field for well-known types.")));
  AnyOut any;
  google::protobuf::Any* any_type = any.mutable_any();
  any_type->set_type_url("type.googleapis.com/google.protobuf.Value");

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Value")
      ->StartObject("not_value")
      ->EndObject()
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

// Test the following case:
//
// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Value",
//     "not_value": [],
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, AnyWellKnownTypesNoValueFieldForArray) {
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("Any"),
          StringPiece("Expect a \"value\" field for well-known types.")));
  AnyOut any;
  google::protobuf::Any* any_type = any.mutable_any();
  any_type->set_type_url("type.googleapis.com/google.protobuf.Value");

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Value")
      ->StartList("not_value")
      ->EndList()
      ->EndObject()
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

// Test the following case:
//
// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Struct",
//     "value": "",
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, AnyWellKnownTypesExpectObjectForStruct) {
  EXPECT_CALL(listener_,
              InvalidValue(_, StringPiece("Any"),
                           StringPiece("Expect a JSON object.")));
  AnyOut any;
  google::protobuf::Any* any_type = any.mutable_any();
  any_type->set_type_url("type.googleapis.com/google.protobuf.Struct");

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Struct")
      ->RenderString("value", "")
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

// Test the following case:
//
// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Any",
//     "value": "",
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, AnyWellKnownTypesExpectObjectForAny) {
  EXPECT_CALL(listener_,
              InvalidValue(_, StringPiece("Any"),
                           StringPiece("Expect a JSON object.")));
  AnyOut any;
  google::protobuf::Any* any_type = any.mutable_any();
  any_type->set_type_url("type.googleapis.com/google.protobuf.Any");

  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Any")
      ->RenderString("value", "")
      ->EndObject()
      ->EndObject();
  CheckOutput(any);
}

// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Any",
//     "value": null
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, AnyInAnyAcceptsNull) {
  AnyOut out;
  google::protobuf::Any empty;
  out.mutable_any()->PackFrom(empty);

  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);
  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Any")
      ->RenderNull("value")
      ->EndObject()
      ->EndObject();
  CheckOutput(out);
}

// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Timestamp",
//     "value": null
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, TimestampInAnyAcceptsNull) {
  AnyOut out;
  google::protobuf::Timestamp empty;
  out.mutable_any()->PackFrom(empty);

  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);
  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Timestamp")
      ->RenderNull("value")
      ->EndObject()
      ->EndObject();
  CheckOutput(out);
}

// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Duration",
//     "value": null
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, DurationInAnyAcceptsNull) {
  AnyOut out;
  google::protobuf::Duration empty;
  out.mutable_any()->PackFrom(empty);

  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);
  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Duration")
      ->RenderNull("value")
      ->EndObject()
      ->EndObject();
  CheckOutput(out);
}

// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.FieldMask",
//     "value": null
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, FieldMaskInAnyAcceptsNull) {
  AnyOut out;
  google::protobuf::FieldMask empty;
  out.mutable_any()->PackFrom(empty);

  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);
  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.FieldMask")
      ->RenderNull("value")
      ->EndObject()
      ->EndObject();
  CheckOutput(out);
}

// {
//   "any": {
//     "@type": "type.googleapis.com/google.protobuf.Int32Value",
//     "value": null
//   }
// }
TEST_P(ProtoStreamObjectWriterAnyTest, WrapperInAnyAcceptsNull) {
  AnyOut out;
  google::protobuf::Int32Value empty;
  out.mutable_any()->PackFrom(empty);

  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);
  ow_->StartObject("")
      ->StartObject("any")
      ->RenderString("@type", "type.googleapis.com/google.protobuf.Int32Value")
      ->RenderNull("value")
      ->EndObject()
      ->EndObject();
  CheckOutput(out);
}

class ProtoStreamObjectWriterFieldMaskTest
    : public BaseProtoStreamObjectWriterTest {
 protected:
  ProtoStreamObjectWriterFieldMaskTest() {
    std::vector<const Descriptor*> descriptors;
    descriptors.push_back(FieldMaskTest::descriptor());
    descriptors.push_back(google::protobuf::FieldMask::descriptor());
    ResetTypeInfo(descriptors);
  }
};

INSTANTIATE_TEST_SUITE_P(DifferentTypeInfoSourceTest,
                         ProtoStreamObjectWriterFieldMaskTest,
                         ::testing::Values(
                             testing::USE_TYPE_RESOLVER));

TEST_P(ProtoStreamObjectWriterFieldMaskTest, SimpleFieldMaskTest) {
  FieldMaskTest expected;
  expected.set_id("1");
  expected.mutable_single_mask()->add_paths("path1");

  ow_->StartObject("");
  ow_->RenderString("id", "1");
  ow_->RenderString("single_mask", "path1");
  ow_->EndObject();

  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest, MutipleMasksInCompactForm) {
  FieldMaskTest expected;
  expected.set_id("1");
  expected.mutable_single_mask()->add_paths("camel_case1");
  expected.mutable_single_mask()->add_paths("camel_case2");
  expected.mutable_single_mask()->add_paths("camel_case3");

  ow_->StartObject("");
  ow_->RenderString("id", "1");
  ow_->RenderString("single_mask", "camelCase1,camelCase2,camelCase3");
  ow_->EndObject();

  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest, RepeatedFieldMaskTest) {
  FieldMaskTest expected;
  expected.set_id("1");
  google::protobuf::FieldMask* mask = expected.add_repeated_mask();
  mask->add_paths("field1");
  mask->add_paths("field2");
  expected.add_repeated_mask()->add_paths("field3");

  ow_->StartObject("");
  ow_->RenderString("id", "1");
  ow_->StartList("repeated_mask");
  ow_->RenderString("", "field1,field2");
  ow_->RenderString("", "field3");
  ow_->EndList();
  ow_->EndObject();

  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest, EmptyFieldMaskTest) {
  FieldMaskTest expected;
  expected.set_id("1");

  ow_->StartObject("");
  ow_->RenderString("id", "1");
  ow_->RenderString("single_mask", "");
  ow_->EndObject();

  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest, MaskUsingApiaryStyleShouldWork) {
  FieldMaskTest expected;
  expected.set_id("1");

  ow_->StartObject("");
  ow_->RenderString("id", "1");
  // Case1
  ow_->RenderString("single_mask",
                    "outerField(camelCase1,camelCase2,camelCase3)");
  expected.mutable_single_mask()->add_paths("outer_field.camel_case1");
  expected.mutable_single_mask()->add_paths("outer_field.camel_case2");
  expected.mutable_single_mask()->add_paths("outer_field.camel_case3");

  ow_->StartList("repeated_mask");

  ow_->RenderString("", "a(field1,field2)");
  google::protobuf::FieldMask* mask = expected.add_repeated_mask();
  mask->add_paths("a.field1");
  mask->add_paths("a.field2");

  ow_->RenderString("", "a(field3)");
  mask = expected.add_repeated_mask();
  mask->add_paths("a.field3");

  ow_->RenderString("", "a()");
  expected.add_repeated_mask();

  ow_->RenderString("", "a(,)");
  expected.add_repeated_mask();

  ow_->RenderString("", "a(field1(field2(field3)))");
  mask = expected.add_repeated_mask();
  mask->add_paths("a.field1.field2.field3");

  ow_->RenderString("", "a(field1(field2(field3,field4),field5),field6)");
  mask = expected.add_repeated_mask();
  mask->add_paths("a.field1.field2.field3");
  mask->add_paths("a.field1.field2.field4");
  mask->add_paths("a.field1.field5");
  mask->add_paths("a.field6");

  ow_->RenderString("", "a(id,field1(id,field2(field3,field4),field5),field6)");
  mask = expected.add_repeated_mask();
  mask->add_paths("a.id");
  mask->add_paths("a.field1.id");
  mask->add_paths("a.field1.field2.field3");
  mask->add_paths("a.field1.field2.field4");
  mask->add_paths("a.field1.field5");
  mask->add_paths("a.field6");

  ow_->RenderString("", "a(((field3,field4)))");
  mask = expected.add_repeated_mask();
  mask->add_paths("a.field3");
  mask->add_paths("a.field4");

  ow_->EndList();
  ow_->EndObject();

  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest, MoreCloseThanOpenParentheses) {
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.FieldMask"),
          StringPiece("Field 'single_mask', Invalid FieldMask 'a(b,c))'. "
                            "Cannot find matching '(' for all ')'.")));

  ow_->StartObject("");
  ow_->RenderString("id", "1");
  ow_->RenderString("single_mask", "a(b,c))");
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest, MoreOpenThanCloseParentheses) {
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.FieldMask"),
          StringPiece(
              "Field 'single_mask', Invalid FieldMask 'a(((b,c)'. Cannot "
              "find matching ')' for all '('.")));

  ow_->StartObject("");
  ow_->RenderString("id", "1");
  ow_->RenderString("single_mask", "a(((b,c)");
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest, PathWithMapKeyShouldWork) {
  FieldMaskTest expected;
  expected.mutable_single_mask()->add_paths("path.to.map[\"key1\"]");
  expected.mutable_single_mask()->add_paths(
      "path.to.map[\"e\\\"[]][scape\\\"\"]");
  expected.mutable_single_mask()->add_paths("path.to.map[\"key2\"]");

  ow_->StartObject("");
  ow_->RenderString("single_mask",
                    "path.to.map[\"key1\"],path.to.map[\"e\\\"[]][scape\\\"\"],"
                    "path.to.map[\"key2\"]");
  ow_->EndObject();

  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest,
       MapKeyMustBeAtTheEndOfAPathSegment) {
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.FieldMask"),
          StringPiece(
              "Field 'single_mask', Invalid FieldMask "
              "'path.to.map[\"key1\"]a,path.to.map[\"key2\"]'. "
              "Map keys should be at the end of a path segment.")));

  ow_->StartObject("");
  ow_->RenderString("single_mask",
                    "path.to.map[\"key1\"]a,path.to.map[\"key2\"]");
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest, MapKeyMustEnd) {
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.FieldMask"),
          StringPiece("Field 'single_mask', Invalid FieldMask "
                            "'path.to.map[\"key1\"'. Map keys should be "
                            "represented as [\"some_key\"].")));

  ow_->StartObject("");
  ow_->RenderString("single_mask", "path.to.map[\"key1\"");
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest, MapKeyMustBeEscapedCorrectly) {
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("type.googleapis.com/google.protobuf.FieldMask"),
          StringPiece("Field 'single_mask', Invalid FieldMask "
                            "'path.to.map[\"ke\"y1\"]'. Map keys should be "
                            "represented as [\"some_key\"].")));

  ow_->StartObject("");
  ow_->RenderString("single_mask", "path.to.map[\"ke\"y1\"]");
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest, MapKeyCanContainAnyChars) {
  FieldMaskTest expected;
  expected.mutable_single_mask()->add_paths(
      // \xE5\xAD\x99 is the UTF-8 byte sequence for chinese character 孙.
      // We cannot embed non-ASCII characters in the code directly because
      // some windows compilers will try to interpret them using the system's
      // current encoding and end up with invalid UTF-8 byte sequence.
      "path.to.map[\"(),[],\\\"'!@#$%^&*123_|War\xE5\xAD\x99,./?><\\\\\"]");
  expected.mutable_single_mask()->add_paths("path.to.map[\"key2\"]");

  ow_->StartObject("");
  ow_->RenderString(
      "single_mask",
      "path.to.map[\"(),[],\\\"'!@#$%^&*123_|War\xE5\xAD\x99,./?><\\\\\"],"
      "path.to.map[\"key2\"]");
  ow_->EndObject();

  CheckOutput(expected);
}

TEST_P(ProtoStreamObjectWriterFieldMaskTest, FieldMaskAcceptsNull) {
  FieldMaskTest expected;
  EXPECT_CALL(listener_, InvalidValue(_, _, _)).Times(0);
  ow_->StartObject("")->RenderNull("single_mask")->EndObject();
  CheckOutput(expected);
}

class ProtoStreamObjectWriterWrappersTest
    : public BaseProtoStreamObjectWriterTest {
 protected:
  ProtoStreamObjectWriterWrappersTest() {
    std::vector<const Descriptor*> descriptors;
    descriptors.push_back(Int32Wrapper::descriptor());
    descriptors.push_back(google::protobuf::Int32Value::descriptor());
    ResetTypeInfo(descriptors);
  }
};

INSTANTIATE_TEST_SUITE_P(DifferentTypeInfoSourceTest,
                         ProtoStreamObjectWriterWrappersTest,
                         ::testing::Values(
                             testing::USE_TYPE_RESOLVER));

TEST_P(ProtoStreamObjectWriterWrappersTest, WrapperAcceptsNull) {
  Int32Wrapper wrapper;
  EXPECT_CALL(listener_, InvalidName(_, _, _)).Times(0);
  ow_->StartObject("")->RenderNull("int32")->EndObject();
  CheckOutput(wrapper);
}

class ProtoStreamObjectWriterOneOfsTest
    : public BaseProtoStreamObjectWriterTest {
 protected:
  ProtoStreamObjectWriterOneOfsTest() {
    std::vector<const Descriptor*> descriptors;
    descriptors.push_back(OneOfsRequest::descriptor());
    descriptors.push_back(google::protobuf::Struct::descriptor());
    ResetTypeInfo(descriptors);
  }
};

INSTANTIATE_TEST_SUITE_P(DifferentTypeInfoSourceTest,
                         ProtoStreamObjectWriterOneOfsTest,
                         ::testing::Values(
                             testing::USE_TYPE_RESOLVER));

TEST_P(ProtoStreamObjectWriterOneOfsTest,
       MultipleOneofsFailForPrimitiveTypesTest) {
  EXPECT_CALL(
      listener_,
      InvalidValue(
          _, StringPiece("oneof"),
          StringPiece(
              "oneof field 'data' is already set. Cannot set 'intData'")));

  ow_->StartObject("");
  ow_->RenderString("strData", "blah");
  ow_->RenderString("intData", "123");
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterOneOfsTest,
       MultipleOneofsFailForMessageTypesPrimitiveFirstTest) {
  // Test for setting primitive oneof field first and then message field.
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("oneof"),
                                      StringPiece(
                                          "oneof field 'data' is already set. "
                                          "Cannot set 'messageData'")));

  // JSON: { "strData": "blah", "messageData": { "dataValue": 123 } }
  ow_->StartObject("");
  ow_->RenderString("strData", "blah");
  ow_->StartObject("messageData");
  ow_->RenderInt32("dataValue", 123);
  ow_->EndObject();
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterOneOfsTest,
       MultipleOneofsFailForMessageTypesMessageFirstTest) {
  // Test for setting message oneof field first and then primitive field.
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("oneof"),
                                      StringPiece(
                                          "oneof field 'data' is already set. "
                                          "Cannot set 'strData'")));

  // JSON: { "messageData": { "dataValue": 123 }, "strData": "blah" }
  ow_->StartObject("");
  ow_->StartObject("messageData");
  ow_->RenderInt32("dataValue", 123);
  ow_->EndObject();
  ow_->RenderString("strData", "blah");
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterOneOfsTest,
       MultipleOneofsFailForStructTypesPrimitiveFirstTest) {
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("oneof"),
                                      StringPiece(
                                          "oneof field 'data' is already set. "
                                          "Cannot set 'structData'")));

  // JSON: { "strData": "blah", "structData": { "a": "b" } }
  ow_->StartObject("");
  ow_->RenderString("strData", "blah");
  ow_->StartObject("structData");
  ow_->RenderString("a", "b");
  ow_->EndObject();
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterOneOfsTest,
       MultipleOneofsFailForStructTypesStructFirstTest) {
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("oneof"),
                                      StringPiece(
                                          "oneof field 'data' is already set. "
                                          "Cannot set 'strData'")));

  // JSON: { "structData": { "a": "b" }, "strData": "blah" }
  ow_->StartObject("");
  ow_->StartObject("structData");
  ow_->RenderString("a", "b");
  ow_->EndObject();
  ow_->RenderString("strData", "blah");
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterOneOfsTest,
       MultipleOneofsFailForStructValueTypesTest) {
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("oneof"),
                                      StringPiece(
                                          "oneof field 'data' is already set. "
                                          "Cannot set 'valueData'")));

  // JSON: { "messageData": { "dataValue": 123 }, "valueData": { "a": "b" } }
  ow_->StartObject("");
  ow_->StartObject("messageData");
  ow_->RenderInt32("dataValue", 123);
  ow_->EndObject();
  ow_->StartObject("valueData");
  ow_->RenderString("a", "b");
  ow_->EndObject();
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterOneOfsTest,
       MultipleOneofsFailForWellKnownTypesPrimitiveFirstTest) {
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("oneof"),
                                      StringPiece(
                                          "oneof field 'data' is already set. "
                                          "Cannot set 'tsData'")));

  // JSON: { "intData": 123, "tsData": "1970-01-02T01:00:00.000Z" }
  ow_->StartObject("");
  ow_->RenderInt32("intData", 123);
  ow_->RenderString("tsData", "1970-01-02T01:00:00.000Z");
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterOneOfsTest,
       MultipleOneofsFailForWellKnownTypesWktFirstTest) {
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("oneof"),
                                      StringPiece(
                                          "oneof field 'data' is already set. "
                                          "Cannot set 'intData'")));

  // JSON: { "tsData": "1970-01-02T01:00:00.000Z", "intData": 123 }
  ow_->StartObject("");
  ow_->RenderString("tsData", "1970-01-02T01:00:00.000Z");
  ow_->RenderInt32("intData", 123);
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterOneOfsTest,
       MultipleOneofsFailForWellKnownTypesAndMessageTest) {
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("oneof"),
                                      StringPiece(
                                          "oneof field 'data' is already set. "
                                          "Cannot set 'messageData'")));

  // JSON: { "tsData": "1970-01-02T01:00:00.000Z",
  //         "messageData": { "dataValue": 123 } }
  ow_->StartObject("");
  ow_->RenderString("tsData", "1970-01-02T01:00:00.000Z");
  ow_->StartObject("messageData");
  ow_->RenderInt32("dataValue", 123);
  ow_->EndObject();
  ow_->EndObject();
}

TEST_P(ProtoStreamObjectWriterOneOfsTest,
       MultipleOneofsFailForOneofWithinAnyTest) {
  EXPECT_CALL(listener_, InvalidValue(_, StringPiece("oneof"),
                                      StringPiece(
                                          "oneof field 'data' is already set. "
                                          "Cannot set 'intData'")));

  // JSON:
  // { "anyData":
  //    { "@type":
  //       "type.googleapis.com/proto_util_converter.testing.oneofs.OneOfsRequest",
  //     "strData": "blah",
  //     "intData": 123
  //    }
  // }
  ow_->StartObject("");
  ow_->StartObject("anyData");
  ow_->RenderString(
      "@type",
      "type.googleapis.com/proto_util_converter.testing.oneofs.OneOfsRequest");
  ow_->RenderString("strData", "blah");
  ow_->RenderInt32("intData", 123);
  ow_->EndObject();
  ow_->EndObject();
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
