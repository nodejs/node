// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm

// Test that CallSite#getFunction and CallSite#getThis throw inside ShadowRealms
// and cannot access objects from the outside, as otherwise we could violate the
// callable boundary invariant.
(function testInside() {
  const shadowRealm = new ShadowRealm();

  // The ShadowRealm won't have assertThrows, so use try-catch and accumulate a
  // message string.
  const wrapped = shadowRealm.evaluate(`
Error.prepareStackTrace = function(err, frames) {
  let a = [];
  for (let i = 0; i < frames.length; i++) {
    try {
      a.push(frames[i].getFunction());
    } catch (e) {
      a.push("getFunction threw");
    }
    try {
      a.push(frames[i].getThis());
    } catch (e) {
      a.push("getThis threw");
    }
  }
  return a.join(' ');
};

function inner() {
  try {
    throw new Error();
  } catch (e) {
    return e.stack;
  }
}

inner;
`);

  (function outer() {
    // There are 4 frames, youngest to oldest:
    //
    //   inner
    //   outer
    //   testInside
    //   top-level
    //
    // So getFunction/getThis should throw 4 times since the prepareStackTrace
    // hook is executing inside the ShadowRealm.
    assertEquals("getFunction threw getThis threw " +
                 "getFunction threw getThis threw " +
                 "getFunction threw getThis threw " +
                 "getFunction threw getThis threw", wrapped());
  })();
})();

// Test that CallSite#getFunction and CallSite#getThis throw for ShadowRealm
// objects from the outside, as otherwise we can also violate the callable
// boundary.
(function testOutside() {
  Error.prepareStackTrace = function(err, frames) {
    let a = [];
    for (let i = 0; i < frames.length; i++) {
      try {
        frames[i].getFunction();
        a.push(`functionName: ${frames[i].getFunctionName()}`);
      } catch (e) {
        a.push(`${frames[i].getFunctionName()} threw`);
      }
      try {
        frames[i].getThis();
        a.push("t");
      } catch (e) {
        a.push("getThis threw");
      }
    }
    return JSON.stringify(a);
  };
  const shadowRealm = new ShadowRealm();
  const wrap = shadowRealm.evaluate(`
function trampolineMaker(callback) {
  return function trampoline() { return callback(); };
}
trampolineMaker;
`);
  const wrapped = wrap(function callback() {
    try {
      throw new Error();
    } catch (e) {
      return e.stack;
    }
  });


  // There are 4 frames, youngest to oldest:
  //
  //   callback     (in outer realm)
  //   trampoline   (in ShadowRealm)
  //   testOutside  (in outer realm)
  //   top-level    (in outer realm)
  //
  // The frame corresponding to trampoline should throw, since the outer realm
  // should not get references to ShadowRealm objects.
  assertEquals(JSON.stringify(
    ["functionName: callback", "t",
     "trampoline threw", "getThis threw",
     "functionName: testOutside", "t",
     "functionName: null", "t"]), wrapped());
  assertEquals
})();
