// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function testMethodNames(o) {
  try {
    o.k = 42;
  } catch (e) {
    Error.prepareStackTrace = function(e, frames) { return frames; };
    var frames = e.stack;
    Error.prepareStackTrace = undefined;
    assertEquals("f", frames[0].getMethodName());
    assertEquals(null, frames[1].getMethodName());
    assertEquals("h1", frames[2].getMethodName());
    assertEquals("j", frames[3].getMethodName());
    assertEquals("k", frames[4].getMethodName());
    assertEquals("testMethodNames", frames[5].getMethodName());
  }
}

var o = {
  f: function() { throw new Error(); },
  get j() { o.h1(); },
  set k(_) { o.j; },
};
const duplicate = function() { o.f() }
o.g1 = duplicate;
o.g2 = duplicate;
o.h1 = function() { o.g1() }
o.h2 = o.h1;

// Test in dictionary mode first.
assertFalse(%HasFastProperties(o));
testMethodNames(o);

// Same test but with fast mode object.
o = %ToFastProperties(o);
assertTrue(%HasFastProperties(o));
testMethodNames(o);

function testMethodName(f, frameNumber, expectedName) {
  try {
    Error.prepareStackTrace = function(e, frames) { return frames; }
    f();
    assertUnreachable();
  } catch (e) {
    const frame = e.stack[frameNumber];
    assertEquals(expectedName, frame.getMethodName());
  } finally {
    Error.prepareStackTrace = undefined;
  }
}

const thrower = { valueOf: () => { throw new Error(); }};

{
  const str = "";
  testMethodName(() => { str.indexOf(str, thrower); }, 1, "indexOf");
}

{
  const nr = 42;
  testMethodName(() => { nr.toString(thrower); }, 1, "toString");
}
