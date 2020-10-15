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

// Flags: --allow-natives-syntax --ignore-unhandled-promises

// Make sure we don't rely on functions patchable by monkeys.
var call = Function.prototype.call.call.bind(Function.prototype.call)
var getOwnPropertyNames = Object.getOwnPropertyNames;
var defineProperty = Object.defineProperty;
var numberPrototype = Number.prototype;
var symbolIterator = Symbol.iterator;

function assertUnreachable() {
  %AbortJS("Failure: unreachable");
}

(function() {
  // Test before clearing global (fails otherwise)
  assertEquals("[object Promise]",
      Object.prototype.toString.call(new Promise(function() {})));
})();


function clear(o) {
  if (o === null || (typeof o !== 'object' && typeof o !== 'function')) return
  clear(o.__proto__)
  var properties = getOwnPropertyNames(o)
  for (var i in properties) {
    // Do not clobber Object.prototype.toString, which is used by tests.
    if (properties[i] === "toString") continue;
    clearProp(o, properties[i])
  }
}

function clearProp(o, name) {
  var poisoned = {caller: 0, callee: 0, arguments: 0}
  try {
    var x = o[name]
    o[name] = undefined
    clear(x)
  } catch(e) {} // assertTrue(name in poisoned) }
}

// Find intrinsics and null them out.
var globals = Object.getOwnPropertyNames(this)
var allowlist = {
  Promise: true,
  TypeError: true,
  String: true,
  JSON: true,
  Error: true,
  MjsUnitAssertionError: true
};

for (var i in globals) {
  var name = globals[i]
  if (name in allowlist || name[0] === name[0].toLowerCase()) delete globals[i]
}
for (var i in globals) {
  if (globals[i]) clearProp(this, globals[i])
}


function defer(constructor) {
  var resolve, reject;
  var promise = new constructor((res, rej) => { resolve = res; reject = rej });
  return { promise, resolve, reject };
}

var asyncAssertsExpected = 0;

function assertAsyncRan() { ++asyncAssertsExpected }

function assertAsync(b, s) {
  if (b) {
    print(s, "succeeded")
  } else {
    %AbortJS(s + " FAILED!")  // Simply throwing here will have no effect.
  }
  --asyncAssertsExpected
}

function assertLater(f, name) {
  assertFalse(f()); // should not be true synchronously
  ++asyncAssertsExpected;
  var iterations = 0;
  function runAssertion() {
    if (f()) {
      print(name, "succeeded");
      --asyncAssertsExpected;
    } else if (iterations++ < 10) {
      %EnqueueMicrotask(runAssertion);
    } else {
      %AbortJS(name + " FAILED!");
    }
  }
  %EnqueueMicrotask(runAssertion);
}

function assertAsyncDone(iteration) {
  var iteration = iteration || 0;
  %EnqueueMicrotask(function() {
    if (asyncAssertsExpected === 0)
      assertAsync(true, "all")
    else if (iteration > 10)  // Shouldn't take more.
      assertAsync(false, "all... " + asyncAssertsExpected)
    else
      assertAsyncDone(iteration + 1)
  });
}

(function() {
  assertThrows(function() { Promise(function() {}) }, TypeError)
})();

(function() {
  assertTrue(new Promise(function() {}) instanceof Promise)
})();

(function() {
  assertThrows(function() { new Promise(5) }, TypeError)
})();

(function() {
  assertDoesNotThrow(function() { new Promise(function() { throw 5 }) })
})();

(function() {
  (new Promise(function() { throw 5 })).then(
    assertUnreachable,
    function(r) { assertAsync(r === 5, "new-throw") }
  )
  assertAsyncRan()
})();

