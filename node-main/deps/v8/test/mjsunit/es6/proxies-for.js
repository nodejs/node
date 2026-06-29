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

// Helper.

function TestWithProxies(test, x, y, z) {
  test(function(h){ return new Proxy({}, h) }, x, y, z)
}


// Iterate over a proxy.

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
  ownKeys() { return ["0", "a"] },
  has(target, property) { return true },
  getOwnPropertyDescriptor() { return { enumerable: true, configurable: true }}
})

TestForIn(["null", "a"], {
  ownKeys() { return this.enumerate() },
  enumerate() { return ["null", "a"] },
  has(target, property) { return true },
  getOwnPropertyDescriptor() { return { enumerable: true, configurable: true }}
})


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
  ownKeys: function() { return ["0", "a"] },
  has: function(t, k) { return k == "0" || k == "a" },
  getOwnPropertyDescriptor() { return { enumerable: true, configurable: true }}
})

TestForInDerived(["null", "a"], {
  ownKeys: function() { return this.enumerate() },
  enumerate: function() { return ["null", "a"] },
  has: function(t, k) { return k == "null" || k == "a" },
  getOwnPropertyDescriptor() { return { enumerable: true, configurable: true }}
})



// Throw exception in ownKeys trap.

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
  ownKeys: function() { throw "myexn" }
})

TestForInThrow({
  ownKeys: function() { return this.enumerate() },
  enumerate: function() { throw "myexn" }
})

TestForInThrow(new Proxy({}, {
  get: function(pr, pk) {
    return function() { throw "myexn" }
  }
}));


function keys(object) {
  var keys = [];
  for (var k in object) {
    keys.push(k);
  }
  return keys;
}

(function testKeysProxyOnProtoEmpty() {
  var p = new Proxy({}, {
    ownKeys() { return []; },
  });
  var o = [0];
  o.__proto__ = p;
  assertEquals(["0"], keys(o));

  delete o[0];
  assertEquals([], keys(o));
})();

(function testKeysProxyOnProto() {
  var handler = {ownKeys() { return ["0"]; }};
  var proxy = new Proxy({}, handler);
  var object = [0];
  object.__proto__ = proxy;
  assertEquals(["0"], keys(object));

  // The Proxy doesn't set its ownKeys enumerable.
  delete object[0];
  assertEquals([], keys(object));

  // The [[Has]] trap has no influence on which are enumerable properties are
  // shown in for-in.
  handler.has = function() { return true };
  assertEquals([], keys(object));

  handler.getOwnPropertyDescriptor = function() {
    return {enumerable: true, configurable: true}
  }
  assertEquals(["0"], keys(object));
})();

(function testKeysProxyProto() {
  var target = {t1:true, t2:true};
  var handler = {};
  var proxy = new Proxy(target, handler);

  assertEquals(["t1", "t2"], keys(proxy));

  target.__proto__ = {p1:true, p2:true};
  assertEquals(["t1", "t2", "p1", "p2"], keys(proxy));

  handler.getPrototypeOf = function(target) {
    return {p3:true, p4:true};
  };
  // for-in walks the prototype chain for the [[Has]] / Enumerable check.
  assertEquals(["t1", "t2", "p3", "p4"], keys(proxy));

  // [[Has]] is not used in for-in.
  handler.has = function() { return false };
  assertEquals(["t1", "t2", "p3", "p4"], keys(proxy));

  // Proxy intercepts enumerability check.
  handler.getOwnPropertyDescriptor = function() {
    return {enumerable: false, configurable: true}
  }
  assertEquals([], keys(proxy));

  handler.getOwnPropertyDescriptor = function() {
    return {enumerable: true, configurable: true}
  }
  assertEquals(["t1", "t2", "p3", "p4"], keys(proxy));

  handler.getOwnPropertyDescriptor = function(target, key) {
    return {
      enumerable: key in target,
      configurable: true
    }
  }
  assertEquals(["t1", "t2"], keys(proxy));

  handler.getPrototypeOf = function() { throw "error" };
  assertThrowsEquals(() => {keys(proxy)}, "error");
})();

(function testNestedProxy() {
  var handler = {
    ownKeys() {
      return ['c'];
    },
    getOwnPropertyDescriptor() { return {configurable: true, enumerable: true } }
  }
  var proxy = new Proxy({}, handler);
  var proxy2 = new Proxy(proxy, {});
  assertEquals(['c'], keys(proxy));
  assertEquals(['c'], keys(proxy2));
})();
