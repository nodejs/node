// Copyright 2013 the V8 project authors. All rights reserved.
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

// Flags: --harmony-generators --expose-gc

// Test generator iteration.

var GeneratorFunction = (function*(){yield 1;}).__proto__.constructor;

function assertIteratorResult(value, done, result) {
  assertEquals({ value: value, done: done}, result);
}

function TestGeneratorResultPrototype() {
  function* g() { yield 1; }
  var iter = g();
  var result = iter.next();

  assertSame(Object.prototype, Object.getPrototypeOf(result));
  property_names = Object.getOwnPropertyNames(result);
  property_names.sort();
  assertEquals(["done", "value"], property_names);
  assertIteratorResult(1, false, result);
}
TestGeneratorResultPrototype()

function TestGenerator(g, expected_values_for_next,
                       send_val, expected_values_for_send) {
  function testNext(thunk) {
    var iter = thunk();
    for (var i = 0; i < expected_values_for_next.length; i++) {
      assertIteratorResult(expected_values_for_next[i],
                           i == expected_values_for_next.length - 1,
                           iter.next());
    }
    assertThrows(function() { iter.next(); }, Error);
  }
  function testSend(thunk) {
    var iter = thunk();
    for (var i = 0; i < expected_values_for_send.length; i++) {
      assertIteratorResult(expected_values_for_send[i],
                           i == expected_values_for_send.length - 1,
                           iter.send(send_val));
    }
    assertThrows(function() { iter.send(send_val); }, Error);
  }
  function testThrow(thunk) {
    for (var i = 0; i < expected_values_for_next.length; i++) {
      var iter = thunk();
      for (var j = 0; j < i; j++) {
        assertIteratorResult(expected_values_for_next[j],
                             j == expected_values_for_next.length - 1,
                             iter.next());
      }
      function Sentinel() {}
      assertThrows(function () { iter.throw(new Sentinel); }, Sentinel);
      assertThrows(function () { iter.next(); }, Error);
    }
  }

  testNext(g);
  testSend(g);
  testThrow(g);

  if (g instanceof GeneratorFunction) {
    testNext(function() { return new g(); });
    testSend(function() { return new g(); });
    testThrow(function() { return new g(); });
  }
}

TestGenerator(function* g1() { },
              [undefined],
              "foo",
              [undefined]);

TestGenerator(function* g2() { yield 1; },
              [1, undefined],
              "foo",
              [1, undefined]);

TestGenerator(function* g3() { yield 1; yield 2; },
              [1, 2, undefined],
              "foo",
              [1, 2, undefined]);

TestGenerator(function* g4() { yield 1; yield 2; return 3; },
              [1, 2, 3],
              "foo",
              [1, 2, 3]);

TestGenerator(function* g5() { return 1; },
              [1],
             "foo",
              [1]);

TestGenerator(function* g6() { var x = yield 1; return x; },
              [1, undefined],
              "foo",
              [1, "foo"]);

TestGenerator(function* g7() { var x = yield 1; yield 2; return x; },
              [1, 2, undefined],
              "foo",
              [1, 2, "foo"]);

TestGenerator(function* g8() { for (var x = 0; x < 4; x++) { yield x; } },
              [0, 1, 2, 3, undefined],
              "foo",
              [0, 1, 2, 3, undefined]);

// Generator with arguments.
TestGenerator(
    function g9() {
      return (function*(a, b, c, d) {
        yield a; yield b; yield c; yield d;
      })("fee", "fi", "fo", "fum");
    },
    ["fee", "fi", "fo", "fum", undefined],
    "foo",
    ["fee", "fi", "fo", "fum", undefined]);

// Too few arguments.
TestGenerator(
    function g10() {
      return (function*(a, b, c, d) {
        yield a; yield b; yield c; yield d;
      })("fee", "fi");
    },
    ["fee", "fi", undefined, undefined, undefined],
    "foo",
    ["fee", "fi", undefined, undefined, undefined]);

// Too many arguments.
TestGenerator(
    function g11() {
      return (function*(a, b, c, d) {
        yield a; yield b; yield c; yield d;
      })("fee", "fi", "fo", "fum", "I smell the blood of an Englishman");
    },
    ["fee", "fi", "fo", "fum", undefined],
    "foo",
    ["fee", "fi", "fo", "fum", undefined]);

// The arguments object.
TestGenerator(
    function g12() {
      return (function*(a, b, c, d) {
        for (var i = 0; i < arguments.length; i++) {
          yield arguments[i];
        }
      })("fee", "fi", "fo", "fum", "I smell the blood of an Englishman");
    },
    ["fee", "fi", "fo", "fum", "I smell the blood of an Englishman",
     undefined],
    "foo",
    ["fee", "fi", "fo", "fum", "I smell the blood of an Englishman",
     undefined]);

