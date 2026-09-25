// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

(function TestErrorObjectsRetainMap() {
  const error1 = new Error("foo");
  const error2 = new Error("bar");

  assertTrue(%HaveSameMap(error1, error2));

  // Trigger serialization of the stack-trace.
  error1.stack;
  assertTrue(%HaveSameMap(error1, error2));

  error2.stack;
  assertTrue(%HaveSameMap(error1, error2));
})();

(function TestPrepareStackTraceCallback() {
  Error.prepareStackTrace = (error, frame) => {
    return "custom stack trace No. 42";
  };

  const error = new Error("foo");

  // Check it twice, so both code paths in the accessor are exercised.
  assertEquals(error.stack, "custom stack trace No. 42");
  assertEquals(error.stack, "custom stack trace No. 42");
})();

(function TestPrepareStackTraceCallbackMessesWithProperty() {
  Error.prepareStackTrace = (error, frames) => {
    error.stack = "Yes, we can write to this!";
    return 42;
  };

  const error = new Error("foo");

  // Check it twice. The first returns the formatting result,
  // the second the value of the private symbol.
  assertEquals(error.stack, 42);
  assertEquals(error.stack, 42);
})();

(function TestPrepareStackTraceCallbackInstallsGetter() {
  Error.prepareStackTrace = (error, frames) => {
    Object.defineProperty(error, "stack", { get: () => 42 });
    return "<formatted stack trace>";
  };

  const error = new Error("foo");

  // Check it twice. The second time the accessor should be used.
  assertEquals(error.stack, "<formatted stack trace>");
  assertEquals(error.stack, 42);
})();

(function TestPrepareStackTraceCallbackInstallsSetter() {
  Error.prepareStackTrace = (error, frames) => {
    Object.defineProperty(error, "stack", { get: undefined, set: (x) => {
      error[42] = x;
    }});
    return "<formatted stack trace>";
  };

  const error = new Error("foo");
  // Cause the accessor to get installed.
  error.stack;

  error.stack = "Who needs stack traces anyway?";
  assertEquals(error[42], "Who needs stack traces anyway?");
  assertEquals(error.stack, undefined); // No getter.
})();

(function TestFormatStackPropertyInDictionaryMode() {
  Error.prepareStackTrace = (error, frames) => {
    return "<formatted stack trace>";
  };
  const error = new Error("foo");
  error[%MaxSmi()] = 42;

  assertTrue(%HasDictionaryElements(error));

  // Check it twice.
  assertEquals(error.stack, "<formatted stack trace>");
  assertEquals(error.stack, "<formatted stack trace>");
})();

(function TestTransitionToDictionaryModeAfterFormatting() {
  Error.prepareStackTrace = (error, frames) => {
    return "<formatted stack trace>";
  };
  const error = new Error("foo");
  assertFalse(%HasDictionaryElements(error));

  assertEquals(error.stack, "<formatted stack trace>");

  error[%MaxSmi()] = 42;
  assertTrue(%HasDictionaryElements(error));
  assertEquals(error.stack, "<formatted stack trace>");
})();
