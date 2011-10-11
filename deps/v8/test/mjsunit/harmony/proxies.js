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

// Flags: --harmony-proxies


// TODO(rossberg): for-in for proxies not implemented.
// TODO(rossberg): inheritance from proxies not implemented.
// TODO(rossberg): function proxies as constructors not implemented.


// Helper.

function TestWithProxies(test, handler) {
  test(handler, Proxy.create)
  test(handler, function(h) {return Proxy.createFunction(h, function() {})})
}


// Getters.

function TestGet(handler) {
  TestWithProxies(TestGet2, handler)
}

function TestGet2(handler, create) {
  var p = create(handler)
  assertEquals(42, p.a)
  assertEquals(42, p["b"])

  // TODO(rossberg): inheritance from proxies not yet implemented.
  // var o = Object.create(p, {x: {value: 88}})
  // assertEquals(42, o.a)
  // assertEquals(42, o["b"])
  // assertEquals(88, o.x)
  // assertEquals(88, o["x"])
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
  TestWithProxies(TestGetCall2, handler)
}

function TestGetCall2(handler, create) {
  var p = create(handler)
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


function TestGetThrow(handler) {
  TestWithProxies(TestGetThrow2, handler)
}

function TestGetThrow2(handler, create) {
  var p = create(handler)
  assertThrows(function(){ p.a }, "myexn")
  assertThrows(function(){ p["b"] }, "myexn")
}

TestGetThrow({
  get: function(r, k) { throw "myexn" }
})

TestGetThrow({
  get: function(r, k) { return this.get2(r, k) },
  get2: function(r, k) { throw "myexn" }
})

TestGetThrow({
  getPropertyDescriptor: function(k) { throw "myexn" }
})

TestGetThrow({
  getPropertyDescriptor: function(k) { return this.getPropertyDescriptor2(k) },
  getPropertyDescriptor2: function(k) { throw "myexn" }
})

TestGetThrow({
  getPropertyDescriptor: function(k) {
    return {get value() { throw "myexn" }}
  }
})

TestGetThrow({
  get: undefined,
  getPropertyDescriptor: function(k) { throw "myexn" }
})

TestGetThrow(Proxy.create({
  get: function(pr, pk) { throw "myexn" }
}))

TestGetThrow(Proxy.create({
  get: function(pr, pk) {
    return function(r, k) { throw "myexn" }
  }
}))



// Setters.

var key
var val

function TestSet(handler, create) {
  TestWithProxies(TestSet2, handler)
}

function TestSet2(handler, create) {
  var p = create(handler)
  assertEquals(42, p.a = 42)
  assertEquals("a", key)
  assertEquals(42, val)
  assertEquals(43, p["b"] = 43)
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



function TestSetThrow(handler, create) {
  TestWithProxies(TestSetThrow2, handler)
}

function TestSetThrow2(handler, create) {
  var p = create(handler)
  assertThrows(function(){ p.a = 42 }, "myexn")
  assertThrows(function(){ p["b"] = 42 }, "myexn")
}

TestSetThrow({
  set: function(r, k, v) { throw "myexn" }
})

TestSetThrow({
  set: function(r, k, v) { return this.set2(r, k, v) },
  set2: function(r, k, v) { throw "myexn" }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" },
  defineProperty: function(k, desc) { key = k; val = desc.value }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { return {writable: true} },
  defineProperty: function(k, desc) { throw "myexn" }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { throw "myexn" },
  defineProperty: function(k, desc) { this.defineProperty2(k, desc) },
  defineProperty2: function(k, desc) { key = k; val = desc.value }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { return {writable: true} },
  defineProperty: function(k, desc) { this.defineProperty2(k, desc) },
  defineProperty2: function(k, desc) { throw "myexn" }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" },
  defineProperty: function(k, desc) { key = k; val = desc.value }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) {
    return {get writable() { return true }}
  },
  defineProperty: function(k, desc) { throw "myexn" }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) {
    return {set: function(v) { throw "myexn" }}
  }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" },
  getPropertyDescriptor: function(k) { return {writable: true} },
  defineProperty: function(k, desc) { key = k; val = desc.value }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { return null },
  getPropertyDescriptor: function(k) { throw "myexn" },
  defineProperty: function(k, desc) { key = k; val = desc.value }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { return null },
  getPropertyDescriptor: function(k) { return {writable: true} },
  defineProperty: function(k, desc) { throw "myexn" }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { return null },
  getPropertyDescriptor: function(k) {
    return {get writable() { throw "myexn" }}
  },
  defineProperty: function(k, desc) { key = k; val = desc.value }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { return null },
  getPropertyDescriptor: function(k) {
    return {set: function(v) { throw "myexn" }}
  }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { return null },
  getPropertyDescriptor: function(k) { return null },
  defineProperty: function(k, desc) { throw "myexn" }
})

