// Flags: --harmony-proxies

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


// TODO(rossberg): test exception cases.


// Getters.

function TestGet(handler) {
  var o = Proxy.create(handler)
  assertEquals(42, o.a)
  assertEquals(42, o["b"])
}

TestGet({
  get: function(r, k) { return 42 }
})
TestGet({
  get: function(r, k) { return this.get2(r, k) },
  get2: function(r, k) { return 42 }
})
TestGet({
  getPropertyDescriptor: function(k) { return {value: 42} }
})
TestGet({
  getPropertyDescriptor: function(k) { return this.getPropertyDescriptor2(k) },
  getPropertyDescriptor2: function(k) { return {value: 42} }
})
TestGet({
  getPropertyDescriptor: function(k) {
    return {get value() { return 42 }}
  }
})
TestGet({
  get: undefined,
  getPropertyDescriptor: function(k) { return {value: 42} }
})

TestGet(Proxy.create({
  get: function(pr, pk) {
    return function(r, k) { return 42 }
  }
}))


function TestGetCall(handler) {
  var p = Proxy.create(handler)
  assertEquals(55, p.f())
  assertEquals(55, p.f("unused", "arguments"))
  assertEquals(55, p.f.call(p))
  assertEquals(55, p.withargs(45, 5))
  assertEquals(55, p.withargs.call(p, 11, 22))
  assertEquals("6655", "66" + p)  // calls p.toString
}

TestGetCall({
  get: function(r, k) { return function() { return 55 } }
})
TestGetCall({
  get: function(r, k) { return this.get2(r, k) },
  get2: function(r, k) { return function() { return 55 } }
})
TestGetCall({
  getPropertyDescriptor: function(k) {
    return {value: function() { return 55 }}
  }
})
TestGetCall({
  getPropertyDescriptor: function(k) { return this.getPropertyDescriptor2(k) },
  getPropertyDescriptor2: function(k) {
    return {value: function() { return 55 }}
  }
})
TestGetCall({
  getPropertyDescriptor: function(k) {
    return {get value() { return function() { return 55 } }}
  }
})
TestGetCall({
  get: undefined,
  getPropertyDescriptor: function(k) {
    return {value: function() { return 55 }}
  }
})
TestGetCall({
  get: function(r, k) {
    if (k == "gg") {
      return function() { return 55 }
    } else if (k == "withargs") {
      return function(n, m) { return n + m * 2 }
    } else {
      return function() { return this.gg() }
    }
  }
})

TestGetCall(Proxy.create({
  get: function(pr, pk) {
    return function(r, k) { return function() { return 55 } }
  }
}))



// Setters.

var key
var val
function TestSet(handler) {
  var o = Proxy.create(handler)
  assertEquals(42, o.a = 42)
  assertEquals("a", key)
  assertEquals(42, val)
  assertEquals(43, o["b"] = 43)
  assertEquals("b", key)
  assertEquals(43, val)
}

TestSet({
  set: function(r, k, v) { key = k; val = v; return true }
})
TestSet({
  set: function(r, k, v) { return this.set2(r, k, v) },
  set2: function(r, k, v) { key = k; val = v; return true }
})
TestSet({
  getOwnPropertyDescriptor: function(k) { return {writable: true} },
  defineProperty: function(k, desc) { key = k; val = desc.value }
})
TestSet({
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { return {writable: true} },
  defineProperty: function(k, desc) { this.defineProperty2(k, desc) },
  defineProperty2: function(k, desc) { key = k; val = desc.value }
})
TestSet({
  getOwnPropertyDescriptor: function(k) {
    return {get writable() { return true }}
  },
  defineProperty: function(k, desc) { key = k; val = desc.value }
})
TestSet({
  getOwnPropertyDescriptor: function(k) {
    return {set: function(v) { key = k; val = v }}
  }
})
TestSet({
  getOwnPropertyDescriptor: function(k) { return null },
  getPropertyDescriptor: function(k) { return {writable: true} },
  defineProperty: function(k, desc) { key = k; val = desc.value }
})
TestSet({
  getOwnPropertyDescriptor: function(k) { return null },
  getPropertyDescriptor: function(k) {
    return {get writable() { return true }}
  },
  defineProperty: function(k, desc) { key = k; val = desc.value }
})
TestSet({
  getOwnPropertyDescriptor: function(k) { return null },
  getPropertyDescriptor: function(k) {
    return {set: function(v) { key = k; val = v }}
  }
})
TestSet({
  getOwnPropertyDescriptor: function(k) { return null },
  getPropertyDescriptor: function(k) { return null },
  defineProperty: function(k, desc) { key = k, val = desc.value }
})

