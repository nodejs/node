// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function enumerate(o) {
  var keys = [];
  for (var key in o) keys.push(key);
  return keys;
}

(function testSlowSloppyArgumentsElements()  {
  function slowSloppyArguments(a, b, c) {
    %HeapObjectVerify(arguments);
    arguments[10000] = "last";
    arguments[4000] = "first";
    arguments[6000] = "second";
    arguments[5999] = "x";
    arguments[3999] = "y";
    %HeapObjectVerify(arguments);
    return arguments;
  }
  assertEquals(["0", "1", "2", "3999", "4000", "5999", "6000", "10000"],
               Object.keys(slowSloppyArguments(1, 2, 3)));

  assertEquals(["0", "1", "2", "3999", "4000", "5999", "6000", "10000"],
               enumerate(slowSloppyArguments(1,2,3)));
})();

(function testSlowSloppyArgumentsElementsNotEnumerable() {
  function slowSloppyArguments(a, b, c) {
    Object.defineProperty(arguments, 10000, {
      enumerable: false, configurable: false, value: "NOPE"
    });
    %HeapObjectVerify(arguments);
    arguments[4000] = "first";
    arguments[6000] = "second";
    arguments[5999] = "x";
    arguments[3999] = "y";
    %HeapObjectVerify(arguments);
    return arguments;
  }

  assertEquals(["0", "1", "2", "3999", "4000", "5999", "6000"],
               Object.keys(slowSloppyArguments(1, 2, 3)));

  assertEquals(["0", "1", "2", "3999", "4000", "5999", "6000"],
                enumerate(slowSloppyArguments(1,2,3)));
})();


(function testFastSloppyArgumentsElements()  {
  function fastSloppyArguments(a, b, c) {
    arguments[5] = 1;
    arguments[7] = 0;
    arguments[3] = 2;
    %HeapObjectVerify(arguments);
    return arguments;
  }
  assertEquals(["0", "1", "2", "3", "5", "7"],
               Object.keys(fastSloppyArguments(1, 2, 3)));

  assertEquals(
      ["0", "1", "2", "3", "5", "7"], enumerate(fastSloppyArguments(1, 2, 3)));

  function fastSloppyArguments2(a, b, c) {
    delete arguments[0];
    %DebugPrint(arguments);
    %HeapObjectVerify(arguments);
    arguments[0] = "test";
    %DebugPrint(arguments);
    %HeapObjectVerify(arguments);
    return arguments;
  }

  assertEquals(["0", "1", "2"], Object.keys(fastSloppyArguments2(1, 2, 3)));
  assertEquals(["0", "1", "2"], enumerate(fastSloppyArguments2(1, 2, 3)));
})();

(function testFastSloppyArgumentsElementsNotEnumerable() {
  function fastSloppyArguments(a, b, c) {
    Object.defineProperty(arguments, 5, {
      enumerable: false, configurable: false, value: "NOPE"
    });
    %HeapObjectVerify(arguments);
    arguments[7] = 0;
    arguments[3] = 2;
    %HeapObjectVerify(arguments);
    return arguments;
  }
  assertEquals(
      ["0", "1", "2", "3", "7"], Object.keys(fastSloppyArguments(1, 2, 3)));

  assertEquals(
      ["0", "1", "2", "3", "7"], enumerate(fastSloppyArguments(1,2,3)));

  function fastSloppyArguments2(a, b, c) {
    delete arguments[0];
    %HeapObjectVerify(arguments);
    Object.defineProperty(arguments, 1, {
      enumerable: false, configurable: false, value: "NOPE"
    });
    arguments[0] = "test";
    %HeapObjectVerify(arguments);
    return arguments;
  }

  assertEquals(["0", "2"], Object.keys(fastSloppyArguments2(1, 2, 3)));
  assertEquals(["0", "2"], enumerate(fastSloppyArguments2(1, 2, 3)));
})();
