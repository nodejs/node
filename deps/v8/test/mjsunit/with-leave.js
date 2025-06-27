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

L: with ({x:12}) {
  assertEquals(12, x);
  break L;
  assertTrue(false);
}

do {
  with ({x:15}) {
    assertEquals(15, x);
    continue;
    assertTrue(false);
  }
} while (false);

var caught = false;
try {
  with ({x:18}) { throw 25; assertTrue(false); }
} catch (e) {
  caught = true;
  assertEquals(25, e);
  with ({y:19}) {
    assertEquals(19, y);
    try {
      // NOTE: This checks that the object containing x has been
      // removed from the context chain.
      x;
      assertTrue(false);  // should not reach here
    } catch (e2) {
      assertTrue(e2 instanceof ReferenceError);
    }
  }
}
assertTrue(caught);


// We want to test the context chain shape.  In each of the tests cases
// below, the outer with is to force a runtime lookup of the identifier 'x'
// to actually verify that the inner context has been discarded.  A static
// lookup of 'x' might accidentally succeed.
with ({x: 'outer'}) {
  label: {
    with ({x: 'inner'}) {
      break label;
    }
  }
  assertEquals('outer', x);
}


with ({x: 'outer'}) {
  label: {
    with ({x: 'middle'}) {
      with ({x: 'inner'}) {
        break label;
      }
    }
  }
  assertEquals('outer', x);
}


with ({x: 'outer'}) {
  for (var i = 0; i < 10; ++i) {
    with ({x: 'inner' + i}) {
      continue;
    }
  }
  assertEquals('outer', x);
}


with ({x: 'outer'}) {
  label: for (var i = 0; i < 10; ++i) {
    with ({x: 'middle' + i}) {
      for (var j = 0; j < 10; ++j) {
        with ({x: 'inner' + j}) {
          continue label;
        }
      }
    }
  }
  assertEquals('outer', x);
}


with ({x: 'outer'}) {
  try {
    with ({x: 'inner'}) {
      throw 0;
    }
  } catch (e) {
    assertEquals('outer', x);
  }
}


with ({x: 'outer'}) {
  try {
    with ({x: 'middle'}) {
      with ({x: 'inner'}) {
        throw 0;
      }
    }
  } catch (e) {
    assertEquals('outer', x);
  }
}


try {
  with ({x: 'outer'}) {
    try {
      with ({x: 'inner'}) {
        throw 0;
      }
    } finally {
      assertEquals('outer', x);
    }
  }
} catch (e) {
  if (e instanceof MjsUnitAssertionError) throw e;
}


try {
  with ({x: 'outer'}) {
    try {
      with ({x: 'middle'}) {
        with ({x: 'inner'}) {
          throw 0;
        }
      }
    } finally {
      assertEquals('outer', x);
    }
  }
} catch (e) {
  if (e instanceof MjsUnitAssertionError) throw e;
}


// Verify that the context is correctly set in the stack frame after exiting
// from with.
function f() {}

with ({x: 'outer'}) {
  label: {
    with ({x: 'inner'}) {
      break label;
    }
  }
  f();  // The context could be restored from the stack after the call.
  assertEquals('outer', x);
}


with ({x: 'outer'}) {
  for (var i = 0; i < 10; ++i) {
    with ({x: 'inner' + i}) {
      continue;
    }
  }
  f();
  assertEquals('outer', x);
}


with ({x: 'outer'}) {
  try {
    with ({x: 'inner'}) {
      throw 0;
    }
  } catch (e) {
    f();
    assertEquals('outer', x);
  }
}


try {
  with ({x: 'outer'}) {
    try {
      with ({x: 'inner'}) {
        throw 0;
      }
    } finally {
      f();
      assertEquals('outer', x);
    }
  }
} catch (e) {
  if (e instanceof MjsUnitAssertionError) throw e;
}