TestSet(Proxy.create({
  get: function(pr, pk) {
    return function(r, k, v) { key = k; val = v; return true }
  }
}))



// Property definition (Object.defineProperty and Object.defineProperties).

var key
var desc
function TestDefine(handler) {
  var o = Proxy.create(handler)
  assertEquals(o, Object.defineProperty(o, "a", {value: 44}))
  assertEquals("a", key)
  assertEquals(1, Object.getOwnPropertyNames(desc).length)
  assertEquals(44, desc.value)

  assertEquals(o, Object.defineProperty(o, "b", {value: 45, writable: false}))
  assertEquals("b", key)
  assertEquals(2, Object.getOwnPropertyNames(desc).length)
  assertEquals(45, desc.value)
  assertEquals(false, desc.writable)

  assertEquals(o, Object.defineProperty(o, "c", {value: 46, enumerable: false}))
  assertEquals("c", key)
  assertEquals(2, Object.getOwnPropertyNames(desc).length)
  assertEquals(46, desc.value)
  assertEquals(false, desc.enumerable)

  var attributes = {configurable: true, mine: 66, minetoo: 23}
  assertEquals(o, Object.defineProperty(o, "d", attributes))
  assertEquals("d", key)
  // Modifying the attributes object after the fact should have no effect.
  attributes.configurable = false
  attributes.mine = 77
  delete attributes.minetoo
  assertEquals(3, Object.getOwnPropertyNames(desc).length)
  assertEquals(true, desc.configurable)
  assertEquals(66, desc.mine)
  assertEquals(23, desc.minetoo)

  assertEquals(o, Object.defineProperty(o, "e", {get: function(){ return 5 }}))
  assertEquals("e", key)
  assertEquals(1, Object.getOwnPropertyNames(desc).length)
  assertEquals(5, desc.get())

  assertEquals(o, Object.defineProperty(o, "zzz", {}))
  assertEquals("zzz", key)
  assertEquals(0, Object.getOwnPropertyNames(desc).length)

// TODO(rossberg): This test requires [s in proxy] to be implemented first.
//  var d = Proxy.create({
//    get: function(r, k) { return (k === "value") ? 77 : void 0 },
//    getOwnPropertyNames: function() { return ["value"] }
//  })
//  assertEquals(1, Object.getOwnPropertyNames(d).length)
//  assertEquals(77, d.value)
//  assertEquals(o, Object.defineProperty(o, "p", d))
//  assertEquals("p", key)
//  assertEquals(1, Object.getOwnPropertyNames(desc).length)
//  assertEquals(77, desc.value)

  var props = {
    'bla': {},
    blub: {get: function() { return true }},
    '': {get value() { return 20 }},
    last: {value: 21, configurable: true, mine: "eyes"}
  }
  Object.defineProperty(props, "hidden", {value: "hidden", enumerable: false})
  assertEquals(o, Object.defineProperties(o, props))
  assertEquals("last", key)
  assertEquals(2, Object.getOwnPropertyNames(desc).length)
  assertEquals(21, desc.value)
  assertEquals(true, desc.configurable)
  assertEquals(undefined, desc.mine)  // Arguably a bug in the spec...
}

TestDefine({
  defineProperty: function(k, d) { key = k; desc = d; return true }
})
TestDefine({
  defineProperty: function(k, d) { return this.defineProperty2(k, d) },
  defineProperty2: function(k, d) { key = k; desc = d; return true }
})
TestDefine(Proxy.create({
  get: function(pr, pk) {
    return function(k, d) { key = k; desc = d; return true }
  }
}))