// Access to captured free variables.
TestGenerator(
    function g13() {
      return (function(a, b, c, d) {
        return (function*() {
          yield a; yield b; yield c; yield d;
        })();
      })("fee", "fi", "fo", "fum");
    },
    ["fee", "fi", "fo", "fum", undefined],
    "foo",
    ["fee", "fi", "fo", "fum", undefined]);

// Abusing the arguments object.
TestGenerator(
    function g14() {
      return (function*(a, b, c, d) {
        arguments[0] = "Be he live";
        arguments[1] = "or be he dead";
        arguments[2] = "I'll grind his bones";
        arguments[3] = "to make my bread";
        yield a; yield b; yield c; yield d;
      })("fee", "fi", "fo", "fum");
    },
    ["Be he live", "or be he dead", "I'll grind his bones", "to make my bread",
     undefined],
    "foo",
    ["Be he live", "or be he dead", "I'll grind his bones", "to make my bread",
     undefined]);

// Abusing the arguments object: strict mode.
TestGenerator(
    function g15() {
      return (function*(a, b, c, d) {
        "use strict";
        arguments[0] = "Be he live";
        arguments[1] = "or be he dead";
        arguments[2] = "I'll grind his bones";
        arguments[3] = "to make my bread";
        yield a; yield b; yield c; yield d;
      })("fee", "fi", "fo", "fum");
    },
    ["fee", "fi", "fo", "fum", undefined],
    "foo",
    ["fee", "fi", "fo", "fum", undefined]);

// GC.
TestGenerator(function* g16() { yield "baz"; gc(); yield "qux"; },
              ["baz", "qux", undefined],
              "foo",
              ["baz", "qux", undefined]);

// Receivers.
TestGenerator(
    function g17() {
      function* g() { yield this.x; yield this.y; }
      var o = { start: g, x: 1, y: 2 };
      return o.start();
    },
    [1, 2, undefined],
    "foo",
    [1, 2, undefined]);

TestGenerator(
    function g18() {
      function* g() { yield this.x; yield this.y; }
      var iter = new g;
      iter.x = 1;
      iter.y = 2;
      return iter;
    },
    [1, 2, undefined],
    "foo",
    [1, 2, undefined]);

TestGenerator(
    function* g19() {
      var x = 1;
      yield x;
      with({x:2}) { yield x; }
      yield x;
    },
    [1, 2, 1, undefined],
    "foo",
    [1, 2, 1, undefined]);

TestGenerator(
    function* g20() { yield (1 + (yield 2) + 3); },
    [2, NaN, undefined],
    "foo",
    [2, "1foo3", undefined]);

TestGenerator(
    function* g21() { return (1 + (yield 2) + 3); },
    [2, NaN],
    "foo",
    [2, "1foo3"]);

TestGenerator(
    function* g22() { yield (1 + (yield 2) + 3); yield (4 + (yield 5) + 6); },
    [2, NaN, 5, NaN, undefined],
    "foo",
    [2, "1foo3", 5, "4foo6", undefined]);

TestGenerator(
    function* g23() {
      return (yield (1 + (yield 2) + 3)) + (yield (4 + (yield 5) + 6));
    },
    [2, NaN, 5, NaN, NaN],
    "foo",
    [2, "1foo3", 5, "4foo6", "foofoo"]);

// Rewind a try context with and without operands on the stack.
TestGenerator(
    function* g24() {
      try {
        return (yield (1 + (yield 2) + 3)) + (yield (4 + (yield 5) + 6));
      } catch (e) {
        throw e;
      }
    },
    [2, NaN, 5, NaN, NaN],
    "foo",
    [2, "1foo3", 5, "4foo6", "foofoo"]);

// Yielding in a catch context, with and without operands on the stack.
TestGenerator(
    function* g25() {
      try {
        throw (yield (1 + (yield 2) + 3))
      } catch (e) {
        if (typeof e == 'object') throw e;
        return e + (yield (4 + (yield 5) + 6));
      }
    },
    [2, NaN, 5, NaN, NaN],
    "foo",
    [2, "1foo3", 5, "4foo6", "foofoo"]);