TestSetThrow(Proxy.create({
  get: function(pr, pk) { throw "myexn" }
}))

TestSetThrow(Proxy.create({
  get: function(pr, pk) {
    return function(r, k, v) { throw "myexn" }
  }
}))



// Property definition (Object.defineProperty and Object.defineProperties).

var key
var desc

function TestDefine(handler) {
  TestWithProxies(TestDefine2, handler)
}

function TestDefine2(handler, create) {
  var p = create(handler)
  assertEquals(p, Object.defineProperty(p, "a", {value: 44}))
  assertEquals("a", key)
  assertEquals(1, Object.getOwnPropertyNames(desc).length)
  assertEquals(44, desc.value)

  assertEquals(p, Object.defineProperty(p, "b", {value: 45, writable: false}))
  assertEquals("b", key)
  assertEquals(2, Object.getOwnPropertyNames(desc).length)
  assertEquals(45, desc.value)
  assertEquals(false, desc.writable)

  assertEquals(p, Object.defineProperty(p, "c", {value: 46, enumerable: false}))
  assertEquals("c", key)
  assertEquals(2, Object.getOwnPropertyNames(desc).length)
  assertEquals(46, desc.value)
  assertEquals(false, desc.enumerable)

  var attributes = {configurable: true, mine: 66, minetoo: 23}
  assertEquals(p, Object.defineProperty(p, "d", attributes))
  assertEquals("d", key)
  // Modifying the attributes object after the fact should have no effect.
  attributes.configurable = false
  attributes.mine = 77
  delete attributes.minetoo
  assertEquals(3, Object.getOwnPropertyNames(desc).length)
  assertEquals(true, desc.configurable)
  assertEquals(66, desc.mine)
  assertEquals(23, desc.minetoo)

  assertEquals(p, Object.defineProperty(p, "e", {get: function(){ return 5 }}))
  assertEquals("e", key)
  assertEquals(1, Object.getOwnPropertyNames(desc).length)
  assertEquals(5, desc.get())

  assertEquals(p, Object.defineProperty(p, "zzz", {}))
  assertEquals("zzz", key)
  assertEquals(0, Object.getOwnPropertyNames(desc).length)

// TODO(rossberg): This test requires for-in on proxies.
//  var d = create({
//    get: function(r, k) { return (k === "value") ? 77 : void 0 },
//    getOwnPropertyNames: function() { return ["value"] }
//  })
//  assertEquals(1, Object.getOwnPropertyNames(d).length)
//  assertEquals(77, d.value)
//  assertEquals(p, Object.defineProperty(p, "p", d))
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
  assertEquals(p, Object.defineProperties(p, props))
  assertEquals("last", key)
  assertEquals(2, Object.getOwnPropertyNames(desc).length)
  assertEquals(21, desc.value)
  assertEquals(true, desc.configurable)
  assertEquals(undefined, desc.mine)  // Arguably a bug in the spec...

  var props = {bla: {get value() { throw "myexn" }}}
  assertThrows(function(){ Object.defineProperties(p, props) }, "myexn")
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


function TestDefineThrow(handler) {
  TestWithProxies(TestDefineThrow2, handler)
}

function TestDefineThrow2(handler, create) {
  var p = create(handler)
  assertThrows(function(){ Object.defineProperty(p, "a", {value: 44})}, "myexn")

// TODO(rossberg): These tests require for-in on proxies.
//  var d1 = create({
//    get: function(r, k) { throw "myexn" },
//    getOwnPropertyNames: function() { return ["value"] }
//  })
//  assertThrows(function(){ Object.defineProperty(p, "p", d1) }, "myexn")
//  var d2 = create({
//    get: function(r, k) { return 77 },
//    getOwnPropertyNames: function() { throw "myexn" }
//  })
//  assertThrows(function(){ Object.defineProperty(p, "p", d2) }, "myexn")

  var props = {bla: {get value() { throw "otherexn" }}}
  assertThrows(function(){ Object.defineProperties(p, props) }, "otherexn")
}

TestDefineThrow({
  defineProperty: function(k, d) { throw "myexn" }
})

TestDefineThrow({
  defineProperty: function(k, d) { return this.defineProperty2(k, d) },
  defineProperty2: function(k, d) { throw "myexn" }
})

TestDefineThrow(Proxy.create({
  get: function(pr, pk) { throw "myexn" }
}))

TestDefineThrow(Proxy.create({
  get: function(pr, pk) {
    return function(k, d) { throw "myexn" }
  }
}))



// Property deletion (delete).

var key

function TestDelete(handler) {
  TestWithProxies(TestDelete2, handler)
}

function TestDelete2(handler, create) {
  var p = create(handler)
  assertEquals(true, delete p.a)
  assertEquals("a", key)
  assertEquals(true, delete p["b"])
  assertEquals("b", key)

  assertEquals(false, delete p.z1)
  assertEquals("z1", key)
  assertEquals(false, delete p["z2"])
  assertEquals("z2", key);

  (function() {
    "use strict"
    assertEquals(true, delete p.c)
    assertEquals("c", key)
    assertEquals(true, delete p["d"])
    assertEquals("d", key)

    assertThrows(function(){ delete p.z3 }, TypeError)
    assertEquals("z3", key)
    assertThrows(function(){ delete p["z4"] }, TypeError)
    assertEquals("z4", key)
  })()
}

TestDelete({
  delete: function(k) { key = k; return k < "z" }
})

TestDelete({
  delete: function(k) { return this.delete2(k) },
  delete2: function(k) { key = k; return k < "z" }
})

TestDelete(Proxy.create({
  get: function(pr, pk) {
    return function(k) { key = k; return k < "z" }
  }
}))


function TestDeleteThrow(handler) {
  TestWithProxies(TestDeleteThrow2, handler)
}

function TestDeleteThrow2(handler, create) {
  var p = create(handler)
  assertThrows(function(){ delete p.a }, "myexn")
  assertThrows(function(){ delete p["b"] }, "myexn");

  (function() {
    "use strict"
    assertThrows(function(){ delete p.c }, "myexn")
    assertThrows(function(){ delete p["d"] }, "myexn")
  })()
}

TestDeleteThrow({
  delete: function(k) { throw "myexn" }
})

TestDeleteThrow({
  delete: function(k) { return this.delete2(k) },
  delete2: function(k) { throw "myexn" }
})

TestDeleteThrow(Proxy.create({
  get: function(pr, pk) { throw "myexn" }
}))

TestDeleteThrow(Proxy.create({
  get: function(pr, pk) {
    return function(k) { throw "myexn" }
  }
}))



// Property descriptors (Object.getOwnPropertyDescriptor).

function TestDescriptor(handler) {
  TestWithProxies(TestDescriptor2, handler)
}

function TestDescriptor2(handler, create) {
  var p = create(handler)
  var descs = [
    {configurable: true},
    {value: 34, enumerable: true, configurable: true},
    {value: 3, writable: false, mine: "eyes", configurable: true},
    {get value() { return 20 }, get configurable() { return true }},
    {get: function() { "get" }, set: function() { "set" }, configurable: true}
  ]
  for (var i = 0; i < descs.length; ++i) {
    assertEquals(p, Object.defineProperty(p, i, descs[i]))
    var desc = Object.getOwnPropertyDescriptor(p, i)
    for (prop in descs[i]) {
      // TODO(rossberg): Ignore user attributes as long as the spec isn't
      // fixed suitably.
      if (prop != "mine") assertEquals(descs[i][prop], desc[prop])
    }
    assertEquals(undefined, Object.getOwnPropertyDescriptor(p, "absent"))
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


function TestDescriptorThrow(handler) {
  TestWithProxies(TestDescriptorThrow2, handler)
}

function TestDescriptorThrow2(handler, create) {
  var p = create(handler)
  assertThrows(function(){ Object.getOwnPropertyDescriptor(p, "a") }, "myexn")
}

TestDescriptorThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" }
})

TestDescriptorThrow({
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { throw "myexn" }
})



// Comparison.

function TestComparison(eq) {
  TestWithProxies(TestComparison2, eq)
}

function TestComparison2(eq, create) {
  var p1 = create({})
  var p2 = create({})

  assertTrue(eq(p1, p1))
  assertTrue(eq(p2, p2))
  assertTrue(!eq(p1, p2))
  assertTrue(!eq(p1, {}))
  assertTrue(!eq({}, p2))
  assertTrue(!eq({}, {}))
}

TestComparison(function(o1, o2) { return o1 == o2 })
TestComparison(function(o1, o2) { return o1 === o2 })
TestComparison(function(o1, o2) { return !(o1 != o2) })
TestComparison(function(o1, o2) { return !(o1 !== o2) })



// Type (typeof).

function TestTypeof() {
  assertEquals("object", typeof Proxy.create({}))
  assertTrue(typeof Proxy.create({}) == "object")
  assertTrue("object" == typeof Proxy.create({}))

  assertEquals("function", typeof Proxy.createFunction({}, function() {}))
  assertTrue(typeof Proxy.createFunction({}, function() {}) == "function")
  assertTrue("function" == typeof Proxy.createFunction({}, function() {}))
}

TestTypeof()



// Membership test (in).

var key

function TestIn(handler) {
  TestWithProxies(TestIn2, handler)
}

function TestIn2(handler, create) {
  var p = create(handler)
  assertTrue("a" in p)
  assertEquals("a", key)
  assertTrue(99 in p)
  assertEquals("99", key)
  assertFalse("z" in p)
  assertEquals("z", key)

  assertEquals(2, ("a" in p) ? 2 : 0)
  assertEquals(0, !("a" in p) ? 2 : 0)
  assertEquals(0, ("zzz" in p) ? 2 : 0)
  assertEquals(2, !("zzz" in p) ? 2 : 0)

  if ("b" in p) {
  } else {
    assertTrue(false)
  }
  assertEquals("b", key)

  if ("zz" in p) {
    assertTrue(false)
  }
  assertEquals("zz", key)

  if (!("c" in p)) {
    assertTrue(false)
  }
  assertEquals("c", key)

  if (!("zzz" in p)) {
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


function TestInThrow(handler) {
  TestWithProxies(TestInThrow2, handler)
}

function TestInThrow2(handler, create) {
  var p = create(handler)
  assertThrows(function(){ return "a" in o }, "myexn")
  assertThrows(function(){ return !("a" in o) }, "myexn")
  assertThrows(function(){ return ("a" in o) ? 2 : 3 }, "myexn")
  assertThrows(function(){ if ("b" in o) {} }, "myexn")
  assertThrows(function(){ if (!("b" in o)) {} }, "myexn")
  assertThrows(function(){ if ("zzz" in o) {} }, "myexn")
}

TestInThrow({
  has: function(k) { throw "myexn" }
})

TestInThrow({
  has: function(k) { return this.has2(k) },
  has2: function(k) { throw "myexn" }
})

TestInThrow({
  getPropertyDescriptor: function(k) { throw "myexn" }
})

TestInThrow({
  getPropertyDescriptor: function(k) { return this.getPropertyDescriptor2(k) },
  getPropertyDescriptor2: function(k) { throw "myexn" }
})

TestInThrow({
  get: undefined,
  getPropertyDescriptor: function(k) { throw "myexn" }
})

TestInThrow(Proxy.create({
  get: function(pr, pk) { throw "myexn" }
}))

TestInThrow(Proxy.create({
  get: function(pr, pk) {
    return function(k) { throw "myexn" }
  }
}))



// Own Properties (Object.prototype.hasOwnProperty).

var key

function TestHasOwn(handler) {
  TestWithProxies(TestHasOwn2, handler)
}

function TestHasOwn2(handler, create) {
  var p = create(handler)
  assertTrue(Object.prototype.hasOwnProperty.call(p, "a"))
  assertEquals("a", key)
  assertTrue(Object.prototype.hasOwnProperty.call(p, 99))
  assertEquals("99", key)
  assertFalse(Object.prototype.hasOwnProperty.call(p, "z"))
  assertEquals("z", key)
}

TestHasOwn({
  hasOwn: function(k) { key = k; return k < "z" }
})

TestHasOwn({
  hasOwn: function(k) { return this.hasOwn2(k) },
  hasOwn2: function(k) { key = k; return k < "z" }
})

TestHasOwn({
  getOwnPropertyDescriptor: function(k) {
    key = k; return k < "z" ? {value: 42} : void 0
  }
})

TestHasOwn({
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) {
    key = k; return k < "z" ? {value: 42} : void 0
  }
})

TestHasOwn({
  getOwnPropertyDescriptor: function(k) {
    key = k; return k < "z" ? {get value() { return 42 }} : void 0
  }
})

TestHasOwn({
  hasOwn: undefined,
  getOwnPropertyDescriptor: function(k) {
    key = k; return k < "z" ? {value: 42} : void 0
  }
})

TestHasOwn(Proxy.create({
  get: function(pr, pk) {
    return function(k) { key = k; return k < "z" }
  }
}))


function TestHasOwnThrow(handler) {
  TestWithProxies(TestHasOwnThrow2, handler)
}

function TestHasOwnThrow2(handler, create) {
  var p = create(handler)
  assertThrows(function(){ Object.prototype.hasOwnProperty.call(p, "a")},
    "myexn")
  assertThrows(function(){ Object.prototype.hasOwnProperty.call(p, 99)},
    "myexn")
}

TestHasOwnThrow({
  hasOwn: function(k) { throw "myexn" }
})

TestHasOwnThrow({
  hasOwn: function(k) { return this.hasOwn2(k) },
  hasOwn2: function(k) { throw "myexn" }
})

TestHasOwnThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" }
})