// Property deletion (delete).

var key
function TestDelete(handler) {
  var o = Proxy.create(handler)
  assertEquals(true, delete o.a)
  assertEquals("a", key)
  assertEquals(true, delete o["b"])
  assertEquals("b", key)

  assertEquals(false, delete o.z1)
  assertEquals("z1", key)
  assertEquals(false, delete o["z2"])
  assertEquals("z2", key);

  (function() {
    "use strict"
    assertEquals(true, delete o.c)
    assertEquals("c", key)
    assertEquals(true, delete o["d"])
    assertEquals("d", key)

    assertThrows(function() { delete o.z3 }, TypeError)
    assertEquals("z3", key)
    assertThrows(function() { delete o["z4"] }, TypeError)
    assertEquals("z4", key)
  })()
}

TestDelete({
  'delete': function(k) { key = k; return k < "z" }
})
TestDelete({
  'delete': function(k) { return this.delete2(k) },
  delete2: function(k) { key = k; return k < "z" }
})
TestDelete(Proxy.create({
  get: function(pr, pk) {
    return function(k) { key = k; return k < "z" }
  }
}))



// Property descriptors (Object.getOwnPropertyDescriptor).

function TestDescriptor(handler) {
  var o = Proxy.create(handler)
  var descs = [
    {configurable: true},
    {value: 34, enumerable: true, configurable: true},
    {value: 3, writable: false, mine: "eyes", configurable: true},
    {get value() { return 20 }, get configurable() { return true }},
    {get: function() { "get" }, set: function() { "set" }, configurable: true}
  ]
  for (var i = 0; i < descs.length; ++i) {
    assertEquals(o, Object.defineProperty(o, i, descs[i]))
    var desc = Object.getOwnPropertyDescriptor(o, i)
    for (p in descs[i]) {
      // TODO(rossberg): Ignore user attributes as long as the spec isn't
      // fixed suitably.
      if (p != "mine") assertEquals(descs[i][p], desc[p])
    }
    assertEquals(undefined, Object.getOwnPropertyDescriptor(o, "absent"))
  }
}


TestDescriptor({
  defineProperty: function(k, d) { this["__" + k] = d; return true },
  getOwnPropertyDescriptor: function(k) { return this["__" + k] }
})
TestDescriptor({
  defineProperty: function(k, d) { this["__" + k] = d; return true },
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { return this["__" + k] }
})



// Comparison.

function TestComparison(eq) {
  var o1 = Proxy.create({})
  var o2 = Proxy.create({})

  assertTrue(eq(o1, o1))
  assertTrue(eq(o2, o2))
  assertTrue(!eq(o1, o2))
  assertTrue(!eq(o1, {}))
  assertTrue(!eq({}, o2))
  assertTrue(!eq({}, {}))
}

TestComparison(function(o1, o2) { return o1 == o2 })
TestComparison(function(o1, o2) { return o1 === o2 })
TestComparison(function(o1, o2) { return !(o1 != o2) })
TestComparison(function(o1, o2) { return !(o1 !== o2) })



// Type.

assertEquals("object", typeof Proxy.create({}))
assertTrue(typeof Proxy.create({}) == "object")
assertTrue("object" == typeof Proxy.create({}))

// No function proxies yet.



// Element (in).

var key
function TestIn(handler) {
  var o = Proxy.create(handler)
  assertTrue("a" in o)
  assertEquals("a", key)
  assertTrue(99 in o)
  assertEquals("99", key)
  assertFalse("z" in o)
  assertEquals("z", key)

  if ("b" in o) {
  } else {
    assertTrue(false)
  }
  assertEquals("b", key)

  if ("zz" in o) {
    assertTrue(false)
  }
  assertEquals("zz", key)

  if (!("c" in o)) {
    assertTrue(false)
  }
  assertEquals("c", key)

  if (!("zzz" in o)) {
  } else {
    assertTrue(false)
  }
  assertEquals("zzz", key)
}

