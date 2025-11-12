// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/objects-inl.h"
#include "test/unittests/compiler/function-tester.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

using RunJSExceptionsTest = TestWithContext;

TEST_F(RunJSExceptionsTest, Throw) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { if (a) { throw b; } else { return b; }})");

  T.CheckThrows(T.true_value(), T.NewObject("new Error"));
  T.CheckCall(T.NewNumber(23), T.false_value(), T.NewNumber(23));
}

TEST_F(RunJSExceptionsTest, ThrowMessagePosition) {
  static const char* src =
      "(function(a, b) {        \n"
      "  if (a == 1) throw 1;   \n"
      "  if (a == 2) {throw 2}  \n"
      "  if (a == 3) {0;throw 3}\n"
      "  throw 4;               \n"
      "})                       ";
  FunctionTester T(i_isolate(), src);
  v8::Local<v8::Message> message;
  v8::Local<v8::Context> context = isolate()->GetCurrentContext();

  message = T.CheckThrowsReturnMessage(T.NewNumber(1), T.undefined());
  CHECK_EQ(2, message->GetLineNumber(context).FromMaybe(-1));
  CHECK_EQ(40, message->GetStartPosition());

  message = T.CheckThrowsReturnMessage(T.NewNumber(2), T.undefined());
  CHECK_EQ(3, message->GetLineNumber(context).FromMaybe(-1));
  CHECK_EQ(67, message->GetStartPosition());

  message = T.CheckThrowsReturnMessage(T.NewNumber(3), T.undefined());
  CHECK_EQ(4, message->GetLineNumber(context).FromMaybe(-1));
  CHECK_EQ(95, message->GetStartPosition());
}

TEST_F(RunJSExceptionsTest, ThrowMessageDirectly) {
  static const char* src =
      "(function(a, b) {"
      "  if (a) { throw b; } else { throw new Error(b); }"
      "})";
  FunctionTester T(i_isolate(), src);
  v8::Local<v8::Message> message;
  v8::Local<v8::Context> context = isolate()->GetCurrentContext();
  v8::Maybe<bool> t = v8::Just(true);

  message = T.CheckThrowsReturnMessage(T.false_value(), T.NewString("Wat?"));
  CHECK(t ==
        message->Get()->Equals(context, NewString("Uncaught Error: Wat?")));

  message = T.CheckThrowsReturnMessage(T.true_value(), T.NewString("Kaboom!"));
  CHECK(t == message->Get()->Equals(context, NewString("Uncaught Kaboom!")));
}

TEST_F(RunJSExceptionsTest, ThrowMessageIndirectly) {
  static const char* src =
      "(function(a, b) {"
      "  try {"
      "    if (a) { throw b; } else { throw new Error(b); }"
      "  } finally {"
      "    try { throw 'clobber'; } catch (e) { 'unclobber'; }"
      "  }"
      "})";
  FunctionTester T(i_isolate(), src);
  v8::Local<v8::Message> message;
  v8::Local<v8::Context> context = isolate()->GetCurrentContext();
  v8::Maybe<bool> t = v8::Just(true);

  message = T.CheckThrowsReturnMessage(T.false_value(), T.NewString("Wat?"));
  CHECK(t ==
        message->Get()->Equals(context, NewString("Uncaught Error: Wat?")));

  message = T.CheckThrowsReturnMessage(T.true_value(), T.NewString("Kaboom!"));
  CHECK(t == message->Get()->Equals(context, NewString("Uncaught Kaboom!")));
}

TEST_F(RunJSExceptionsTest, Catch) {
  const char* src =
      "(function(a,b) {"
      "  var r = '-';"
      "  try {"
      "    r += 'A-';"
      "    throw 'B-';"
      "  } catch (e) {"
      "    r += e;"
      "  }"
      "  return r;"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewString("-A-B-"));
}