(function() {
  Promise.resolve(5);
  Promise.resolve(5).then(undefined, assertUnreachable).then(
    function(x) { assertAsync(x === 5, "resolved/then-nohandler") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  Promise.resolve(5).then(undefined, assertUnreachable).then(
    function(x) { assertAsync(x === 5, "resolved/then-nohandler-undefined") },
    assertUnreachable
  )
  assertAsyncRan()
  Promise.resolve(6).then(null, assertUnreachable).then(
    function(x) { assertAsync(x === 6, "resolved/then-nohandler-null") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(
    function(x) { assertAsync(x === 5, "resolved/then") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.reject(5)
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "rejected/then") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(function(x) { return x }, assertUnreachable).then(
    function(x) { assertAsync(x === 5, "resolved/then/then") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(function(x){ return Promise.resolve(x+1) }, assertUnreachable).then(
    function(x) { assertAsync(x === 6, "resolved/then/then2") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(function(x) { throw 6 }, assertUnreachable).then(
    assertUnreachable,
    function(x) { assertAsync(x === 6, "resolved/then-throw/then") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(function(x) { throw 6 }, assertUnreachable).then(
    assertUnreachable,
    function(x) { assertAsync(x === 6, "resolved/then-throw/then") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(function(x) { throw 6 }, assertUnreachable).then(
    assertUnreachable,
    function(x) { assertAsync(x === 6, "resolved/then-throw/then") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(function(x) { throw 6 }, assertUnreachable).then(
    assertUnreachable,
    function(x) { assertAsync(x === 6, "resolved/then-throw/then") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.resolve(p2)
  p3.then(
    function(x) { assertAsync(x === 5, "resolved/thenable/then") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.resolve(p2)
  p3.then(
    function(x) { assertAsync(x === 5, "resolved/thenable/then") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.reject(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.resolve(p2)
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "rejected/thenable/then") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.reject(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.resolve(p2)
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "rejected/thenable/then") }
  )
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve") },
    assertUnreachable
  )
  deferred.resolve(5)
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve") },
    assertUnreachable
  )
  deferred.resolve(5)
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "then/reject") }
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = Promise.resolve(p1)
  var p3 = Promise.resolve(p2)
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "then/reject") }
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = p1.then(1, 2)
  p2.then(
    function(x) { assertAsync(x === 5, "then/resolve-non-function") },
    assertUnreachable
  )
  deferred.resolve(5)
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = p1.then(1, 2)
  p2.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "then/reject-non-function") }
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.resolve(p2)
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve/thenable") },
    assertUnreachable
  )
  deferred.resolve(5)
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.resolve(p2)
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve/thenable") },
    assertUnreachable
  )
  deferred.resolve(5)
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.resolve(p2)
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "then/reject/thenable") }
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.resolve(p2)
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "then/reject/thenable") }
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = Promise.resolve(p1)
  var deferred = defer(Promise)
  var p3 = deferred.promise
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve2") },
    assertUnreachable
  )
  deferred.resolve(p2)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = Promise.resolve(p1)
  var deferred = defer(Promise)
  var p3 = deferred.promise
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve2") },
    assertUnreachable
  )
  deferred.resolve(p2)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = Promise.resolve(p1)
  var deferred = defer(Promise)
  var p3 = deferred.promise
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "then/reject2") }
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = Promise.resolve(p1)
  var deferred = defer(Promise)
  var p3 = deferred.promise
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "then/reject2") }
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var deferred = defer(Promise)
  var p3 = deferred.promise
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve/thenable2") },
    assertUnreachable
  )
  deferred.resolve(p2)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var deferred = defer(Promise)
  var p3 = deferred.promise
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve/thenable2") },
    assertUnreachable
  )
  deferred.resolve(p2)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(0)
  var p2 = p1.then(function(x) { return p2 }, assertUnreachable)
  p2.then(
    assertUnreachable,
    function(r) { assertAsync(r instanceof TypeError, "cyclic/then") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(0)
  var p2 = p1.then(function(x) { return p2 }, assertUnreachable)
  p2.then(
    assertUnreachable,
    function(r) { assertAsync(r instanceof TypeError, "cyclic/then") }
  )
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p = deferred.promise
  deferred.resolve(p)
  p.then(
    assertUnreachable,
    function(r) { assertAsync(r instanceof TypeError, "cyclic/deferred/then") }
  )
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p = deferred.promise
  deferred.resolve(p)
  p.then(
    assertUnreachable,
    function(r) { assertAsync(r instanceof TypeError, "cyclic/deferred/then") }
  )
  assertAsyncRan()
})();

