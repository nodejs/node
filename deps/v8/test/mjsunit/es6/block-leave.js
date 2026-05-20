// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"use strict";

// We want to test the context chain shape.  In each of the tests cases
// below, the outer with is to force a runtime lookup of the identifier 'x'
// to actually verify that the inner context has been discarded.  A static
// lookup of 'x' might accidentally succeed.

{
  let x = 2;
  L: {
    let x = 3;
    assertEquals(3, x);
    break L;
    assertTrue(false);
  }
  assertEquals(2, x);
}

do {
  let x = 4;
  assertEquals(4,x);
  {
    let x = 5;
    assertEquals(5, x);
    continue;
    assertTrue(false);
  }
} while (false);

var caught = false;
try {
  {
    let xx = 18;
    throw 25;
    assertTrue(false);
  }
} catch (e) {
  caught = true;
  assertEquals(25, e);
  (function () {
    try {
      // NOTE: This checks that the block scope containing xx has been
      // removed from the context chain.
      eval('xx');
      assertTrue(false);  // should not reach here
    } catch (e2) {
      assertTrue(e2 instanceof ReferenceError);
    }
  })();
}
assertTrue(caught);


(function(x) {
  label: {
    let x = 'inner';
    break label;
  }
  assertEquals('outer', eval('x'));
})('outer');


(function(x) {
  label: {
    let x = 'middle';
    {
      let x = 'inner';
      break label;
    }
  }
  assertEquals('outer', eval('x'));
})('outer');


(function(x) {
  for (var i = 0; i < 10; ++i) {
    let x = 'inner' + i;
    continue;
  }
  assertEquals('outer', eval('x'));
})('outer');


(function(x) {
  label: for (var i = 0; i < 10; ++i) {
    let x = 'middle' + i;
    for (var j = 0; j < 10; ++j) {
      let x = 'inner' + j;
      continue label;
    }
  }
  assertEquals('outer', eval('x'));
})('outer');


(function(x) {
  try {
    let x = 'inner';
    throw 0;
  } catch (e) {
    assertEquals('outer', eval('x'));
  }
})('outer');


(function(x) {
  try {
    let x = 'middle';
    {
      let x = 'inner';
      throw 0;
    }
  } catch (e) {
    assertEquals('outer', eval('x'));
  }
})('outer');


try {
  (function(x) {
    try {
      let x = 'inner';
      throw 0;
    } finally {
      assertEquals('outer', eval('x'));
    }
  })('outer');
} catch (e) {
  if (e instanceof MjsUnitAssertionError) throw e;
}


try {
  (function(x) {
    try {
      let x = 'middle';
      {
        let x = 'inner';
        throw 0;
      }
    } finally {
      assertEquals('outer', eval('x'));
    }
  })('outer');
} catch (e) {
  if (e instanceof MjsUnitAssertionError) throw e;
}


// Verify that the context is correctly set in the stack frame after exiting
// from eval.
function f() {}

(function(x) {
  label: {
    let x = 'inner';
    break label;
  }
  f();  // The context could be restored from the stack after the call.
  assertEquals('outer', eval('x'));
})('outer');


(function(x) {
  for (var i = 0; i < 10; ++i) {
    let x = 'inner';
    continue;
  }
  f();
  assertEquals('outer', eval('x'));
})('outer');


(function(x) {
  try {
    let x = 'inner';
    throw 0;
  } catch (e) {
    f();
    assertEquals('outer', eval('x'));
  }
})('outer');


try {
  (function(x) {
    try {
      let x = 'inner';
      throw 0;
    } finally {
      f();
      assertEquals('outer', eval('x'));
    }
  })('outer');
} catch (e) {
  if (e instanceof MjsUnitAssertionError) throw e;
}
