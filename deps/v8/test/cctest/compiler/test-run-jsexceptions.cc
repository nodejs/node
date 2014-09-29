// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "test/cctest/compiler/function-tester.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

TEST(Throw) {
  FunctionTester T("(function(a,b) { if (a) { throw b; } else { return b; }})");

  T.CheckThrows(T.true_value(), T.NewObject("new Error"));
  T.CheckCall(T.Val(23), T.false_value(), T.Val(23));
}


TEST(ThrowSourcePosition) {
  static const char* src =
      "(function(a, b) {        \n"
      "  if (a == 1) throw 1;   \n"
      "  if (a == 2) {throw 2}  \n"
      "  if (a == 3) {0;throw 3}\n"
      "  throw 4;               \n"
      "})                       ";
  FunctionTester T(src);
  v8::Handle<v8::Message> message;

  message = T.CheckThrowsReturnMessage(T.Val(1), T.undefined());
  CHECK(!message.IsEmpty());
  CHECK_EQ(2, message->GetLineNumber());
  CHECK_EQ(40, message->GetStartPosition());

  message = T.CheckThrowsReturnMessage(T.Val(2), T.undefined());
  CHECK(!message.IsEmpty());
  CHECK_EQ(3, message->GetLineNumber());
  CHECK_EQ(67, message->GetStartPosition());

  message = T.CheckThrowsReturnMessage(T.Val(3), T.undefined());
  CHECK(!message.IsEmpty());
  CHECK_EQ(4, message->GetLineNumber());
  CHECK_EQ(95, message->GetStartPosition());
}
