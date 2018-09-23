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

  assertTrue(%_IsObject({}));
  assertTrue(%_IsObject(null));
  assertTrue(%_IsObject(/regex/));
  assertFalse(%_IsObject(0));
  assertFalse(%_IsObject(""));

  assertTrue(%_IsSmi(1));
  assertFalse(%_IsSmi(1.1));
  assertFalse(%_IsSmi({}));

  assertTrue(%_IsRegExp(/regexp/));
  assertFalse(%_IsRegExp({}));

  assertTrue(%_IsArray([1]));
  assertFalse(%_IsArray(function() {}));

  assertTrue(%_IsFunction(function() {}));
  assertFalse(%_IsFunction(null));

  assertTrue(%_IsSpecObject(new Date()));
  assertFalse(%_IsSpecObject(1));

  assertTrue(%_IsMinusZero(-0.0));
  assertFalse(%_IsMinusZero(1));
  assertFalse(%_IsMinusZero(""));
}


test();
test();
%OptimizeFunctionOnNextCall(test);
test();
