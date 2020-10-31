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
#include <google/protobuf/stubs/status.h>

#include <stdio.h>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace {
TEST(Status, Empty) {
  util::Status status;
  EXPECT_EQ(util::error::OK, util::Status::OK.error_code());
  EXPECT_EQ(util::error::OK, util::Status::OK.code());
  EXPECT_EQ("OK", util::Status::OK.ToString());
}

TEST(Status, GenericCodes) {
  EXPECT_EQ(util::error::OK, util::Status::OK.error_code());
  EXPECT_EQ(util::error::OK, util::Status::OK.code());
  EXPECT_EQ(util::error::CANCELLED, util::Status::CANCELLED.error_code());
  EXPECT_EQ(util::error::CANCELLED, util::Status::CANCELLED.code());
  EXPECT_EQ(util::error::UNKNOWN, util::Status::UNKNOWN.error_code());
  EXPECT_EQ(util::error::UNKNOWN, util::Status::UNKNOWN.code());
}

TEST(Status, ConstructorZero) {
  util::Status status(util::error::OK, "msg");
  EXPECT_TRUE(status.ok());
  EXPECT_EQ("OK", status.ToString());
}

TEST(Status, CheckOK) {
  util::Status status;
  GOOGLE_CHECK_OK(status);
  GOOGLE_CHECK_OK(status) << "Failed";
  GOOGLE_DCHECK_OK(status) << "Failed";
}

TEST(Status, ErrorMessage) {
  util::Status status(util::error::INVALID_ARGUMENT, "");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ("", status.error_message().ToString());
  EXPECT_EQ("", status.message().ToString());
  EXPECT_EQ("INVALID_ARGUMENT", status.ToString());
  status = util::Status(util::error::INVALID_ARGUMENT, "msg");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ("msg", status.error_message().ToString());
  EXPECT_EQ("msg", status.message().ToString());
  EXPECT_EQ("INVALID_ARGUMENT:msg", status.ToString());
  status = util::Status(util::error::OK, "msg");
  EXPECT_TRUE(status.ok());
  EXPECT_EQ("", status.error_message().ToString());
  EXPECT_EQ("", status.message().ToString());
  EXPECT_EQ("OK", status.ToString());
}

TEST(Status, Copy) {
  util::Status a(util::error::UNKNOWN, "message");
  util::Status b(a);
  ASSERT_EQ(a.ToString(), b.ToString());
}

TEST(Status, Assign) {
  util::Status a(util::error::UNKNOWN, "message");
  util::Status b;
  b = a;
  ASSERT_EQ(a.ToString(), b.ToString());
}

TEST(Status, AssignEmpty) {
  util::Status a(util::error::UNKNOWN, "message");
  util::Status b;
  a = b;
  ASSERT_EQ(string("OK"), a.ToString());
  ASSERT_TRUE(b.ok());
  ASSERT_TRUE(a.ok());
}

TEST(Status, EqualsOK) {
  ASSERT_EQ(util::Status::OK, util::Status());
}

TEST(Status, EqualsSame) {
  const util::Status a = util::Status(util::error::CANCELLED, "message");
  const util::Status b = util::Status(util::error::CANCELLED, "message");
  ASSERT_EQ(a, b);
}

TEST(Status, EqualsCopy) {
  const util::Status a = util::Status(util::error::CANCELLED, "message");
  const util::Status b = a;
  ASSERT_EQ(a, b);
}

TEST(Status, EqualsDifferentCode) {
  const util::Status a = util::Status(util::error::CANCELLED, "message");
  const util::Status b = util::Status(util::error::UNKNOWN, "message");
  ASSERT_NE(a, b);
}

TEST(Status, EqualsDifferentMessage) {
  const util::Status a = util::Status(util::error::CANCELLED, "message");
  const util::Status b = util::Status(util::error::CANCELLED, "another");
  ASSERT_NE(a, b);
}
}  // namespace
}  // namespace protobuf
}  // namespace google