TestIn({
  has: function(k) { key = k; return k < "z" }
})
TestIn({
  has: function(k) { return this.has2(k) },
  has2: function(k) { key = k; return k < "z" }
})
TestIn({
  getPropertyDescriptor: function(k) {
    key = k; return k < "z" ? {value: 42} : void 0
  }
})
TestIn({
  getPropertyDescriptor: function(k) { return this.getPropertyDescriptor2(k) },
  getPropertyDescriptor2: function(k) {
    key = k; return k < "z" ? {value: 42} : void 0
  }
})
TestIn({
  getPropertyDescriptor: function(k) {
    key = k; return k < "z" ? {get value() { return 42 }} : void 0
  }
})
TestIn({
  get: undefined,
  getPropertyDescriptor: function(k) {
    key = k; return k < "z" ? {value: 42} : void 0
  }
})

TestIn(Proxy.create({
  get: function(pr, pk) {
    return function(k) { key = k; return k < "z" }
  }
}))



// Instanceof (instanceof).

function TestInstanceof() {
  var o = {}
  var p1 = Proxy.create({})
  var p2 = Proxy.create({}, o)
  var p3 = Proxy.create({}, p2)

  var f = function() {}
  f.prototype = o
  var f1 = function() {}
  f1.prototype = p1
  var f2 = function() {}
  f2.prototype = p2

  assertTrue(o instanceof Object)
  assertFalse(o instanceof f)
  assertFalse(o instanceof f1)
  assertFalse(o instanceof f2)
  assertFalse(p1 instanceof Object)
  assertFalse(p1 instanceof f)
  assertFalse(p1 instanceof f1)
  assertFalse(p1 instanceof f2)
  assertTrue(p2 instanceof Object)
  assertTrue(p2 instanceof f)
  assertFalse(p2 instanceof f1)
  assertFalse(p2 instanceof f2)
  assertTrue(p3 instanceof Object)
  assertTrue(p3 instanceof f)
  assertFalse(p3 instanceof f1)
  assertTrue(p3 instanceof f2)
}

TestInstanceof()



// Prototype (Object.getPrototypeOf).

function TestPrototype() {
  var o = {}
  var p1 = Proxy.create({})
  var p2 = Proxy.create({}, o)
  var p3 = Proxy.create({}, p2)
  var p4 = Proxy.create({}, 666)

  assertSame(Object.getPrototypeOf(o), Object.prototype)
  assertSame(Object.getPrototypeOf(p1), null)
  assertSame(Object.getPrototypeOf(p2), o)
  assertSame(Object.getPrototypeOf(p3), p2)
  assertSame(Object.getPrototypeOf(p4), null)
}

TestPrototype()



// Property names (Object.getOwnPropertyNames, Object.keys).

function TestPropertyNames(names, handler) {
  var p = Proxy.create(handler)
  assertArrayEquals(names, Object.getOwnPropertyNames(p))
}

TestPropertyNames([], {
  getOwnPropertyNames: function() { return [] }
})
TestPropertyNames(["a", "zz", " ", "0"], {
  getOwnPropertyNames: function() { return ["a", "zz", " ", 0] }
})
TestPropertyNames(["throw", "function "], {
  getOwnPropertyNames: function() { return this.getOwnPropertyNames2() },
  getOwnPropertyNames2: function() { return ["throw", "function "] }
})
TestPropertyNames(["[object Object]"], {
  get getOwnPropertyNames() {
    return function() { return [{}] }
  }
})


function TestKeys(names, handler) {
  var p = Proxy.create(handler)
  assertArrayEquals(names, Object.keys(p))
}