TestHasOwnThrow({
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { throw "myexn" }
})

TestHasOwnThrow({
  hasOwn: undefined,
  getOwnPropertyDescriptor: function(k) { throw "myexn" }
})

TestHasOwnThrow(Proxy.create({
  get: function(pr, pk) { throw "myexn" }
}))

TestHasOwnThrow(Proxy.create({
  get: function(pr, pk) {
    return function(k) { throw "myexn" }
  }
}))



// Instanceof (instanceof)

function TestInstanceof() {
  var o = {}
  var p1 = Proxy.create({})
  var p2 = Proxy.create({}, o)
  var p3 = Proxy.create({}, p2)

  var f0 = function() {}
  f0.prototype = o
  var f1 = function() {}
  f1.prototype = p1
  var f2 = function() {}
  f2.prototype = p2

  assertTrue(o instanceof Object)
  assertFalse(o instanceof f0)
  assertFalse(o instanceof f1)
  assertFalse(o instanceof f2)
  assertFalse(p1 instanceof Object)
  assertFalse(p1 instanceof f0)
  assertFalse(p1 instanceof f1)
  assertFalse(p1 instanceof f2)
  assertTrue(p2 instanceof Object)
  assertTrue(p2 instanceof f0)
  assertFalse(p2 instanceof f1)
  assertFalse(p2 instanceof f2)
  assertTrue(p3 instanceof Object)
  assertTrue(p3 instanceof f0)
  assertFalse(p3 instanceof f1)
  assertTrue(p3 instanceof f2)

  var f = Proxy.createFunction({}, function() {})
  assertTrue(f instanceof Function)
}

