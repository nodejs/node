// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test() {
  assertEquals("string", typeof "");
  assertEquals("number", typeof 1.1);
  assertEquals("number", typeof 1);
  assertEquals("boolean", typeof true);
  assertEquals("function", typeof function() {});
  assertEquals("object", typeof null);
  assertEquals("object", typeof {});
  assertEquals("object", typeof /regex/);

  assertTrue(%IsSmi(1));
  assertFalse(%IsSmi(1.1));
  assertFalse(%IsSmi({}));

  assertTrue(%IsArray([1]));
  assertFalse(%IsArray(function() {}));

  assertTrue(%IsJSReceiver(new Date()));
  assertFalse(%IsJSReceiver(1));
}

%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeFunctionOnNextCall(test);
test();
