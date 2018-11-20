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

// Flags: --allow-natives-syntax --harmony-tostring

// Make sure we don't rely on functions patchable by monkeys.
var call = Function.prototype.call.call.bind(Function.prototype.call)
var observe = Object.observe;
var getOwnPropertyNames = Object.getOwnPropertyNames;
var defineProperty = Object.defineProperty;


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
var whitelist = {Promise: true, TypeError: true}
for (var i in globals) {
  var name = globals[i]
  if (name in whitelist || name[0] === name[0].toLowerCase()) delete globals[i]
}
for (var i in globals) {
  if (globals[i]) clearProp(this, globals[i])
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

function assertAsyncDone(iteration) {
  var iteration = iteration || 0
  var dummy = {}
  observe(dummy,
    function() {
      if (asyncAssertsExpected === 0)
        assertAsync(true, "all")
      else if (iteration > 10)  // Shouldn't take more.
        assertAsync(false, "all")
      else
        assertAsyncDone(iteration + 1)
    }
  )
  dummy.dummy = dummy
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
  (new Promise(function() { throw 5 })).chain(
    assertUnreachable,
    function(r) { assertAsync(r === 5, "new-throw") }
  )
  assertAsyncRan()
})();

(function() {
  Promise.accept(5);
  Promise.accept(5).chain(undefined, assertUnreachable).chain(
    function(x) { assertAsync(x === 5, "resolved/chain-nohandler") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  Promise.reject(5).chain(assertUnreachable, undefined).chain(
    assertUnreachable,
    function(r) { assertAsync(r === 5, "rejected/chain-nohandler") }
  )
  assertAsyncRan()
})();

(function() {
  Promise.accept(5).then(undefined, assertUnreachable).chain(
    function(x) { assertAsync(x === 5, "resolved/then-nohandler-undefined") },
    assertUnreachable
  )
  assertAsyncRan()
  Promise.accept(6).then(null, assertUnreachable).chain(
    function(x) { assertAsync(x === 6, "resolved/then-nohandler-null") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  Promise.reject(5).then(assertUnreachable, undefined).chain(
    assertUnreachable,
    function(r) { assertAsync(r === 5, "rejected/then-nohandler-undefined") }
  )
  assertAsyncRan()
  Promise.reject(6).then(assertUnreachable, null).chain(
    assertUnreachable,
    function(r) { assertAsync(r === 6, "rejected/then-nohandler-null") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.chain(
    function(x) { assertAsync(x === p2, "resolved/chain") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.then(
    function(x) { assertAsync(x === 5, "resolved/then") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.reject(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.chain(
    function(x) { assertAsync(x === p2, "rejected/chain") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.reject(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "rejected/then") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.chain(function(x) { return x }, assertUnreachable).chain(
    function(x) { assertAsync(x === p1, "resolved/chain/chain") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.chain(function(x) { return x }, assertUnreachable).then(
    function(x) { assertAsync(x === 5, "resolved/chain/then") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.chain(function(x) { return 6 }, assertUnreachable).chain(
    function(x) { assertAsync(x === 6, "resolved/chain/chain2") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.chain(function(x) { return 6 }, assertUnreachable).then(
    function(x) { assertAsync(x === 6, "resolved/chain/then2") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.then(function(x) { return x + 1 }, assertUnreachable).chain(
    function(x) { assertAsync(x === 6, "resolved/then/chain") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.then(function(x) { return x + 1 }, assertUnreachable).then(
    function(x) { assertAsync(x === 6, "resolved/then/then") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.then(function(x){ return Promise.accept(x+1) }, assertUnreachable).chain(
    function(x) { assertAsync(x === 6, "resolved/then/chain2") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.then(function(x) { return Promise.accept(x+1) }, assertUnreachable).then(
    function(x) { assertAsync(x === 6, "resolved/then/then2") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.chain(function(x) { throw 6 }, assertUnreachable).chain(
    assertUnreachable,
    function(x) { assertAsync(x === 6, "resolved/chain-throw/chain") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.chain(function(x) { throw 6 }, assertUnreachable).then(
    assertUnreachable,
    function(x) { assertAsync(x === 6, "resolved/chain-throw/then") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.then(function(x) { throw 6 }, assertUnreachable).chain(
    assertUnreachable,
    function(x) { assertAsync(x === 6, "resolved/then-throw/chain") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.then(function(x) { throw 6 }, assertUnreachable).then(
    assertUnreachable,
    function(x) { assertAsync(x === 6, "resolved/then-throw/then") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.accept(p2)
  p3.chain(
    function(x) { assertAsync(x === p2, "resolved/thenable/chain") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.accept(p2)
  p3.then(
    function(x) { assertAsync(x === 5, "resolved/thenable/then") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.reject(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.accept(p2)
  p3.chain(
    function(x) { assertAsync(x === p2, "rejected/thenable/chain") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.reject(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.accept(p2)
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "rejected/thenable/then") }
  )
  assertAsyncRan()
})();

(function() {
  var deferred = Promise.defer()
  var p1 = deferred.promise
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.chain(
    function(x) { assertAsync(x === p2, "chain/resolve") },
    assertUnreachable
  )
  deferred.resolve(5)
  assertAsyncRan()
})();

(function() {
  var deferred = Promise.defer()
  var p1 = deferred.promise
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve") },
    assertUnreachable
  )
  deferred.resolve(5)
  assertAsyncRan()
})();

(function() {
  var deferred = Promise.defer()
  var p1 = deferred.promise
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.chain(
    function(x) { assertAsync(x === p2, "chain/reject") },
    assertUnreachable
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var deferred = Promise.defer()
  var p1 = deferred.promise
  var p2 = Promise.accept(p1)
  var p3 = Promise.accept(p2)
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "then/reject") }
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var deferred = Promise.defer()
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
  var deferred = Promise.defer()
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
  var deferred = Promise.defer()
  var p1 = deferred.promise
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.accept(p2)
  p3.chain(
    function(x) { assertAsync(x === p2, "chain/resolve/thenable") },
    assertUnreachable
  )
  deferred.resolve(5)
  assertAsyncRan()
})();

(function() {
  var deferred = Promise.defer()
  var p1 = deferred.promise
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.accept(p2)
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve/thenable") },
    assertUnreachable
  )
  deferred.resolve(5)
  assertAsyncRan()
})();

(function() {
  var deferred = Promise.defer()
  var p1 = deferred.promise
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.accept(p2)
  p3.chain(
    function(x) { assertAsync(x === p2, "chain/reject/thenable") },
    assertUnreachable
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var deferred = Promise.defer()
  var p1 = deferred.promise
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var p3 = Promise.accept(p2)
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "then/reject/thenable") }
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var deferred = Promise.defer()
  var p3 = deferred.promise
  p3.chain(
    function(x) { assertAsync(x === p2, "chain/resolve2") },
    assertUnreachable
  )
  deferred.resolve(p2)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var deferred = Promise.defer()
  var p3 = deferred.promise
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve2") },
    assertUnreachable
  )
  deferred.resolve(p2)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var deferred = Promise.defer()
  var p3 = deferred.promise
  p3.chain(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "chain/reject2") }
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = Promise.accept(p1)
  var deferred = Promise.defer()
  var p3 = deferred.promise
  p3.then(
    assertUnreachable,
    function(x) { assertAsync(x === 5, "then/reject2") }
  )
  deferred.reject(5)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var deferred = Promise.defer()
  var p3 = deferred.promise
  p3.chain(
    function(x) { assertAsync(x === p2, "chain/resolve/thenable2") },
    assertUnreachable
  )
  deferred.resolve(p2)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(5)
  var p2 = {then: function(onResolve, onReject) { onResolve(p1) }}
  var deferred = Promise.defer()
  var p3 = deferred.promise
  p3.then(
    function(x) { assertAsync(x === 5, "then/resolve/thenable2") },
    assertUnreachable
  )
  deferred.resolve(p2)
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(0)
  var p2 = p1.chain(function(x) { return p2 }, assertUnreachable)
  p2.chain(
    assertUnreachable,
    function(r) { assertAsync(r instanceof TypeError, "cyclic/chain") }
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(0)
  var p2 = p1.then(function(x) { return p2 }, assertUnreachable)
  p2.chain(
    assertUnreachable,
    function(r) { assertAsync(r instanceof TypeError, "cyclic/then") }
  )
  assertAsyncRan()
})();

(function() {
  var deferred = Promise.defer()
  var p = deferred.promise
  deferred.resolve(p)
  p.chain(
    function(x) { assertAsync(x === p, "cyclic/deferred/chain") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var deferred = Promise.defer()
  var p = deferred.promise
  deferred.resolve(p)
  p.then(
    assertUnreachable,
    function(r) { assertAsync(r instanceof TypeError, "cyclic/deferred/then") }
  )
  assertAsyncRan()
})();

(function() {
  Promise.all({}).chain(
    assertUnreachable,
    function(r) { assertAsync(r instanceof TypeError, "all/no-array") }
  )
  assertAsyncRan()
})();

(function() {
  Promise.all([]).chain(
    function(x) { assertAsync(x.length === 0, "all/resolve/empty") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var deferred1 = Promise.defer()
  var p1 = deferred1.promise
  var deferred2 = Promise.defer()
  var p2 = deferred2.promise
  var deferred3 = Promise.defer()
  var p3 = deferred3.promise
  Promise.all([p1, p2, p3]).chain(
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
  var deferred = Promise.defer()
  var p1 = deferred.promise
  var p2 = Promise.accept(2)
  var p3 = Promise.defer().promise
  Promise.all([p1, p2, p3]).chain(
    assertUnreachable,
    assertUnreachable
  )
  deferred.resolve(1)
})();

(function() {
  var deferred1 = Promise.defer()
  var p1 = deferred1.promise
  var deferred2 = Promise.defer()
  var p2 = deferred2.promise
  var deferred3 = Promise.defer()
  var p3 = deferred3.promise
  Promise.all([p1, p2, p3]).chain(
    assertUnreachable,
    function(x) { assertAsync(x === 2, "all/reject") }
  )
  deferred1.resolve(1)
  deferred3.resolve(3)
  deferred2.reject(2)
  assertAsyncRan()
})();

(function() {
  Promise.race([]).chain(
    assertUnreachable,
    assertUnreachable
  )
})();

(function() {
  var p1 = Promise.accept(1)
  var p2 = Promise.accept(2)
  var p3 = Promise.accept(3)
  Promise.race([p1, p2, p3]).chain(
    function(x) { assertAsync(x === 1, "resolved/one") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  var p1 = Promise.accept(1)
  var p2 = Promise.accept(2)
  var p3 = Promise.accept(3)
  Promise.race([0, p1, p2, p3]).chain(
    function(x) { assertAsync(x === 0, "resolved-const/one") },
    assertUnreachable
  )
  assertAsyncRan()
})();

(function() {
  Promise.race({}).chain(
    assertUnreachable,
    function(r) { assertAsync(r instanceof TypeError, "one/no-array") }
  )
  assertAsyncRan()
})();

(function() {
  var deferred1 = Promise.defer()
  var p1 = deferred1.promise
  var deferred2 = Promise.defer()
  var p2 = deferred2.promise
  var deferred3 = Promise.defer()
  var p3 = deferred3.promise
  Promise.race([p1, p2, p3]).chain(
    function(x) { assertAsync(x === 3, "one/resolve") },
    assertUnreachable
  )
  deferred3.resolve(3)
  deferred1.resolve(1)
  assertAsyncRan()
})();

(function() {
  var deferred = Promise.defer()
  var p1 = deferred.promise
  var p2 = Promise.accept(2)
  var p3 = Promise.defer().promise
  Promise.race([p1, p2, p3]).chain(
    function(x) { assertAsync(x === 2, "resolved/one") },
    assertUnreachable
  )
  deferred.resolve(1)
  assertAsyncRan()
})();

(function() {
  var deferred1 = Promise.defer()
  var p1 = deferred1.promise
  var deferred2 = Promise.defer()
  var p2 = deferred2.promise
  var deferred3 = Promise.defer()
  var p3 = deferred3.promise
  Promise.race([p1, p2, p3]).chain(
    function(x) { assertAsync(x === 3, "one/resolve/reject") },
    assertUnreachable
  )
  deferred3.resolve(3)
  deferred1.reject(1)
  assertAsyncRan()
})();

(function() {
  var deferred1 = Promise.defer()
  var p1 = deferred1.promise
  var deferred2 = Promise.defer()
  var p2 = deferred2.promise
  var deferred3 = Promise.defer()
  var p3 = deferred3.promise
  Promise.race([p1, p2, p3]).chain(
    assertUnreachable,
    function(x) { assertAsync(x === 3, "one/reject/resolve") }
  )
  deferred3.reject(3)
  deferred1.resolve(1)
  assertAsyncRan()
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
  MyPromise.prototype.chain = function(resolve, reject) {
    log += "c"
    return call(this.__proto__.__proto__.chain, this, resolve, reject)
  }

  log = ""
  var p1 = new MyPromise(function(resolve, reject) { resolve(1) })
  var p2 = new MyPromise(function(resolve, reject) { reject(2) })
  var d3 = MyPromise.defer()
  assertTrue(d3.promise instanceof Promise, "subclass/instance")
  assertTrue(d3.promise instanceof MyPromise, "subclass/instance-my3")
  assertTrue(log === "nx1nr2dn", "subclass/create")

  log = ""
  var p4 = MyPromise.resolve(4)
  var p5 = MyPromise.reject(5)
  assertTrue(p4 instanceof Promise, "subclass/instance4")
  assertTrue(p4 instanceof MyPromise, "subclass/instance-my4")
  assertTrue(p5 instanceof Promise, "subclass/instance5")
  assertTrue(p5 instanceof MyPromise, "subclass/instance-my5")
  d3.resolve(3)
  assertTrue(log === "nx4nr5x3", "subclass/resolve")

  log = ""
  var d6 = MyPromise.defer()
  d6.promise.chain(function(x) {
    return new Promise(function(resolve) { resolve(x) })
  }).chain(function() {})
  d6.resolve(6)
  assertTrue(log === "dncncnx6", "subclass/chain")

  log = ""
  Promise.all([11, Promise.accept(12), 13, MyPromise.accept(14), 15, 16])
  assertTrue(log === "nx14n", "subclass/all/arg")

  log = ""
  MyPromise.all([21, Promise.accept(22), 23, MyPromise.accept(24), 25, 26])
  assertTrue(log === "nx24nnx21nnx23nnnx25nnx26n", "subclass/all/self")
})();


assertAsyncDone()