TestInstanceof()



// Prototype (Object.getPrototypeOf, Object.prototype.isPrototypeOf).

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

  assertTrue(Object.prototype.isPrototypeOf(o))
  assertFalse(Object.prototype.isPrototypeOf(p1))
  assertTrue(Object.prototype.isPrototypeOf(p2))
  assertTrue(Object.prototype.isPrototypeOf(p3))
  assertFalse(Object.prototype.isPrototypeOf(p4))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, o))
  assertFalse(Object.prototype.isPrototypeOf.call(Object.prototype, p1))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, p2))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(Object.prototype, p4))
  assertFalse(Object.prototype.isPrototypeOf.call(o, o))
  assertFalse(Object.prototype.isPrototypeOf.call(o, p1))
  assertTrue(Object.prototype.isPrototypeOf.call(o, p2))
  assertTrue(Object.prototype.isPrototypeOf.call(o, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(o, p4))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, p1))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, o))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, p2))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, p4))
  assertFalse(Object.prototype.isPrototypeOf.call(p2, p1))
  assertFalse(Object.prototype.isPrototypeOf.call(p2, p2))
  assertTrue(Object.prototype.isPrototypeOf.call(p2, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(p2, p4))
  assertFalse(Object.prototype.isPrototypeOf.call(p3, p2))

  var f = Proxy.createFunction({}, function() {})
  assertSame(Object.getPrototypeOf(f), Function.prototype)
  assertTrue(Object.prototype.isPrototypeOf(f))
  assertTrue(Object.prototype.isPrototypeOf.call(Function.prototype, f))
}