(function() {
  Promise.all([]).then(
    function(x) { assertAsync(x.length === 0, "all/resolve/empty") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  function testPromiseAllNonIterable(value) {
    Promise.all(value).then(
        assertUnreachable,
        function(r) {
          assertAsync(r instanceof TypeError, 'all/non iterable');
        });
    assertAsyncRan();
  }
  testPromiseAllNonIterable(null);
  testPromiseAllNonIterable(undefined);
  testPromiseAllNonIterable({});
  testPromiseAllNonIterable(42);
})();

(function() {
  Promise.all({[symbolIterator](){ return null; }}).then(
    assertUnreachable,
    function(r) {
      assertAsync(r instanceof TypeError, 'all/non iterable');
    });
  assertAsyncRan();
})();

(function() {
  var deferred = defer(Promise);
  var p = deferred.promise;
  function* f() {
    yield 1;
    yield p;
    yield 3;
  }
  Promise.all(f()).then(
      function(x) {
        assertAsync(x.length === 3, "all/resolve/iterable");
        assertAsync(x[0] === 1, "all/resolve/iterable/0");
        assertAsync(x[1] === 2, "all/resolve/iterable/1");
        assertAsync(x[2] === 3, "all/resolve/iterable/2");
      },
      assertUnreachable);
  deferred.resolve(2);
  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
})();


(function() {
  var deferred1 = defer(Promise)
  var p1 = deferred1.promise
  var deferred2 = defer(Promise)
  var p2 = deferred2.promise
  var deferred3 = defer(Promise)
  var p3 = deferred3.promise
  Promise.all([p1, p2, p3]).then(
    function(x) {
      assertAsync(x.length === 3, "all/resolve")
      assertAsync(x[0] === 1, "all/resolve/0")
      assertAsync(x[1] === 2, "all/resolve/1")
      assertAsync(x[2] === 3, "all/resolve/2")
    },
    assertUnreachable
  )
  deferred1.resolve(1)
  deferred3.resolve(3)
  deferred2.resolve(2)
  assertAsyncRan()
  assertAsyncRan()
  assertAsyncRan()
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = Promise.resolve(2)
  var p3 = defer(Promise).promise
  Promise.all([p1, p2, p3]).then(
    assertUnreachable,
    assertUnreachable
  )
  deferred.resolve(1)
})();

(function() {
  var deferred1 = defer(Promise)
  var p1 = deferred1.promise
  var deferred2 = defer(Promise)
  var p2 = deferred2.promise
  var deferred3 = defer(Promise)
  var p3 = deferred3.promise
  Promise.all([p1, p2, p3]).then(
    assertUnreachable,
    function(x) { assertAsync(x === 2, "all/reject") }
  )
  deferred1.resolve(1)
  deferred3.resolve(3)
  deferred2.reject(2)
  assertAsyncRan()
})();

(function() {
  'use strict';
  var getCalls = 0;
  var funcCalls = 0;
  var nextCalls = 0;
  defineProperty(numberPrototype, symbolIterator, {
    get: function() {
      assertEquals('number', typeof this);
      getCalls++;
      return function() {
        assertEquals('number', typeof this);
        funcCalls++;
        var n = this;
        var i = 0
        return {
          next() {
            nextCalls++;
            return {value: i++, done: i > n};
          }
        };
      };
    },
    configurable: true
  });

  Promise.all(3).then(
      function(x) {
        assertAsync(x.length === 3, "all/iterable/number/length");
        assertAsync(x[0] === 0, "all/iterable/number/0");
        assertAsync(x[1] === 1, "all/iterable/number/1");
        assertAsync(x[2] === 2, "all/iterable/number/2");
      },
      assertUnreachable);
  delete numberPrototype[symbolIterator];

  assertEquals(getCalls, 1);
  assertEquals(funcCalls, 1);
  assertEquals(nextCalls, 3 + 1);  // + 1 for {done: true}
  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
})();


(function() {
  Promise.race([]).then(
    assertUnreachable,
    assertUnreachable
  )
})();

(function() {
  var p1 = Promise.resolve(1)
  var p2 = Promise.resolve(2)
  var p3 = Promise.resolve(3)
  Promise.race([p1, p2, p3]).then(
    function(x) { assertAsync(x === 1, "resolved/one") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.resolve(1)
  var p2 = Promise.resolve(2)
  var p3 = Promise.resolve(3)
  Promise.race([0, p1, p2, p3]).then(
    function(x) { assertAsync(x === 0, "resolved-const/one") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var deferred1 = defer(Promise)
  var p1 = deferred1.promise
  var deferred2 = defer(Promise)
  var p2 = deferred2.promise
  var deferred3 = defer(Promise)
  var p3 = deferred3.promise
  Promise.race([p1, p2, p3]).then(
    function(x) { assertAsync(x === 3, "one/resolve") },
    assertUnreachable
  )
  deferred3.resolve(3)
  deferred1.resolve(1)
  assertAsyncRan()
})();

(function() {
  var deferred = defer(Promise)
  var p1 = deferred.promise
  var p2 = Promise.resolve(2)
  var p3 = defer(Promise).promise
  Promise.race([p1, p2, p3]).then(
    function(x) { assertAsync(x === 2, "resolved/one") },
    assertUnreachable
  )
  deferred.resolve(1)
  assertAsyncRan()
})();

(function() {
  var deferred1 = defer(Promise)
  var p1 = deferred1.promise
  var deferred2 = defer(Promise)
  var p2 = deferred2.promise
  var deferred3 = defer(Promise)
  var p3 = deferred3.promise
  Promise.race([p1, p2, p3]).then(
    function(x) { assertAsync(x === 3, "one/resolve/reject") },
    assertUnreachable
  )
  deferred3.resolve(3)
  deferred1.reject(1)
  assertAsyncRan()
})();

(function() {
  var deferred1 = defer(Promise)
  var p1 = deferred1.promise
  var deferred2 = defer(Promise)
  var p2 = deferred2.promise
  var deferred3 = defer(Promise)
  var p3 = deferred3.promise
  Promise.race([p1, p2, p3]).then(
    assertUnreachable,
    function(x) { assertAsync(x === 3, "one/reject/resolve") }
  )
  deferred3.reject(3)
  deferred1.resolve(1)
  assertAsyncRan()
})();


(function() {
  function testPromiseRaceNonIterable(value) {
    Promise.race(value).then(
        assertUnreachable,
        function(r) {
          assertAsync(r instanceof TypeError, 'race/non iterable');
        });
    assertAsyncRan();
  }
  testPromiseRaceNonIterable(null);
  testPromiseRaceNonIterable(undefined);
  testPromiseRaceNonIterable({});
  testPromiseRaceNonIterable(42);
})();


(function() {
  var deferred1 = defer(Promise)
  var p1 = deferred1.promise
  var deferred2 = defer(Promise)
  var p2 = deferred2.promise
  var deferred3 = defer(Promise)
  var p3 = deferred3.promise
  function* f() {
    yield p1;
    yield p2;
    yield p3;
  }
  Promise.race(f()).then(
    function(x) { assertAsync(x === 3, "race/iterable/resolve/reject") },
    assertUnreachable
  )
  deferred3.resolve(3)
  deferred1.reject(1)
  assertAsyncRan()
})();

(function() {
  var deferred1 = defer(Promise)
  var p1 = deferred1.promise
  var deferred2 = defer(Promise)
  var p2 = deferred2.promise
  var deferred3 = defer(Promise)
  var p3 = deferred3.promise
  function* f() {
    yield p1;
    yield p2;
    yield p3;
  }
  Promise.race(f()).then(
    assertUnreachable,
    function(x) { assertAsync(x === 3, "race/iterable/reject/resolve") }
  )
  deferred3.reject(3)
  deferred1.resolve(1)
  assertAsyncRan()
})();

(function() {
  'use strict';
  var getCalls = 0;
  var funcCalls = 0;
  var nextCalls = 0;
  defineProperty(numberPrototype, symbolIterator, {
    get: function() {
      assertEquals('number', typeof this);
      getCalls++;
      return function() {
        assertEquals('number', typeof this);
        funcCalls++;
        var n = this;
        var i = 0
        return {
          next() {
            nextCalls++;
            return {value: i++, done: i > n};
          }
        };
      };
    },
    configurable: true
  });

  Promise.race(3).then(
      function(x) {
        assertAsync(x === 0, "race/iterable/number");
      },
      assertUnreachable);
  delete numberPrototype[symbolIterator];

  assertEquals(getCalls, 1);
  assertEquals(funcCalls, 1);
  assertEquals(nextCalls, 3 + 1);  // + 1 for {done: true}
  assertAsyncRan();
})();

(function() {
  var log
  function MyPromise(resolver) {
    log += "n"
    var promise = new Promise(function(resolve, reject) {
      resolver(
        function(x) { log += "x" + x; resolve(x) },
        function(r) { log += "r" + r; reject(r) }
      )
    })
    promise.__proto__ = MyPromise.prototype
    return promise
  }

  MyPromise.__proto__ = Promise
  MyPromise.defer = function() {
    log += "d"
    return call(this.__proto__.defer, this)
  }

  MyPromise.prototype.__proto__ = Promise.prototype
  MyPromise.prototype.then = function(resolve, reject) {
    log += "c"
    return call(this.__proto__.__proto__.then, this, resolve, reject)
  }

  log = ""
  var p1 = new MyPromise(function(resolve, reject) { resolve(1) })
  var p2 = new MyPromise(function(resolve, reject) { reject(2) })
  var d3 = defer(MyPromise)
  assertTrue(d3.promise instanceof Promise, "subclass/instance")
  assertTrue(d3.promise instanceof MyPromise, "subclass/instance-my3")
  assertTrue(log === "nx1nr2n", "subclass/create")

  log = ""
  var p4 = MyPromise.resolve(4)
  var p5 = MyPromise.reject(5)
  assertTrue(p4 instanceof MyPromise, "subclass/instance4")
  assertTrue(p4 instanceof MyPromise, "subclass/instance-my4")
  assertTrue(p5 instanceof MyPromise, "subclass/instance5")
  assertTrue(p5 instanceof MyPromise, "subclass/instance-my5")
  d3.resolve(3)
  assertTrue(log === "nx4nr5x3", "subclass/resolve")

  log = ""
  var d6 = defer(MyPromise)
  d6.promise.then(function(x) {
    return new Promise(function(resolve) { resolve(x) })
  }).then(function() {})
  d6.resolve(6)
  assertTrue(log === "ncncnx6", "subclass/then")

  log = ""
  Promise.all([11, Promise.resolve(12), 13, MyPromise.resolve(14), 15, 16])

  assertTrue(log === "nx14", "subclass/all/arg")

  log = ""
  MyPromise.all([21, Promise.resolve(22), 23, MyPromise.resolve(24), 25, 26])
  assertTrue(log === "nx24nnx21cnnx[object Promise]cnnx23cncnnx25cnnx26cn",
             "subclass/all/self")
})();

(function() {
  'use strict';

  class Pact extends Promise { }
  class Vow  extends Pact    { }
  class Oath extends Vow     { }

  Oath.constructor = Vow;

  assertTrue(Pact.resolve(Pact.resolve()).constructor === Pact,
             "subclass/resolve/own");

  assertTrue(Pact.resolve(Promise.resolve()).constructor === Pact,
             "subclass/resolve/ancestor");

  assertTrue(Pact.resolve(Vow.resolve()).constructor === Pact,
             "subclass/resolve/descendant"); var vow = Vow.resolve();

  vow.constructor = Oath;
  assertTrue(Oath.resolve(vow) === vow,
             "subclass/resolve/descendant with transplanted own constructor");
}());

(function() {
  var thenCalled = false;

  var resolve;
  var promise = new Promise(function(res) { resolve = res; });
  resolve({ then() { thenCalled = true; throw new Error(); } });
  assertLater(function() { return thenCalled; }, "resolve-with-thenable");
})();

(function() {
  var calledWith;

  var resolve;
  var p1 = (new Promise(function(res) { resolve = res; }));
  var p2 = p1.then(function(v) {
    return {
      then(resolve, reject) { resolve({ then() { calledWith = v }}); }
    };
  });

  resolve({ then(resolve) { resolve(2); } });
  assertLater(function() { return calledWith === 2; },
              "resolve-with-thenable2");
})();

(function() {
  var p = Promise.resolve();
  var callCount = 0;
  defineProperty(p, "constructor", {
    get: function() { ++callCount; return Promise; }
  });
  p.then();
  assertEquals(1, callCount);
})();

assertAsyncDone()
