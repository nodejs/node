// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --experimental-stack-trace-frames

// Verifies that "print" shows up in Error.stack when "bar" is optimized
// by Turbofan and that the "print" function belongs to the correct context:
// Error
//     at foo (...)
//     at Object.toString (...)
//     at print (<anonymous>)
//     at bar (...)
//     at (...)
let realm = Realm.createAllowCrossRealmAccess();
let remote_print = Realm.eval(realm, `print`);

let prepareStackTraceCalled = false;
Error.prepareStackTrace = (e, frames) => {
  prepareStackTraceCalled = true;
  assertEquals(5, frames.length);

  assertEquals(foo, frames[0].getFunction());
  assertEquals(object.toString, frames[1].getFunction());
  assertEquals("print", frames[2].getFunctionName());
  // Ensure print function in instantiated for the correct context.
  assertEquals(remote_print, frames[2].getFunction());
  assertEquals(bar, frames[3].getFunction());
  return frames;
};

function foo() { throw new Error(); }
const object = { toString: () => { return foo(); } };

function bar() {
  remote_print(object);
}

%PrepareFunctionForOptimization(bar);
try { bar(); } catch (e) {}
try { bar(); } catch (e) {}
%OptimizeFunctionOnNextCall(bar);

try { bar(); } catch(e) {
  // Trigger prepareStackTrace.
  e.stack;
}

assertOptimized(bar);
assertTrue(prepareStackTraceCalled);