TestPrototype()



// Property names (Object.getOwnPropertyNames, Object.keys).

function TestPropertyNames(names, handler) {
  TestWithProxies(TestPropertyNames2, [names, handler])
}

function TestPropertyNames2(names_handler, create) {
  var p = create(names_handler[1])
  assertArrayEquals(names_handler[0], Object.getOwnPropertyNames(p))
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


function TestPropertyNamesThrow(handler) {
  TestWithProxies(TestPropertyNamesThrow2, handler)
}

function TestPropertyNamesThrow2(handler, create) {
  var p = create(handler)
  assertThrows(function(){ Object.getOwnPropertyNames(p) }, "myexn")
}

TestPropertyNamesThrow({
  getOwnPropertyNames: function() { throw "myexn" }
})

TestPropertyNamesThrow({
  getOwnPropertyNames: function() { return this.getOwnPropertyNames2() },
  getOwnPropertyNames2: function() { throw "myexn" }
})


function TestKeys(names, handler) {
  TestWithProxies(TestKeys2, [names, handler])
}

function TestKeys2(names_handler, create) {
  var p = create(names_handler[1])
  assertArrayEquals(names_handler[0], Object.keys(p))
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


function TestKeysThrow(handler) {
  TestWithProxies(TestKeysThrow2, handler)
}

function TestKeysThrow2(handler, create) {
  var p = create(handler)
  assertThrows(function(){ Object.keys(p) }, "myexn")
}

TestKeysThrow({
  keys: function() { throw "myexn" }
})

TestKeysThrow({
  keys: function() { return this.keys2() },
  keys2: function() { throw "myexn" }
})

TestKeysThrow({
  getOwnPropertyNames: function() { throw "myexn" },
  getOwnPropertyDescriptor: function(k) { return true }
})

TestKeysThrow({
  getOwnPropertyNames: function() { return [1, 2] },
  getOwnPropertyDescriptor: function(k) { throw "myexn" }
})

TestKeysThrow({
  getOwnPropertyNames: function() { return this.getOwnPropertyNames2() },
  getOwnPropertyNames2: function() { throw "myexn" },
})

TestKeysThrow({
  getOwnPropertyNames: function() { return this.getOwnPropertyNames2() },
  getOwnPropertyNames2: function() { return [1, 2] },
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { throw "myexn" }
})

TestKeysThrow({
  get getOwnPropertyNames() { throw "myexn" }
})

TestKeysThrow({
  get getOwnPropertyNames() {
    return function() { throw "myexn" }
  },
})

TestKeysThrow([], {
  get getOwnPropertyNames() {
    return function() { return [1, 2] }
  },
  getOwnPropertyDescriptor: function(k) { throw "myexn" }
})



// Fixing (Object.freeze, Object.seal, Object.preventExtensions,
//         Object.isFrozen, Object.isSealed, Object.isExtensible)

// TODO(rossberg): use TestWithProxies to include funciton proxies
function TestFix(names, handler) {
  var proto = {p: 77}
  var assertFixing = function(o, s, f, e) {
    assertEquals(s, Object.isSealed(o))
    assertEquals(f, Object.isFrozen(o))
    assertEquals(e, Object.isExtensible(o))
  }

  var p1 = Proxy.create(handler, proto)
  assertFixing(p1, false, false, true)
  Object.seal(p1)
  assertFixing(p1, true, names.length === 0, false)
  assertArrayEquals(names.sort(), Object.getOwnPropertyNames(p1).sort())
  assertArrayEquals(names.filter(function(x) {return x < "z"}).sort(),
                    Object.keys(p1).sort())
  assertEquals(proto, Object.getPrototypeOf(p1))
  assertEquals(77, p1.p)
  for (var n in p1) {
    var desc = Object.getOwnPropertyDescriptor(p1, n)
    if (desc !== undefined) assertFalse(desc.configurable)
  }

  var p2 = Proxy.create(handler, proto)
  assertFixing(p2, false, false, true)
  Object.freeze(p2)
  assertFixing(p2, true, true, false)
  assertArrayEquals(names.sort(), Object.getOwnPropertyNames(p2).sort())
  assertArrayEquals(names.filter(function(x) {return x < "z"}).sort(),
                    Object.keys(p2).sort())
  assertEquals(proto, Object.getPrototypeOf(p2))
  assertEquals(77, p2.p)
  for (var n in p2) {
    var desc = Object.getOwnPropertyDescriptor(p2, n)
    if (desc !== undefined) assertFalse(desc.writable)
    if (desc !== undefined) assertFalse(desc.configurable)
  }

  var p3 = Proxy.create(handler, proto)
  assertFixing(p3, false, false, true)
  Object.preventExtensions(p3)
  assertFixing(p3, names.length === 0, names.length === 0, false)
  assertArrayEquals(names.sort(), Object.getOwnPropertyNames(p3).sort())
  assertArrayEquals(names.filter(function(x) {return x < "z"}).sort(),
                    Object.keys(p3).sort())
  assertEquals(proto, Object.getPrototypeOf(p3))
  assertEquals(77, p3.p)
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


function TestFixFunction(fix) {
  var f1 = Proxy.createFunction({
    fix: function() { return {} }
  }, function() {})
  fix(f1)
  assertEquals(0, f1.length)

  var f2 = Proxy.createFunction({
    fix: function() { return {length: {value: 3}} }
  }, function() {})
  fix(f2)
  assertEquals(3, f2.length)

  var f3 = Proxy.createFunction({
    fix: function() { return {length: {value: "huh"}} }
  }, function() {})
  fix(f3)
  assertEquals(0, f1.length)
}

TestFixFunction(Object.seal)
TestFixFunction(Object.freeze)
TestFixFunction(Object.preventExtensions)


function TestFixThrow(handler) {
  TestWithProxies(TestFixThrow2, handler)
}

function TestFixThrow2(handler) {
  var p = Proxy.create(handler, {})
  assertThrows(function(){ Object.seal(p) }, "myexn")
  assertThrows(function(){ Object.freeze(p) }, "myexn")
  assertThrows(function(){ Object.preventExtensions(p) }, "myexn")
}

TestFixThrow({
  fix: function() { throw "myexn" }
})

TestFixThrow({
  fix: function() { return this.fix2() },
  fix2: function() { throw "myexn" }
})

TestFixThrow({
  get fix() { throw "myexn" }
})

TestFixThrow({
  get fix() {
    return function() { throw "myexn" }
  }
})



// String conversion (Object.prototype.toString,
//                    Object.prototype.toLocaleString,
//                    Function.prototype.toString)

var key

function TestToString(handler) {
  var p = Proxy.create(handler)
  key = ""
  assertEquals("[object Object]", Object.prototype.toString.call(p))
  assertEquals("", key)
  assertEquals("my_proxy", Object.prototype.toLocaleString.call(p))
  assertEquals("toString", key)

  var f = Proxy.createFunction(handler, function() {})
  key = ""
  assertEquals("[object Function]", Object.prototype.toString.call(f))
  assertEquals("", key)
  assertEquals("my_proxy", Object.prototype.toLocaleString.call(f))
  assertEquals("toString", key)
  assertDoesNotThrow(function(){ Function.prototype.toString.call(f) })
}

TestToString({
  get: function(r, k) { key = k; return function() { return "my_proxy" } }
})

TestToString({
  get: function(r, k) { return this.get2(r, k) },
  get2: function(r, k) { key = k; return function() { return "my_proxy" } }
})

TestToString(Proxy.create({
  get: function(pr, pk) {
    return function(r, k) { key = k; return function() { return "my_proxy" } }
  }
}))


function TestToStringThrow(handler) {
  var p = Proxy.create(handler)
  assertEquals("[object Object]", Object.prototype.toString.call(p))
  assertThrows(function(){ Object.prototype.toLocaleString.call(p) }, "myexn")

  var f = Proxy.createFunction(handler, function() {})
  assertEquals("[object Function]", Object.prototype.toString.call(f))
  assertThrows(function(){ Object.prototype.toLocaleString.call(f) }, "myexn")
}

TestToStringThrow({
  get: function(r, k) { throw "myexn" }
})

TestToStringThrow({
  get: function(r, k) { return function() { throw "myexn" } }
})

TestToStringThrow({
  get: function(r, k) { return this.get2(r, k) },
  get2: function(r, k) { throw "myexn" }
})

TestToStringThrow(Proxy.create({
  get: function(pr, pk) { throw "myexn" }
}))

TestToStringThrow(Proxy.create({
  get: function(pr, pk) {
    return function(r, k) { throw "myexn" }
  }
}))



// Value conversion (Object.prototype.toValue)

function TestValueOf(handler) {
  TestWithProxies(TestValueOf2, handler)
}

function TestValueOf2(handler, create) {
  var p = create(handler)
  assertSame(p, Object.prototype.valueOf.call(p))
}

TestValueOf({})



// Enumerability (Object.prototype.propertyIsEnumerable)

var key

function TestIsEnumerable(handler) {
  TestWithProxies(TestIsEnumerable2, handler)
}

function TestIsEnumerable2(handler, create) {
  var p = create(handler)
  assertTrue(Object.prototype.propertyIsEnumerable.call(p, "a"))
  assertEquals("a", key)
  assertTrue(Object.prototype.propertyIsEnumerable.call(p, 2))
  assertEquals("2", key)
  assertFalse(Object.prototype.propertyIsEnumerable.call(p, "z"))
  assertEquals("z", key)
}

TestIsEnumerable({
  getOwnPropertyDescriptor: function(k) {
    key = k; return {enumerable: k < "z", configurable: true}
  },
})

TestIsEnumerable({
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) {
    key = k; return {enumerable: k < "z", configurable: true}
  },
})

TestIsEnumerable({
  getOwnPropertyDescriptor: function(k) {
    key = k; return {get enumerable() { return k < "z" }, configurable: true}
  },
})

TestIsEnumerable(Proxy.create({
  get: function(pr, pk) {
    return function(k) {
      key = k; return {enumerable: k < "z", configurable: true}
    }
  }
}))


function TestIsEnumerableThrow(handler) {
  TestWithProxies(TestIsEnumerableThrow2, handler)
}

function TestIsEnumerableThrow2(handler, create) {
  var p = create(handler)
  assertThrows(function(){ Object.prototype.propertyIsEnumerable.call(p, "a") },
    "myexn")
  assertThrows(function(){ Object.prototype.propertyIsEnumerable.call(p, 11) },
    "myexn")
}

TestIsEnumerableThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" }
})