function TestTryCatch() {
  function* g() { yield 1; try { yield 2; } catch (e) { yield e; } yield 3; }
  function Sentinel() {}
  var iter;

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(3, false, iter.next());
  assertIteratorResult(undefined, true, iter.next());
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertThrows(function() { iter.throw(new Sentinel); }, Sentinel);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertThrows(function() { iter.throw(new Sentinel); }, Sentinel);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  var exn = new Sentinel;
  assertIteratorResult(exn, false, iter.throw(exn));
  assertIteratorResult(3, false, iter.next());
  assertIteratorResult(undefined, true, iter.next());
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  var exn = new Sentinel;
  assertIteratorResult(exn, false, iter.throw(exn));
  assertIteratorResult(3, false, iter.next());
  assertThrows(function() { iter.throw(new Sentinel); }, Sentinel);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  var exn = new Sentinel;
  assertIteratorResult(exn, false, iter.throw(exn));
  assertThrows(function() { iter.throw(new Sentinel); }, Sentinel);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(3, false, iter.next());
  assertIteratorResult(undefined, true, iter.next());
  assertThrows(function() { iter.next(); }, Error);
}
TestTryCatch();

function TestTryFinally() {
  function* g() { yield 1; try { yield 2; } finally { yield 3; } yield 4; }
  function Sentinel() {}
  function Sentinel2() {}
  var iter;

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(3, false, iter.next());
  assertIteratorResult(4, false, iter.next());
  assertIteratorResult(undefined, true, iter.next());
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertThrows(function() { iter.throw(new Sentinel); }, Sentinel);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertThrows(function() { iter.throw(new Sentinel); }, Sentinel);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(3, false, iter.throw(new Sentinel));
  assertThrows(function() { iter.next(); }, Sentinel);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(3, false, iter.throw(new Sentinel));
  assertThrows(function() { iter.throw(new Sentinel2); }, Sentinel2);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(3, false, iter.next());
  assertThrows(function() { iter.throw(new Sentinel); }, Sentinel);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(3, false, iter.next());
  assertIteratorResult(4, false, iter.next());
  assertThrows(function() { iter.throw(new Sentinel); }, Sentinel);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(3, false, iter.next());
  assertIteratorResult(4, false, iter.next());
  assertIteratorResult(undefined, true, iter.next());
  assertThrows(function() { iter.next(); }, Error);
}
TestTryFinally();

function TestNestedTry() {
  function* g() {
    try {
      yield 1;
      try { yield 2; } catch (e) { yield e; }
      yield 3;
    } finally {
      yield 4;
    }
    yield 5;
  }
  function Sentinel() {}
  function Sentinel2() {}
  var iter;

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(3, false, iter.next());
  assertIteratorResult(4, false, iter.next());
  assertIteratorResult(5, false, iter.next());
  assertIteratorResult(undefined, true, iter.next());
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertThrows(function() { iter.throw(new Sentinel); }, Sentinel);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(4, false, iter.throw(new Sentinel));
  assertThrows(function() { iter.next(); }, Sentinel);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(4, false, iter.throw(new Sentinel));
  assertThrows(function() { iter.throw(new Sentinel2); }, Sentinel2);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  var exn = new Sentinel;
  assertIteratorResult(exn, false, iter.throw(exn));
  assertIteratorResult(3, false, iter.next());
  assertIteratorResult(4, false, iter.next());
  assertIteratorResult(5, false, iter.next());
  assertIteratorResult(undefined, true, iter.next());
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  var exn = new Sentinel;
  assertIteratorResult(exn, false, iter.throw(exn));
  assertIteratorResult(4, false, iter.throw(new Sentinel2));
  assertThrows(function() { iter.next(); }, Sentinel2);
  assertThrows(function() { iter.next(); }, Error);

  iter = g();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  var exn = new Sentinel;
  assertIteratorResult(exn, false, iter.throw(exn));
  assertIteratorResult(3, false, iter.next());
  assertIteratorResult(4, false, iter.throw(new Sentinel2));
  assertThrows(function() { iter.next(); }, Sentinel2);
  assertThrows(function() { iter.next(); }, Error);

  // That's probably enough.
}
TestNestedTry();

function TestRecursion() {
  function TestNextRecursion() {
    function* g() { yield iter.next(); }
    var iter = g();
    return iter.next();
  }
  function TestSendRecursion() {
    function* g() { yield iter.send(42); }
    var iter = g();
    return iter.next();
  }
  function TestThrowRecursion() {
    function* g() { yield iter.throw(1); }
    var iter = g();
    return iter.next();
  }
  assertThrows(TestNextRecursion, Error);
  assertThrows(TestSendRecursion, Error);
  assertThrows(TestThrowRecursion, Error);
}
TestRecursion();