TEST_F(RunJSExceptionsTest, CatchNested) {
  const char* src =
      "(function(a,b) {"
      "  var r = '-';"
      "  try {"
      "    r += 'A-';"
      "    throw 'C-';"
      "  } catch (e) {"
      "    try {"
      "      throw 'B-';"
      "    } catch (e) {"
      "      r += e;"
      "    }"
      "    r += e;"
      "  }"
      "  return r;"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewString("-A-B-C-"));
}

TEST_F(RunJSExceptionsTest, CatchBreak) {
  const char* src =
      "(function(a,b) {"
      "  var r = '-';"
      "  L: try {"
      "    r += 'A-';"
      "    if (a) break L;"
      "    r += 'B-';"
      "    throw 'C-';"
      "  } catch (e) {"
      "    if (b) break L;"
      "    r += e;"
      "  }"
      "  r += 'D-';"
      "  return r;"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewString("-A-D-"), T.true_value(), T.false_value());
  T.CheckCall(T.NewString("-A-B-D-"), T.false_value(), T.true_value());
  T.CheckCall(T.NewString("-A-B-C-D-"), T.false_value(), T.false_value());
}

TEST_F(RunJSExceptionsTest, CatchCall) {
  const char* src =
      "(function(fun) {"
      "  var r = '-';"
      "  try {"
      "    r += 'A-';"
      "    return r + 'B-' + fun();"
      "  } catch (e) {"
      "    r += e;"
      "  }"
      "  return r;"
      "})";
  FunctionTester T(i_isolate(), src);

  TryRunJS("function thrower() { throw 'T-'; }");
  T.CheckCall(T.NewString("-A-T-"), T.NewFunction("thrower"));
  TryRunJS("function returner() { return 'R-'; }");
  T.CheckCall(T.NewString("-A-B-R-"), T.NewFunction("returner"));
}

TEST_F(RunJSExceptionsTest, Finally) {
  const char* src =
      "(function(a,b) {"
      "  var r = '-';"
      "  try {"
      "    r += 'A-';"
      "  } finally {"
      "    r += 'B-';"
      "  }"
      "  return r;"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewString("-A-B-"));
}

TEST_F(RunJSExceptionsTest, FinallyBreak) {
  const char* src =
      "(function(a,b) {"
      "  var r = '-';"
      "  L: try {"
      "    r += 'A-';"
      "    if (a) return r;"
      "    r += 'B-';"
      "    if (b) break L;"
      "    r += 'C-';"
      "  } finally {"
      "    r += 'D-';"
      "  }"
      "  return r;"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewString("-A-"), T.true_value(), T.false_value());
  T.CheckCall(T.NewString("-A-B-D-"), T.false_value(), T.true_value());
  T.CheckCall(T.NewString("-A-B-C-D-"), T.false_value(), T.false_value());
}

TEST_F(RunJSExceptionsTest, DeoptTry) {
  const char* src =
      "(function f(a) {"
      "  try {"
      "    %DeoptimizeFunction(f);"
      "    throw a;"
      "  } catch (e) {"
      "    return e + 1;"
      "  }"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewNumber(2), T.NewNumber(1));
}

TEST_F(RunJSExceptionsTest, DeoptCatch) {
  const char* src =
      "(function f(a) {"
      "  try {"
      "    throw a;"
      "  } catch (e) {"
      "    %DeoptimizeFunction(f);"
      "    return e + 1;"
      "  }"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewNumber(2), T.NewNumber(1));
}

TEST_F(RunJSExceptionsTest, DeoptFinallyReturn) {
  const char* src =
      "(function f(a) {"
      "  try {"
      "    throw a;"
      "  } finally {"
      "    %DeoptimizeFunction(f);"
      "    return a + 1;"
      "  }"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewNumber(2), T.NewNumber(1));
}

TEST_F(RunJSExceptionsTest, DeoptFinallyReThrow) {
  const char* src =
      "(function f(a) {"
      "  try {"
      "    throw a;"
      "  } finally {"
      "    %DeoptimizeFunction(f);"
      "  }"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckThrows(T.NewObject("new Error"), T.NewNumber(1));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