TestIsEnumerableThrow({
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { throw "myexn" }
})

TestIsEnumerableThrow({
  getOwnPropertyDescriptor: function(k) {
    return {get enumerable() { throw "myexn" }, configurable: true}
  },
})

TestIsEnumerableThrow(Proxy.create({
  get: function(pr, pk) { throw "myexn" }
}))

TestIsEnumerableThrow(Proxy.create({
  get: function(pr, pk) {
    return function(k) { throw "myexn" }
  }
}))



// Calling (call, Function.prototype.call, Function.prototype.apply,
//          Function.prototype.bind).

var global = this
var receiver

function TestCall(isStrict, callTrap) {
  assertEquals(42, callTrap(5, 37))
// TODO(rossberg): unrelated bug: this does not succeed for optimized code.
// assertEquals(isStrict ? undefined : global, receiver)

  var f = Proxy.createFunction({fix: function() { return {} }}, callTrap)
  receiver = 333
  assertEquals(42, f(11, 31))
  assertEquals(isStrict ? undefined : global, receiver)
  var o = {}
  assertEquals(42, Function.prototype.call.call(f, o, 20, 22))
  assertEquals(o, receiver)
  assertEquals(43, Function.prototype.call.call(f, null, 20, 23))
  assertEquals(isStrict ? null : global, receiver)
  assertEquals(44, Function.prototype.call.call(f, 2, 21, 23))
  assertEquals(2, receiver.valueOf())
  receiver = 333
  assertEquals(32, Function.prototype.apply.call(f, o, [17, 15]))
  assertEquals(o, receiver)
  var ff = Function.prototype.bind.call(f, o, 12)
  receiver = 333
  assertEquals(42, ff(30))
  assertEquals(o, receiver)
  receiver = 333
  assertEquals(32, Function.prototype.apply.call(ff, {}, [20]))
  assertEquals(o, receiver)

  Object.freeze(f)
  receiver = 333
  assertEquals(42, f(11, 31))
// TODO(rossberg): unrelated bug: this does not succeed for optimized code.
// assertEquals(isStrict ? undefined : global, receiver)
  receiver = 333
  assertEquals(42, Function.prototype.call.call(f, o, 20, 22))
  assertEquals(o, receiver)
  receiver = 333
  assertEquals(32, Function.prototype.apply.call(f, o, [17, 15]))
  assertEquals(o, receiver)
  receiver = 333
  assertEquals(42, ff(30))
  assertEquals(o, receiver)
  receiver = 333
  assertEquals(32, Function.prototype.apply.call(ff, {}, [20]))
  assertEquals(o, receiver)
}

