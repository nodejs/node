// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#include <stdlib.h>

#include "src/v8.h"

#include "src/ast/ast.h"
#include "src/zone/accounting-allocator.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;

TEST(List) {
  List<AstNode*>* list = new List<AstNode*>(0);
  CHECK_EQ(0, list->length());

  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator);
  AstValueFactory value_factory(&zone, 0);
  AstNodeFactory factory(&value_factory);
  AstNode* node = factory.NewEmptyStatement(kNoSourcePosition);
  list->Add(node);
  CHECK_EQ(1, list->length());
  CHECK_EQ(node, list->at(0));
  CHECK_EQ(node, list->last());

  const int kElements = 100;
  for (int i = 0; i < kElements; i++) {
    list->Add(node);
  }
  CHECK_EQ(1 + kElements, list->length());

  list->Clear();
  CHECK_EQ(0, list->length());
  delete list;
}

TEST(ConcatStrings) {
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator);
  AstValueFactory value_factory(&zone, 0);

  const AstRawString* one_byte = value_factory.GetOneByteString("a");

  uint16_t two_byte_buffer[] = {
      0x3b1,
  };
  const AstRawString* two_byte = value_factory.GetTwoByteString(
      Vector<const uint16_t>(two_byte_buffer, 1));

  const AstRawString* expectation = value_factory.GetOneByteString("aa");
  const AstRawString* result = value_factory.ConcatStrings(one_byte, one_byte);
  CHECK(result->is_one_byte());
  CHECK_EQ(expectation, result);

  uint16_t expectation_buffer_one_two[] = {'a', 0x3b1};
  expectation = value_factory.GetTwoByteString(
      Vector<const uint16_t>(expectation_buffer_one_two, 2));
  result = value_factory.ConcatStrings(one_byte, two_byte);
  CHECK(!result->is_one_byte());
  CHECK_EQ(expectation, result);

  uint16_t expectation_buffer_two_one[] = {0x3b1, 'a'};
  expectation = value_factory.GetTwoByteString(
      Vector<const uint16_t>(expectation_buffer_two_one, 2));
  result = value_factory.ConcatStrings(two_byte, one_byte);
  CHECK(!result->is_one_byte());
  CHECK_EQ(expectation, result);

  uint16_t expectation_buffer_two_two[] = {0x3b1, 0x3b1};
  expectation = value_factory.GetTwoByteString(
      Vector<const uint16_t>(expectation_buffer_two_two, 2));
  result = value_factory.ConcatStrings(two_byte, two_byte);
  CHECK(!result->is_one_byte());
  CHECK_EQ(expectation, result);
}
