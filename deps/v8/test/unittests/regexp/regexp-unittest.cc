// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-regexp-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

TEST_F(TestWithNativeContext, ConvertRegExpFlagsToString) {
  RunJS("let regexp = new RegExp(/ab+c/ig);");
  Handle<JSRegExp> regexp = RunJS<JSRegExp>("regexp");
  Handle<String> flags = RunJS<String>("regexp.flags");
  Handle<String> converted_flags =
      JSRegExp::StringFromFlags(isolate(), regexp->GetFlags());
  EXPECT_TRUE(String::Equals(isolate(), flags, converted_flags));
}

TEST_F(TestWithNativeContext, ConvertRegExpFlagsToStringNoFlags) {
  RunJS("let regexp = new RegExp(/ab+c/);");
  Handle<JSRegExp> regexp = RunJS<JSRegExp>("regexp");
  Handle<String> flags = RunJS<String>("regexp.flags");
  Handle<String> converted_flags =
      JSRegExp::StringFromFlags(isolate(), regexp->GetFlags());
  EXPECT_TRUE(String::Equals(isolate(), flags, converted_flags));
}

TEST_F(TestWithNativeContext, ConvertRegExpFlagsToStringAllFlags) {
  RunJS("let regexp = new RegExp(/ab+c/dgimsuy);");
  Handle<JSRegExp> regexp = RunJS<JSRegExp>("regexp");
  Handle<String> flags = RunJS<String>("regexp.flags");
  Handle<String> converted_flags =
      JSRegExp::StringFromFlags(isolate(), regexp->GetFlags());
  EXPECT_TRUE(String::Equals(isolate(), flags, converted_flags));
}

}  // namespace internal
}  // namespace v8