TestCall(false, function(x, y) {
  receiver = this; return x + y
})

TestCall(true, function(x, y) {
  "use strict";
  receiver = this; return x + y
})

TestCall(false, Proxy.createFunction({}, function(x, y) {
  receiver = this; return x + y
}))

TestCall(true, Proxy.createFunction({}, function(x, y) {
  "use strict";
  receiver = this; return x + y
}))

var p = Proxy.createFunction({fix: function() {return {}}}, function(x, y) {
  receiver = this; return x + y
})
TestCall(false, p)
Object.freeze(p)
TestCall(false, p)


function TestCallThrow(callTrap) {
  var f = Proxy.createFunction({fix: function() {return {}}}, callTrap)
  assertThrows(function(){ f(11) }, "myexn")
  assertThrows(function(){ Function.prototype.call.call(f, {}, 2) }, "myexn")
  assertThrows(function(){ Function.prototype.apply.call(f, {}, [1]) }, "myexn")

  Object.freeze(f)
  assertThrows(function(){ f(11) }, "myexn")
  assertThrows(function(){ Function.prototype.call.call(f, {}, 2) }, "myexn")
  assertThrows(function(){ Function.prototype.apply.call(f, {}, [1]) }, "myexn")
}

TestCallThrow(function() { throw "myexn" })
TestCallThrow(Proxy.createFunction({}, function() { throw "myexn" }))

var p = Proxy.createFunction(
  {fix: function() {return {}}}, function() { throw "myexn" })
Object.freeze(p)
TestCallThrow(p)
