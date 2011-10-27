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
  test(Proxy.create, x, y, z)
  test(function(h) {return Proxy.createFunction(h, function() {})}, x, y, z)
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
  enumerate: function() { return [0, "a"] }
})

TestForIn(["null", "a"], {
  enumerate: function() { return this.enumerate2() },
  enumerate2: function() { return [null, "a"] }
})

TestForIn(["b", "d"], {
  getPropertyNames: function() { return ["a", "b", "c", "d", "e"] },
  getPropertyDescriptor: function(k) {
    switch (k) {
      case "a": return {enumerable: false, value: "3"};
      case "b": return {enumerable: true, get get() {}};
      case "c": return {value: 4};
      case "d": return {get enumerable() { return true }};
      default: return undefined;
    }
  }
})

TestForIn(["b", "a", "0", "c"], Proxy.create({
  get: function(pr, pk) {
    return function() { return ["b", "a", 0, "c"] }
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
  enumerate: function() { return [0, "a"] },
  getPropertyDescriptor: function(k) {
    return k == "0" || k == "a" ? {} : undefined
  }
})

TestForInDerived(["null", "a"], {
  enumerate: function() { return this.enumerate2() },
  enumerate2: function() { return [null, "a"] },
  getPropertyDescriptor: function(k) {
    return k == "null" || k == "a" ? {} : undefined
  }
})

TestForInDerived(["b", "d"], {
  getPropertyNames: function() { return ["a", "b", "c", "d", "e"] },
  getPropertyDescriptor: function(k) {
    switch (k) {
      case "a": return {enumerable: false, value: "3"};
      case "b": return {enumerable: true, get get() {}};
      case "c": return {value: 4};
      case "d": return {get enumerable() { return true }};
      default: return undefined;
    }
  }
})



// Throw exception in enumerate trap.

function TestForInThrow(handler) {
  TestWithProxies(TestForInThrow2, handler)
}

function TestForInThrow2(create, handler) {
  var p = create(handler)
  var o = Object.create(p)
  assertThrows(function(){ for (var x in p) {} }, "myexn")
  assertThrows(function(){ for (var x in o) {} }, "myexn")
}

TestForInThrow({
  enumerate: function() { throw "myexn" }
})

TestForInThrow({
  enumerate: function() { return this.enumerate2() },
  enumerate2: function() { throw "myexn" }
})

TestForInThrow({
  getPropertyNames: function() { throw "myexn" }
})

TestForInThrow({
  getPropertyNames: function() { return ["a"] },
  getPropertyDescriptor: function() { throw "myexn" }
})

TestForInThrow(Proxy.create({
  get: function(pr, pk) {
    return function() { throw "myexn" }
  }
}))