TestKeys([], {
  keys: function() { return [] }
})
TestKeys(["a", "zz", " ", "0"], {
  keys: function() { return ["a", "zz", " ", 0] }
})
TestKeys(["throw", "function "], {
  keys: function() { return this.keys2() },
  keys2: function() { return ["throw", "function "] }
})
TestKeys(["[object Object]"], {
  get keys() {
    return function() { return [{}] }
  }
})
TestKeys(["a", "0"], {
  getOwnPropertyNames: function() { return ["a", 23, "zz", "", 0] },
  getOwnPropertyDescriptor: function(k) { return {enumerable: k.length == 1} }
})
TestKeys(["23", "zz", ""], {
  getOwnPropertyNames: function() { return this.getOwnPropertyNames2() },
  getOwnPropertyNames2: function() { return ["a", 23, "zz", "", 0] },
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { return {enumerable: k.length != 1} }
})
TestKeys(["a", "b", "c", "5"], {
  get getOwnPropertyNames() {
    return function() { return ["0", 4, "a", "b", "c", 5] }
  },
  get getOwnPropertyDescriptor() {
    return function(k) { return {enumerable: k >= "44"} }
  }
})
TestKeys([], {
  get getOwnPropertyNames() {
    return function() { return ["a", "b", "c"] }
  },
  getOwnPropertyDescriptor: function(k) { return {} }
})



// Fixing (Object.freeze, Object.seal, Object.preventExtensions,
//         Object.isFrozen, Object.isSealed, Object.isExtensible)

function TestFix(names, handler) {
  var proto = {p: 77}
  var assertFixing = function(o, s, f, e) {
    assertEquals(s, Object.isSealed(o))
    assertEquals(f, Object.isFrozen(o))
    assertEquals(e, Object.isExtensible(o))
  }

  var o1 = Proxy.create(handler, proto)
  assertFixing(o1, false, false, true)
  Object.seal(o1)
  assertFixing(o1, true, names.length === 0, false)
  assertArrayEquals(names.sort(), Object.getOwnPropertyNames(o1).sort())
  assertArrayEquals(names.filter(function(x) {return x < "z"}).sort(),
                    Object.keys(o1).sort())
  assertEquals(proto, Object.getPrototypeOf(o1))
  assertEquals(77, o1.p)
  for (var n in o1) {
    var desc = Object.getOwnPropertyDescriptor(o1, n)
    if (desc !== undefined) assertFalse(desc.configurable)
  }

  var o2 = Proxy.create(handler, proto)
  assertFixing(o2, false, false, true)
  Object.freeze(o2)
  assertFixing(o2, true, true, false)
  assertArrayEquals(names.sort(), Object.getOwnPropertyNames(o2).sort())
  assertArrayEquals(names.filter(function(x) {return x < "z"}).sort(),
                    Object.keys(o2).sort())
  assertEquals(proto, Object.getPrototypeOf(o2))
  assertEquals(77, o2.p)
  for (var n in o2) {
    var desc = Object.getOwnPropertyDescriptor(o2, n)
    if (desc !== undefined) assertFalse(desc.writable)
    if (desc !== undefined) assertFalse(desc.configurable)
  }

  var o3 = Proxy.create(handler, proto)
  assertFixing(o3, false, false, true)
  Object.preventExtensions(o3)
  assertFixing(o3, names.length === 0, names.length === 0, false)
  assertArrayEquals(names.sort(), Object.getOwnPropertyNames(o3).sort())
  assertArrayEquals(names.filter(function(x) {return x < "z"}).sort(),
                    Object.keys(o3).sort())
  assertEquals(proto, Object.getPrototypeOf(o3))
  assertEquals(77, o3.p)
}

TestFix([], {
  fix: function() { return {} }
})
TestFix(["a", "b", "c", "d", "zz"], {
  fix: function() {
    return {
      a: {value: "a", writable: true, configurable: false, enumerable: true},
      b: {value: 33, writable: false, configurable: false, enumerable: true},
      c: {value: 0, writable: true, configurable: true, enumerable: true},
      d: {value: true, writable: false, configurable: true, enumerable: true},
      zz: {value: 0, enumerable: false}
    }
  }
})
TestFix(["a"], {
  fix: function() { return this.fix2() },
  fix2: function() {
    return {a: {value: 4, writable: true, configurable: true, enumerable: true}}
  }
})
TestFix(["b"], {
  get fix() {
    return function() {
      return {b: {configurable: true, writable: true, enumerable: true}}
    }
  }
})
