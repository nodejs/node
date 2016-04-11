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

// Flags: --harmony-proxies


// Helper.

function TestWithProxies(test, x, y, z) {
  test(function(h){ return new Proxy({}, h) }, x, y, z)
  test(function(h) {
    return new Proxy(function() {}, h)
  }, x, y, z)
}


// Iterate over a proxy.

Array.prototype.values = function() { return this[Symbol.iterator]() }

function TestForIn(properties, handler) {
  TestWithProxies(TestForIn2, properties, handler)
}

function TestForIn2(create, properties, handler) {
  var p = create(handler)
  var found = []
  for (var x in p) found.push(x)
  assertArrayEquals(properties, found)
}

TestForIn(["0", "a"], {
  enumerate() { return ["0", "a"].values() },
  has(target, property) { return true }
})

TestForIn(["null", "a"], {
  enumerate() { return this.enumerate2() },
  enumerate2() { return ["null", "a"].values() },
  has(target, property) { return true }
})

TestForIn(["b", "a", "0", "c"], new Proxy({}, {
  get: function(pr, pk) {
    return function() { return ["b", "a", "0", "c"].values() }
  }
}))



// Iterate over an object with a proxy prototype.

function TestForInDerived(properties, handler) {
  TestWithProxies(TestForInDerived2, properties, handler)
}

function TestForInDerived2(create, properties, handler) {
  var p = create(handler)
  var o = Object.create(p)
  o.z = 0
  var found = []
  for (var x in o) found.push(x)
  assertArrayEquals(["z"].concat(properties), found)

  var oo = Object.create(o)
  oo.y = 0
  var found = []
  for (var x in oo) found.push(x)
  assertArrayEquals(["y", "z"].concat(properties), found)
}

TestForInDerived(["0", "a"], {
  enumerate: function() { return ["0", "a"].values() },
  has: function(t, k) { return k == "0" || k == "a" }
})

TestForInDerived(["null", "a"], {
  enumerate: function() { return this.enumerate2() },
  enumerate2: function() { return ["null", "a"].values() },
  has: function(t, k) { return k == "null" || k == "a" }
})



// Throw exception in enumerate trap.

function TestForInThrow(handler) {
  TestWithProxies(TestForInThrow2, handler)
}

function TestForInThrow2(create, handler) {
  var p = create(handler)
  var o = Object.create(p)
  assertThrowsEquals(function(){ for (var x in p) {} }, "myexn")
  assertThrowsEquals(function(){ for (var x in o) {} }, "myexn")
}

TestForInThrow({
  enumerate: function() { throw "myexn" }
})

TestForInThrow({
  enumerate: function() { return this.enumerate2() },
  enumerate2: function() { throw "myexn" }
})

TestForInThrow(new Proxy({}, {
  get: function(pr, pk) {
    return function() { throw "myexn" }
  }
}));

(function() {
  var p = new Proxy({}, {enumerate:function() { return ["0"].values(); }});
  var o = [0];
  o.__proto__ = p;
  var keys = [];
  for (var k in o) { keys.push(k); };
  assertEquals(["0"], keys);
})();

(function () {
  var p = new Proxy({}, {ownKeys: function() { return ["1", Symbol(), "2"] }});
  assertEquals(["1","2"], Object.getOwnPropertyNames(p));
})();
