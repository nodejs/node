// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function testAdvanceStringIndex(lastIndex, expectedLastIndex) {
  let exec_count = 0;
  let last_last_index = -1;

  let fake_re = {
    exec: () => { return (exec_count++ == 0) ? [""] : null },
    get lastIndex() { return lastIndex; },
    set lastIndex(value) { last_last_index = value },
    get global() { return true; },
    get flags() { return "g"; }
  };

  assertEquals([""], RegExp.prototype[Symbol.match].call(fake_re, "abc"));
  assertEquals(expectedLastIndex, last_last_index);
}

testAdvanceStringIndex(new Number(42), 43);  // Value wrapper.
testAdvanceStringIndex(%AllocateHeapNumber(), 1);  // HeapNumber.
testAdvanceStringIndex(4294967295, 4294967296);  // HeapNumber.
