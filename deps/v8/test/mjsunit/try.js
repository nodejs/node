// Copyright 2008 the V8 project authors. All rights reserved.
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

// Flags: --expose-gc

function Catch(f, g) {
  var r;
  try { r = f(); } catch (o) { return g(o); }
  return r;
}

function CatchReturn(f, g) {
  try { return f(); } catch (o) { return g(o); }
}


var a = [Catch, CatchReturn]
for (var n in a) {
  var c = a[n];
  assertEquals(1, c(function() { return 1; }));
  assertEquals('bar', c(function() { return 'bar'; }));
  assertEquals(1, c(function () { throw 1; }, function (x) { return x; }));
  assertEquals('bar', c(function () { throw 'bar'; }, function (x) { return x; }));
}


assertEquals(1, (function() { try { return 1; } finally { } })());
assertEquals(1, (function() { try { return 1; } finally { var x = 12; } })());
assertEquals(2, (function() { try { } finally { return 2; } })());
assertEquals(4, (function() { try { return 3; } finally { return 4; } })());

function f(x, n, v) { try { return x; } finally { x[n] = v; } }
assertEquals(2, f({}, 'foo', 2).foo);
assertEquals(5, f({}, 'bar', 5).bar);

function guard(f) { try { f(); } catch (o) { return o; } }
assertEquals('baz', guard(function() { throw 'baz'; }));
assertEquals(2, (function() { try { throw {}; } catch(e) {} finally { return 2; } })());
assertEquals(1, guard(function() { try { throw 1; } finally { } }));
assertEquals(2, guard(function() { try { throw 2; } finally { var x = 12; } }));
assertEquals(4, guard(function() { try { throw 3; } finally { throw 4; } }));

(function () {
  var iter = 1000000;
  for (var i = 1; i <= iter; i++) {
    try {
      if (i == iter) gc();
    } finally {
      if (i == iter) gc();
    }
  }
})();

function trycatch(a) {
  var o;
  try {
    throw 1;
  } catch (o) {
    a.push(o);
    try {
      throw 2;
    } catch (o) {
      a.push(o);
    }
    a.push(o);
  }
  a.push(o);
}
var a = [];
trycatch(a);
assertEquals(4, a.length);
assertEquals(1, a[0], "a[0]");
assertEquals(2, a[1], "a[1]");

assertEquals(1, a[2], "a[2]");
assertTrue(typeof a[3] === 'undefined', "a[3]");

assertTrue(typeof o === 'undefined', "global.o");


function return_from_nested_catch(x) {
  try {
    try {
      return x;
    } catch (o) {
      return -1;
    }
  } catch (o) {
    return -2;
  }
}

assertEquals(0, return_from_nested_catch(0));
assertEquals(1, return_from_nested_catch(1));


function return_from_nested_finally(x) {
  var a = [x-2];
  try {
    try {
      return a;
    } finally {
      a[0]++;
    }
  } finally {
    a[0]++;
  }
}

assertEquals(0, return_from_nested_finally(0)[0]);
assertEquals(1, return_from_nested_finally(1)[0]);


function break_from_catch(x) {
  x--;
 L:
  {
    try {
      x++;
      if (false) return -1;
      break L;
    } catch (o) {
      x--;
    }
  }
  return x;
}

assertEquals(0, break_from_catch(0));
assertEquals(1, break_from_catch(1));


function break_from_finally(x) {
 L:
  {
    try {
      x++;
      if (false) return -1;
      break L;
    } finally {
      x--;
    }
    x--;
  }
  return x;
}

assertEquals(0, break_from_finally(0), "break from finally");
assertEquals(1, break_from_finally(1), "break from finally");


function continue_from_catch(x) {
  x--;
  var cont = true;
  while (cont) {
    try {
      x++;
      if (false) return -1;
      cont = false;
      continue;
    } catch (o) {
      x--;
    }
  }
  return x;
}

assertEquals(0, continue_from_catch(0));
assertEquals(1, continue_from_catch(1));


function continue_from_finally(x) {
  var cont = true;
  while (cont) {
    try {
      x++;
      if (false) return -1;
      cont = false;
      continue;
    } finally {
      x--;
    }
    x--;
  }
  return x;
}

assertEquals(0, continue_from_finally(0));
assertEquals(1, continue_from_finally(1));


function continue_alot_from_finally(x) {
  var j = 0;
  for (var i = 0; i < x;) {
    try {
      j++;
      continue;
      j++;  // should not happen
    } finally {
      i++; // must happen
    }
    j++; // should not happen
  }
  return j;
}


assertEquals(100, continue_alot_from_finally(100));
assertEquals(200, continue_alot_from_finally(200));



function break_from_nested_catch(x) {
  x -= 2;
 L:
  {
    try {
      x++;
      try {
        x++;
        if (false) return -1;
        break L;
      } catch (o) {
        x--;
      }
    } catch (o) {
      x--;
    }
  } 
  return x;
}

assertEquals(0, break_from_nested_catch(0));
assertEquals(1, break_from_nested_catch(1));


function break_from_nested_finally(x) {
 L:
  {
    try {
      x++;
      try {
        x++;
        if (false) return -1;
        break L;
      } finally {
        x--;
      }
    } finally {
      x--;
    }
    x--; // should not happen
  } 
  return x;
}

assertEquals(0, break_from_nested_finally(0));
assertEquals(1, break_from_nested_finally(1));


function continue_from_nested_catch(x) {
  x -= 2;
  var cont = true;
  while (cont) {
    try {
      x++;
      try {
        x++;
        if (false) return -1;
        cont = false;
        continue;
      } catch (o) {
        x--;
      }
    } catch (o) {
      x--;
    }
  }
  return x;
}

assertEquals(0, continue_from_nested_catch(0));
assertEquals(1, continue_from_nested_catch(1));


function continue_from_nested_finally(x) {
  var cont = true;
  while (cont) {
    try {
      x++;
      try {
        x++;
        if (false) return -1;
        cont = false;
        continue;
      } finally {
        x--;
      }
    } finally {
      x--;
    }
    x--;  // should not happen
  }
  return x;
}

assertEquals(0, continue_from_nested_finally(0));
assertEquals(1, continue_from_nested_finally(1));


var caught = false;
var finalized = false;
var broke = true;
L: try {
  break L;
  broke = false;
} catch (o) {
  caught = true;
} finally {
  finalized = true;
}
assertTrue(broke);
assertFalse(caught);
assertTrue(finalized);

function return_from_nested_finally_in_finally() {
  try {
    return 1;
  } finally {
    try {
      return 2;
    } finally {
      return 42;
    }
  }
}

assertEquals(42, return_from_nested_finally_in_finally());

function break_from_nested_finally_in_finally() {
  L: try {
    return 1;
  } finally {
    try {
      return 2;
    } finally {
      break L;
    }
  }
  return 42;
}

assertEquals(42, break_from_nested_finally_in_finally());

function continue_from_nested_finally_in_finally() {
  do {
    try {
      return 1;
    } finally {
      try {
        return 2;
      } finally {
        continue;
      }
    }
  } while (false);
  return 42;
}

assertEquals(42, continue_from_nested_finally_in_finally());
