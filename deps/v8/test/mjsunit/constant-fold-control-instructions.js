// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fold-constants

function test() {
  assertEquals("string", typeof "");
  assertEquals("number", typeof 1.1);
  assertEquals("number", typeof 1);
  assertEquals("boolean", typeof true);
  assertEquals("function", typeof function() {});
  assertEquals("object", typeof null);
  assertEquals("object", typeof {});
  assertEquals("object", typeof /regex/);

  assertTrue(%_IsSmi(1));
  assertFalse(%_IsSmi(1.1));
  assertFalse(%_IsSmi({}));

  assertTrue(%_IsRegExp(/regexp/));
  assertFalse(%_IsRegExp({}));

  assertTrue(%_IsArray([1]));
  assertFalse(%_IsArray(function() {}));

  assertTrue(%_IsFunction(function() {}));
  assertFalse(%_IsFunction(null));

  assertTrue(%_IsJSReceiver(new Date()));
  assertFalse(%_IsJSReceiver(1));

  assertTrue(%_IsMinusZero(-0.0));
  assertFalse(%_IsMinusZero(1));
  assertFalse(%_IsMinusZero(""));
}


test();
test();
%OptimizeFunctionOnNextCall(test);
test();
