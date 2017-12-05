// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

TEST(ArgumentsMapped) {
  FunctionTester T("(function(a) { return arguments; })");

  Handle<Object> arguments;
  T.Call(T.Val(19), T.Val(23), T.Val(42), T.Val(65)).ToHandle(&arguments);
  CHECK(arguments->IsJSObject() && !arguments->IsJSArray());
  CHECK(JSObject::cast(*arguments)->HasSloppyArgumentsElements());
  Handle<String> l = T.isolate->factory()->length_string();
  Handle<Object> length = Object::GetProperty(arguments, l).ToHandleChecked();
  CHECK_EQ(4, length->Number());
}


TEST(ArgumentsUnmapped) {
  FunctionTester T("(function(a) { 'use strict'; return arguments; })");

  Handle<Object> arguments;
  T.Call(T.Val(19), T.Val(23), T.Val(42), T.Val(65)).ToHandle(&arguments);
  CHECK(arguments->IsJSObject() && !arguments->IsJSArray());
  CHECK(!JSObject::cast(*arguments)->HasSloppyArgumentsElements());
  Handle<String> l = T.isolate->factory()->length_string();
  Handle<Object> length = Object::GetProperty(arguments, l).ToHandleChecked();
  CHECK_EQ(4, length->Number());
}


TEST(ArgumentsRest) {
  FunctionTester T("(function(a, ...args) { return args; })");

  Handle<Object> arguments;
  T.Call(T.Val(19), T.Val(23), T.Val(42), T.Val(65)).ToHandle(&arguments);
  CHECK(arguments->IsJSObject() && arguments->IsJSArray());
  CHECK(!JSObject::cast(*arguments)->HasSloppyArgumentsElements());
  Handle<String> l = T.isolate->factory()->length_string();
  Handle<Object> length = Object::GetProperty(arguments, l).ToHandleChecked();
  CHECK_EQ(3, length->Number());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
