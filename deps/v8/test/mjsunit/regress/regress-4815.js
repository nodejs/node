// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var thrower = { [Symbol.toPrimitive]: () => FAIL };

// Tests that a native conversion function is included in the
// stack trace.
function testTraceNativeConversion(nativeFunc) {
  var nativeFuncName = nativeFunc.name;
  try {
    nativeFunc(thrower);
    assertUnreachable(nativeFuncName);
  } catch (e) {
    assertTrue(e.stack.indexOf(nativeFuncName) >= 0, nativeFuncName);
  }
}

// C++ builtins.
testTraceNativeConversion(Math.acos);
testTraceNativeConversion(Math.asin);
testTraceNativeConversion(Math.fround);
testTraceNativeConversion(Math.imul);


function testBuiltinInStackTrace(script, expectedString) {
  try {
    eval(script);
    assertUnreachable(expectedString);
  } catch (e) {
    assertTrue(e.stack.indexOf(expectedString) >= 0, expectedString);
  }
}

// C++ builtins.
testBuiltinInStackTrace("Boolean.prototype.toString.call(thrower);",
                        "at Object.toString");

// Constructor builtins.
testBuiltinInStackTrace("new Date(thrower);", "at new Date");

// Ensure we correctly pick up the receiver's string tag.
testBuiltinInStackTrace("Math.acos(thrower);", "at Math.acos");
testBuiltinInStackTrace("Math.asin(thrower);", "at Math.asin");
testBuiltinInStackTrace("Math.fround(thrower);", "at Math.fround");
testBuiltinInStackTrace("Math.imul(thrower);", "at Math.imul");

// As above, but function passed as an argument and then called.
testBuiltinInStackTrace("((f, x) => f(x))(Math.acos, thrower);", "at acos");
testBuiltinInStackTrace("((f, x) => f(x))(Math.asin, thrower);", "at asin");
testBuiltinInStackTrace("((f, x) => f(x))(Math.fround, thrower);", "at fround");
testBuiltinInStackTrace("((f, x) => f(x))(Math.imul, thrower);", "at imul");
