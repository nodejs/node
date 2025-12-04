// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-stack-trace-frames

// Verifies that "print" shows up in Error.stack:
// Error
//     at foo (...)
//     at Object.toString (...)
//     at print (<anonymous>)
//     at bar (...)
//     at (...)
let prepareStackTraceCalled = false;
Error.prepareStackTrace = (e, frames) => {
  prepareStackTraceCalled = true;
  assertEquals(5, frames.length);

  assertEquals(foo, frames[0].getFunction());
  assertEquals(object.toString, frames[1].getFunction());
  assertEquals("print", frames[2].getFunctionName());
  assertEquals(bar, frames[3].getFunction());
  return frames;
};

function foo() { throw new Error(); }
const object = { toString: () => { return foo(); } };

function bar() {
  print(object);
}

try { bar(); } catch(e) {
  // Trigger prepareStackTrace.
  e.stack;
}
assertTrue(prepareStackTraceCalled);
