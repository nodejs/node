// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects-inl.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

TEST(Throw) {
  FunctionTester T("(function(a,b) { if (a) { throw b; } else { return b; }})");

  T.CheckThrows(T.true_value(), T.NewObject("new Error"));
  T.CheckCall(T.Val(23), T.false_value(), T.Val(23));
}


TEST(ThrowMessagePosition) {
  static const char* src =
      "(function(a, b) {        \n"
      "  if (a == 1) throw 1;   \n"
      "  if (a == 2) {throw 2}  \n"
      "  if (a == 3) {0;throw 3}\n"
      "  throw 4;               \n"
      "})                       ";
  FunctionTester T(src);
  v8::Local<v8::Message> message;
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();

  message = T.CheckThrowsReturnMessage(T.Val(1), T.undefined());
  CHECK_EQ(2, message->GetLineNumber(context).FromMaybe(-1));
  CHECK_EQ(40, message->GetStartPosition());

  message = T.CheckThrowsReturnMessage(T.Val(2), T.undefined());
  CHECK_EQ(3, message->GetLineNumber(context).FromMaybe(-1));
  CHECK_EQ(67, message->GetStartPosition());

  message = T.CheckThrowsReturnMessage(T.Val(3), T.undefined());
  CHECK_EQ(4, message->GetLineNumber(context).FromMaybe(-1));
  CHECK_EQ(95, message->GetStartPosition());
}


TEST(ThrowMessageDirectly) {
  static const char* src =
      "(function(a, b) {"
      "  if (a) { throw b; } else { throw new Error(b); }"
      "})";
  FunctionTester T(src);
  v8::Local<v8::Message> message;
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  v8::Maybe<bool> t = v8::Just(true);

  message = T.CheckThrowsReturnMessage(T.false_value(), T.Val("Wat?"));
  CHECK(t == message->Get()->Equals(context, v8_str("Uncaught Error: Wat?")));

  message = T.CheckThrowsReturnMessage(T.true_value(), T.Val("Kaboom!"));
  CHECK(t == message->Get()->Equals(context, v8_str("Uncaught Kaboom!")));
}


TEST(ThrowMessageIndirectly) {
  static const char* src =
      "(function(a, b) {"
      "  try {"
      "    if (a) { throw b; } else { throw new Error(b); }"
      "  } finally {"
      "    try { throw 'clobber'; } catch (e) { 'unclobber'; }"
      "  }"
      "})";
  FunctionTester T(src);
  v8::Local<v8::Message> message;
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  v8::Maybe<bool> t = v8::Just(true);

  message = T.CheckThrowsReturnMessage(T.false_value(), T.Val("Wat?"));
  CHECK(t == message->Get()->Equals(context, v8_str("Uncaught Error: Wat?")));

  message = T.CheckThrowsReturnMessage(T.true_value(), T.Val("Kaboom!"));
  CHECK(t == message->Get()->Equals(context, v8_str("Uncaught Kaboom!")));
}


TEST(Catch) {
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
  FunctionTester T(src);

  T.CheckCall(T.Val("-A-B-"));
}


TEST(CatchNested) {
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
  FunctionTester T(src);

  T.CheckCall(T.Val("-A-B-C-"));
}


TEST(CatchBreak) {
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
  FunctionTester T(src);

  T.CheckCall(T.Val("-A-D-"), T.true_value(), T.false_value());
  T.CheckCall(T.Val("-A-B-D-"), T.false_value(), T.true_value());
  T.CheckCall(T.Val("-A-B-C-D-"), T.false_value(), T.false_value());
}


TEST(CatchCall) {
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
  FunctionTester T(src);

  CompileRun("function thrower() { throw 'T-'; }");
  T.CheckCall(T.Val("-A-T-"), T.NewFunction("thrower"));
  CompileRun("function returner() { return 'R-'; }");
  T.CheckCall(T.Val("-A-B-R-"), T.NewFunction("returner"));
}


TEST(Finally) {
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
  FunctionTester T(src);

  T.CheckCall(T.Val("-A-B-"));
}


TEST(FinallyBreak) {
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
  FunctionTester T(src);

  T.CheckCall(T.Val("-A-"), T.true_value(), T.false_value());
  T.CheckCall(T.Val("-A-B-D-"), T.false_value(), T.true_value());
  T.CheckCall(T.Val("-A-B-C-D-"), T.false_value(), T.false_value());
}


TEST(DeoptTry) {
  const char* src =
      "(function f(a) {"
      "  try {"
      "    %DeoptimizeFunction(f);"
      "    throw a;"
      "  } catch (e) {"
      "    return e + 1;"
      "  }"
      "})";
  FunctionTester T(src);

  T.CheckCall(T.Val(2), T.Val(1));
}


TEST(DeoptCatch) {
  const char* src =
      "(function f(a) {"
      "  try {"
      "    throw a;"
      "  } catch (e) {"
      "    %DeoptimizeFunction(f);"
      "    return e + 1;"
      "  }"
      "})";
  FunctionTester T(src);

  T.CheckCall(T.Val(2), T.Val(1));
}


TEST(DeoptFinallyReturn) {
  const char* src =
      "(function f(a) {"
      "  try {"
      "    throw a;"
      "  } finally {"
      "    %DeoptimizeFunction(f);"
      "    return a + 1;"
      "  }"
      "})";
  FunctionTester T(src);

  T.CheckCall(T.Val(2), T.Val(1));
}


TEST(DeoptFinallyReThrow) {
  const char* src =
      "(function f(a) {"
      "  try {"
      "    throw a;"
      "  } finally {"
      "    %DeoptimizeFunction(f);"
      "  }"
      "})";
  FunctionTester T(src);

  T.CheckThrows(T.NewObject("new Error"), T.Val(1));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
