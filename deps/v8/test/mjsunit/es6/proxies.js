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

// We change the stack size for the ARM64 simulator because at one point this
// test enters an infinite recursion which goes through the runtime and we
// overflow the system stack before the simulator stack.

// Flags: --sim-stack-size=500 --allow-natives-syntax


// Helper.

function TestWithProxies(test, x, y, z) {
  // Separate function for nicer stack traces.
  TestWithObjectProxy(test, x, y, z);
  TestWithFunctionProxy(test, x, y, z);
}

function TestWithObjectProxy(test, x, y, z) {
  test((handler) => { return new Proxy({}, handler) }, x, y, z)

}

function TestWithFunctionProxy(test, x, y, z) {
  test((handler) => { return new Proxy(() => {}, handler) }, x, y, z)
}

// ---------------------------------------------------------------------------
// Getting property descriptors (Object.getOwnPropertyDescriptor).

var key

function TestGetOwnProperty(handler) {
  TestWithProxies(TestGetOwnProperty2, handler)
}

function TestGetOwnProperty2(create, handler) {
  var p = create(handler)
  assertEquals(42, Object.getOwnPropertyDescriptor(p, "a").value)
  assertEquals("a", key)
  assertEquals(42, Object.getOwnPropertyDescriptor(p, 99).value)
  assertEquals("99", key)
}

TestGetOwnProperty({
  getOwnPropertyDescriptor(target, k) {
    key = k
    return {value: 42, configurable: true}
  }
})

TestGetOwnProperty({
  getOwnPropertyDescriptor(target, k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2(k) {
    key = k
    return {value: 42, configurable: true}
  }
})

TestGetOwnProperty({
  getOwnPropertyDescriptor(target, k) {
    key = k
    return {get value() { return 42 }, get configurable() { return true }}
  }
})

TestGetOwnProperty(new Proxy({}, {
  get(target, pk, receiver) {
    return function(t, k) { key = k; return {value: 42, configurable: true} }
  }
}))


// ---------------------------------------------------------------------------
function TestGetOwnPropertyThrow(handler) {
  TestWithProxies(TestGetOwnPropertyThrow2, handler)
}

function TestGetOwnPropertyThrow2(create, handler) {
  var p = create(handler)
  assertThrowsEquals(() => Object.getOwnPropertyDescriptor(p, "a"), "myexn")
  assertThrowsEquals(() => Object.getOwnPropertyDescriptor(p, 77), "myexn")
}

TestGetOwnPropertyThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" }
})

TestGetOwnPropertyThrow({
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { throw "myexn" }
})

TestGetOwnPropertyThrow({
  getOwnPropertyDescriptor: function(k) {
    return {get value() { throw "myexn" }}
  }
})

TestGetOwnPropertyThrow(new Proxy({}, {
  get: function(pr, pk) {
    return function(k) { throw "myexn" }
  }
}))


// ---------------------------------------------------------------------------
// Getters (dot, brackets).

var key

function TestGet(handler) {
  TestWithProxies(TestGet2, handler)
}

function TestGet2(create, handler) {
  var p = create(handler)
  assertEquals(42, p.a)
  assertEquals("a", key)
  assertEquals(42, p["b"])
  assertEquals("b", key)
  assertEquals(42, p[99])
  assertEquals("99", key)
  assertEquals(42, (function(n) { return p[n] })("c"))
  assertEquals("c", key)
  assertEquals(42, (function(n) { return p[n] })(101))
  assertEquals("101", key)

  var o = Object.create(p, {x: {value: 88}})
  assertEquals(42, o.a)
  assertEquals("a", key)
  assertEquals(42, o["b"])
  assertEquals("b", key)
  assertEquals(42, o[99])
  assertEquals("99", key)
  assertEquals(88, o.x)
  assertEquals(88, o["x"])
  assertEquals(42, (function(n) { return o[n] })("c"))
  assertEquals("c", key)
  assertEquals(42, (function(n) { return o[n] })(101))
  assertEquals("101", key)
  assertEquals(88, (function(n) { return o[n] })("x"))
}

TestGet({
  get(t, k, r) { key = k; return 42 }
})

TestGet({
  get(t, k, r) { return this.get2(r, k) },
  get2(r, k) { key = k; return 42 }
})

TestGet(new Proxy({}, {
  get(pt, pk, pr) {
    return function(t, k, r) { key = k; return 42 }
  }
}))


// ---------------------------------------------------------------------------
function TestGetCall(handler) {
  TestWithProxies(TestGetCall2, handler)
}

function TestGetCall2(create, handler) {
  var p = create(handler)
  assertEquals(55, p.f())
  assertEquals(55, p["f"]())
  assertEquals(55, p.f("unused", "arguments"))
  assertEquals(55, p.f.call(p))
  assertEquals(55, p["f"].call(p))
  assertEquals(55, p[101].call(p))
  assertEquals(55, p.withargs(45, 5))
  assertEquals(55, p.withargs.call(p, 11, 22))
  assertEquals(55, (function(n) { return p[n]() })("f"))
  assertEquals(55, (function(n) { return p[n].call(p) })("f"))
  assertEquals(55, (function(n) { return p[n](15, 20) })("withargs"))
  assertEquals(55, (function(n) { return p[n].call(p, 13, 21) })("withargs"))
  assertEquals("6655", "66" + p)  // calls p.toString

  var o = Object.create(p, {g: {value: function(x) { return x + 88 }}})
  assertEquals(55, o.f())
  assertEquals(55, o["f"]())
  assertEquals(55, o.f("unused", "arguments"))
  assertEquals(55, o.f.call(o))
  assertEquals(55, o.f.call(p))
  assertEquals(55, o["f"].call(p))
  assertEquals(55, o[101].call(p))
  assertEquals(55, o.withargs(45, 5))
  assertEquals(55, o.withargs.call(p, 11, 22))
  assertEquals(90, o.g(2))
  assertEquals(91, o.g.call(o, 3))
  assertEquals(92, o.g.call(p, 4))
  assertEquals(55, (function(n) { return o[n]() })("f"))
  assertEquals(55, (function(n) { return o[n].call(o) })("f"))
  assertEquals(55, (function(n) { return o[n](15, 20) })("withargs"))
  assertEquals(55, (function(n) { return o[n].call(o, 13, 21) })("withargs"))
  assertEquals(93, (function(n) { return o[n](5) })("g"))
  assertEquals(94, (function(n) { return o[n].call(o, 6) })("g"))
  assertEquals(95, (function(n) { return o[n].call(p, 7) })("g"))
  assertEquals("6655", "66" + o)  // calls o.toString
}

TestGetCall({
  get(t, k, r) { return () => { return 55 } }
})

TestGetCall({
  get(t, k, r)  { return this.get2(t, k, r) },
  get2(t, k, r) { return () => { return 55 } }
})

TestGetCall({
  get(t, k, r) {
    if (k == "gg") {
      return () => { return 55 }
    } else if (k == "withargs") {
      return (n, m) => { return n + m * 2 }
    } else {
      return () => { return r.gg() }
    }
  }
})

TestGetCall(new Proxy({}, {
  get(pt, pk, pr) {
    return (t, k, r) => { return () => { return 55 } }
  }
}))


// ---------------------------------------------------------------------------
function TestGetThrow(handler) {
  TestWithProxies(TestGetThrow2, handler)
}

function TestGetThrow2(create, handler) {
  var p = create(handler)
  assertThrowsEquals(function(){ p.a }, "myexn")
  assertThrowsEquals(function(){ p["b"] }, "myexn")
  assertThrowsEquals(function(){ p[3] }, "myexn")
  assertThrowsEquals(function(){ (function(n) { p[n] })("c") }, "myexn")
  assertThrowsEquals(function(){ (function(n) { p[n] })(99) }, "myexn")

  var o = Object.create(p, {x: {value: 88}, '4': {value: 89}})
  assertThrowsEquals(function(){ o.a }, "myexn")
  assertThrowsEquals(function(){ o["b"] }, "myexn")
  assertThrowsEquals(function(){ o[3] }, "myexn")
  assertThrowsEquals(function(){ (function(n) { o[n] })("c") }, "myexn")
  assertThrowsEquals(function(){ (function(n) { o[n] })(99) }, "myexn")
}

TestGetThrow({
  get(r, k) { throw "myexn" }
})

TestGetThrow({
  get(r, k) { return this.get2(r, k) },
  get2(r, k) { throw "myexn" }
})

TestGetThrow(new Proxy({}, {
  get(pr, pk) { throw "myexn" }
}))

TestGetThrow(new Proxy({}, {
  get(pr, pk) {
    return function(r, k) { throw "myexn" }
  }
}))


// ---------------------------------------------------------------------------
// Setters.

var key
var val

function TestSet(handler) {
  TestWithProxies(TestSet2, handler)
}

function TestSet2(create, handler) {
  var p = create(handler)
  assertEquals(42, p.a = 42)
  assertEquals("a", key)
  assertEquals(42, val)
  assertEquals(43, p["b"] = 43)
  assertEquals("b", key)
  assertEquals(43, val)
  assertEquals(44, p[77] = 44)
  assertEquals("77", key)
  assertEquals(44, val)

  assertEquals(45, (function(n) { return p[n] = 45 })("c"))
  assertEquals("c", key)
  assertEquals(45, val)
  assertEquals(46, (function(n) { return p[n] = 46 })(99))
  assertEquals("99", key)
  assertEquals(46, val)

  assertEquals(47, p["0"] = 47)
  assertEquals("0", key)
  assertEquals(47, val)
}

TestSet({
  set: function(r, k, v) { key = k; val = v; return true }
})

TestSet({
  set: function(r, k, v) { return this.set2(r, k, v) },
  set2: function(r, k, v) { key = k; val = v; return true }
})

TestSet(new Proxy({}, {
  get(pk, pr) {
    return (r, k, v) => { key = k; val = v; return true }
  }
}))


// ---------------------------------------------------------------------------
function TestSetThrow(handler) {
  TestWithProxies(TestSetThrow2, handler)
}

function TestSetThrow2(create, handler) {
  var p = create(handler)
  assertThrowsEquals(function(){ p.a = 42 }, "myexn")
  assertThrowsEquals(function(){ p["b"] = 42 }, "myexn")
  assertThrowsEquals(function(){ p[22] = 42 }, "myexn")
  assertThrowsEquals(function(){ (function(n) { p[n] = 45 })("c") }, "myexn")
  assertThrowsEquals(function(){ (function(n) { p[n] = 46 })(99) }, "myexn")
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
  getOwnPropertyDescriptor: function(k) {
    return {configurable: true, writable: true}
  },
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
  getOwnPropertyDescriptor2: function(k) {
    return {configurable: true, writable: true}
  },
  defineProperty: function(k, desc) { this.defineProperty2(k, desc) },
  defineProperty2: function(k, desc) { throw "myexn" }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" },
  defineProperty: function(k, desc) { key = k; val = desc.value }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) {
    return {
      get configurable() { return true },
      get writable() { return true }
    }
  },
  defineProperty: function(k, desc) { throw "myexn" }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" }
})

TestSetThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" },
  defineProperty: function(k, desc) { key = k; val = desc.value }
})

TestSetThrow(new Proxy({}, {
  get: function(pr, pk) { throw "myexn" }
}))

TestSetThrow(new Proxy({}, {
  get: function(pr, pk) {
    return function(r, k, v) { throw "myexn" }
  }
}))

// ---------------------------------------------------------------------------

// Evil proxy-induced side-effects shouldn't crash.
TestWithProxies(function(create) {
  var calls = 0
  var handler = {
    getPropertyDescriptor: function() {
      ++calls
      return (calls % 2 == 1)
        ? {get: function() { return 5 }, configurable: true}
        : {set: function() { return false }, configurable: true}
    }
  }
  var p = create(handler)
  var o = Object.create(p)
  // Make proxy prototype property read-only after CanPut check.
  try { o.x = 4 } catch (e) { assertInstanceof(e, Error) }
})

TestWithProxies(function(create) {
  var handler = {
    getPropertyDescriptor: function() {
      Object.defineProperty(o, "x", {get: function() { return 5 }});
      return {set: function() {}}
    }
  }
  var p = create(handler)
  var o = Object.create(p)
  // Make object property read-only after CanPut check.
  try { o.x = 4 } catch (e) { assertInstanceof(e, Error) }
})


// ---------------------------------------------------------------------------
// Property definition (Object.defineProperty and Object.defineProperties).

var key
var desc

function TestDefine(handler) {
  TestWithProxies(TestDefine2, handler)
}

function TestDefine2(create, handler) {
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

  assertEquals(p, Object.defineProperty(p, 101, {value: 47, enumerable: false}))
  assertEquals("101", key)
  assertEquals(2, Object.getOwnPropertyNames(desc).length)
  assertEquals(47, desc.value)
  assertEquals(false, desc.enumerable)

  var attributes = {configurable: true, mine: 66, minetoo: 23}
  assertEquals(p, Object.defineProperty(p, "d", attributes))
  assertEquals("d", key);
  // Modifying the attributes object after the fact should have no effect.
  attributes.configurable = false
  attributes.mine = 77
  delete attributes.minetoo;
  assertEquals(1, Object.getOwnPropertyNames(desc).length)
  assertEquals(true, desc.configurable)
  assertEquals(undefined, desc.mine)
  assertEquals(undefined, desc.minetoo)

  assertEquals(p, Object.defineProperty(p, "e", {get: function(){ return 5 }}))
  assertEquals("e", key)
  assertEquals(1, Object.getOwnPropertyNames(desc).length)
  assertEquals(5, desc.get())

  assertEquals(p, Object.defineProperty(p, "zzz", {}))
  assertEquals("zzz", key)
  assertEquals(0, Object.getOwnPropertyNames(desc).length)

  var props = {
    '11': {},
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
  assertThrowsEquals(function(){ Object.defineProperties(p, props) }, "myexn")
}

TestDefine({
  defineProperty(t, k, d) { key = k; desc = d; return true }
})

TestDefine({
  defineProperty(t, k, d) { return this.defineProperty2(k, d) },
  defineProperty2(k, d) { key = k; desc = d; return true }
})


// ---------------------------------------------------------------------------
function TestDefineThrow(handler) {
  TestWithProxies(TestDefineThrow2, handler)
}

function TestDefineThrow2(create, handler) {
  var p = create(handler)
  assertThrowsEquals(() => Object.defineProperty(p, "a", {value: 44}), "myexn")
  assertThrowsEquals(() => Object.defineProperty(p, 0, {value: 44}), "myexn")

  var d1 = create({
    get: function(r, k) { throw "myexn" },
    getOwnPropertyNames: function() { return ["value"] }
  })
  assertThrowsEquals(function(){ Object.defineProperty(p, "p", d1) }, "myexn")
  var d2 = create({
    get: function(r, k) { return 77 },
    getOwnPropertyNames: function() { throw "myexn" }
  })
  assertThrowsEquals(function(){ Object.defineProperty(p, "p", d2) }, "myexn")

  var props = {bla: {get value() { throw "otherexn" }}}
  assertThrowsEquals(() => Object.defineProperties(p, props), "otherexn")
}

TestDefineThrow({
  defineProperty: function(k, d) { throw "myexn" }
})

TestDefineThrow({
  defineProperty: function(k, d) { return this.defineProperty2(k, d) },
  defineProperty2: function(k, d) { throw "myexn" }
})

TestDefineThrow(new Proxy({}, {
  get: function(pr, pk) { throw "myexn" }
}))

TestDefineThrow(new Proxy({}, {
  get: function(pr, pk) {
    return function(k, d) { throw "myexn" }
  }
}))



// ---------------------------------------------------------------------------
// Property deletion (delete).

var key

function TestDelete(handler) {
  TestWithProxies(TestDelete2, handler)
}

function TestDelete2(create, handler) {
  var p = create(handler)
  assertEquals(true, delete p.a)
  assertEquals("a", key)
  assertEquals(true, delete p["b"])
  assertEquals("b", key)
  assertEquals(true, delete p[1])
  assertEquals("1", key)

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
    assertEquals(true, delete p[2])
    assertEquals("2", key)

    assertThrows(function(){ delete p.z3 }, TypeError)
    assertEquals("z3", key)
    assertThrows(function(){ delete p["z4"] }, TypeError)
    assertEquals("z4", key)
  })()
}

TestDelete({
  deleteProperty(target, k) { key = k; return k < "z" }
})

TestDelete({
  deleteProperty(target, k) { return this.delete2(k) },
  delete2: function(k) { key = k; return k < "z" }
})

TestDelete(new Proxy({}, {
  get(pt, pk, pr) {
    return (target, k) => { key = k; return k < "z" }
  }
}))


// ---------------------------------------------------------------------------
function TestDeleteThrow(handler) {
  TestWithProxies(TestDeleteThrow2, handler)
}

function TestDeleteThrow2(create, handler) {
  var p = create(handler)
  assertThrowsEquals(function(){ delete p.a }, "myexn")
  assertThrowsEquals(function(){ delete p["b"] }, "myexn");
  assertThrowsEquals(function(){ delete p[3] }, "myexn");

  (function() {
    "use strict"
    assertThrowsEquals(function(){ delete p.c }, "myexn")
    assertThrowsEquals(function(){ delete p["d"] }, "myexn")
    assertThrowsEquals(function(){ delete p[4] }, "myexn");
  })()
}

TestDeleteThrow({
  deleteProperty(t, k) { throw "myexn" }
})

TestDeleteThrow({
  deleteProperty(t, k) { return this.delete2(k) },
  delete2(k) { throw "myexn" }
})

TestDeleteThrow(new Proxy({}, {
  get(pt, pk, pr) { throw "myexn" }
}))

TestDeleteThrow(new Proxy({}, {
  get(pt, pk, pr) {
    return (k) => { throw "myexn" }
  }
}))


// ---------------------------------------------------------------------------
// Property descriptors (Object.getOwnPropertyDescriptor).

function TestDescriptor(handler) {
  TestWithProxies(TestDescriptor2, handler)
}

function TestDescriptor2(create, handler) {
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
  defineProperty(t, k, d) { this["__" + k] = d; return true },
  getOwnPropertyDescriptor(t, k) { return this["__" + k] }
})

TestDescriptor({
  defineProperty(t, k, d) { this["__" + k] = d; return true },
  getOwnPropertyDescriptor(t, k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { return this["__" + k] }
})


// ---------------------------------------------------------------------------
function TestDescriptorThrow(handler) {
  TestWithProxies(TestDescriptorThrow2, handler)
}

function TestDescriptorThrow2(create, handler) {
  var p = create(handler)
  assertThrowsEquals(() => Object.getOwnPropertyDescriptor(p, "a"), "myexn")
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



// ---------------------------------------------------------------------------
// Comparison.

function TestComparison(eq) {
  TestWithProxies(TestComparison2, eq)
}

function TestComparison2(create, eq) {
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
  assertEquals("object", typeof new Proxy({},{}))
  assertTrue(typeof new Proxy({}, {}) == "object")
  assertTrue("object" == typeof new Proxy({},{}))

  assertEquals("function", typeof new Proxy(function() {}, {}))
  assertTrue(typeof new Proxy(function() {}, {}) == "function")
  assertTrue("function" == typeof new Proxy(function() {},{}))
}

TestTypeof()



// ---------------------------------------------------------------------------
// Membership test (in).

var key

function TestIn(handler) {
  TestWithProxies(TestIn2, handler)
}

function TestIn2(create, handler) {
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

  // Test compilation in conditionals.
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
  has(t, k) { key = k; return k < "z" }
})

TestIn({
  has(t, k) { return this.has2(k) },
  has2(k) { key = k; return k < "z" }
})

TestIn(new Proxy({},{
  get(pt, pk, pr) {
    return (t, k) => { key = k; return k < "z" }
  }
}))


// ---------------------------------------------------------------------------
function TestInThrow(handler) {
  TestWithProxies(TestInThrow2, handler)
}

function TestInThrow2(create, handler) {
  var p = create(handler)
  assertThrowsEquals(function(){ return "a" in p }, "myexn")
  assertThrowsEquals(function(){ return 99 in p }, "myexn")
  assertThrowsEquals(function(){ return !("a" in p) }, "myexn")
  assertThrowsEquals(function(){ return ("a" in p) ? 2 : 3 }, "myexn")
  assertThrowsEquals(function(){ if ("b" in p) {} }, "myexn")
  assertThrowsEquals(function(){ if (!("b" in p)) {} }, "myexn")
  assertThrowsEquals(function(){ if ("zzz" in p) {} }, "myexn")
}

TestInThrow({
  has: function(k) { throw "myexn" }
})

TestInThrow({
  has: function(k) { return this.has2(k) },
  has2: function(k) { throw "myexn" }
})

TestInThrow(new Proxy({},{
  get: function(pr, pk) { throw "myexn" }
}))

TestInThrow(new Proxy({},{
  get: function(pr, pk) {
    return function(k) { throw "myexn" }
  }
}))



// ---------------------------------------------------------------------------
// Own Properties (Object.prototype.hasOwnProperty).

var key

function TestHasOwn(handler) {
  TestWithProxies(TestHasOwn2, handler)
}

function TestHasOwn2(create, handler) {
  var p = create(handler)
  assertTrue(Object.prototype.hasOwnProperty.call(p, "a"))
  assertEquals("a", key)
  assertTrue(Object.prototype.hasOwnProperty.call(p, 99))
  assertEquals("99", key)
  assertFalse(Object.prototype.hasOwnProperty.call(p, "z"))
  assertEquals("z", key)
}

TestHasOwn({
  getOwnPropertyDescriptor(t, k) {
    key = k; if (k < "z") return {configurable: true}
  },
  has() { assertUnreachable() }
})

TestHasOwn({
  getOwnPropertyDescriptor(t, k) { return this.getOwnPropertyDescriptor2(k) },
  getOwnPropertyDescriptor2(k) {
    key = k; if (k < "z") return {configurable: true}
  }
})



// ---------------------------------------------------------------------------
function TestHasOwnThrow(handler) {
  TestWithProxies(TestHasOwnThrow2, handler)
}

function TestHasOwnThrow2(create, handler) {
  var p = create(handler)
  assertThrowsEquals(function(){ Object.prototype.hasOwnProperty.call(p, "a")},
    "myexn")
  assertThrowsEquals(function(){ Object.prototype.hasOwnProperty.call(p, 99)},
    "myexn")
}

TestHasOwnThrow({
  getOwnPropertyDescriptor(t, k) { throw "myexn" }
})

TestHasOwnThrow({
  getOwnPropertyDescriptor(t, k) { return this.getOwnPropertyDescriptor2(k) },
  getOwnPropertyDescriptor2(k) { throw "myexn" }
});


// ---------------------------------------------------------------------------
// Instanceof (instanceof)

(function TestProxyInstanceof() {
  var o1 = {}
  var p1 = new Proxy({}, {})
  var p2 = new Proxy(o1, {})
  var p3 = new Proxy(p2, {})
  var o2 = Object.create(p2)

  var f0 = function() {}
  f0.prototype = o1
  var f1 = function() {}
  f1.prototype = p1
  var f2 = function() {}
  f2.prototype = p2
  var f3 = function() {}
  f3.prototype = o2

  assertTrue(o1 instanceof Object)
  assertFalse(o1 instanceof f0)
  assertFalse(o1 instanceof f1)
  assertFalse(o1 instanceof f2)
  assertFalse(o1 instanceof f3)
  assertTrue(p1 instanceof Object)
  assertFalse(p1 instanceof f0)
  assertFalse(p1 instanceof f1)
  assertFalse(p1 instanceof f2)
  assertFalse(p1 instanceof f3)
  assertTrue(p2 instanceof Object)
  assertFalse(p2 instanceof f0)
  assertFalse(p2 instanceof f1)
  assertFalse(p2 instanceof f2)
  assertFalse(p2 instanceof f3)
  assertTrue(p3 instanceof Object)
  assertFalse(p3 instanceof f0)
  assertFalse(p3 instanceof f1)
  assertFalse(p3 instanceof f2)
  assertFalse(p3 instanceof f3)
  assertTrue(o2 instanceof Object)
  assertFalse(o2 instanceof f0)
  assertFalse(o2 instanceof f1)
  assertTrue(o2 instanceof f2)
  assertFalse(o2 instanceof f3)

  var f = new Proxy(function() {}, {})
  assertTrue(f instanceof Function)
})();


(function TestInstanceofProxy() {
  var o0 = Object.create(null)
  var o1 = {}
  var o2 = Object.create(o0)
  var o3 = Object.create(o1)
  var o4 = Object.create(o2)
  var o5 = Object.create(o3)

  function handler(o) {
    return {
      get: function(r, p) {
        // We want to test prototype lookup, so ensure the proxy
        // offers OrdinaryHasInstance behavior.
        if (p === Symbol.hasInstance) {
          return undefined;
        }
        return o;
      }
    }
  }

  var f0 = new Proxy(function() {}, handler(o0))
  var f1 = new Proxy(function() {}, handler(o1))
  var f2 = new Proxy(function() {}, handler(o2))
  var f3 = new Proxy(function() {}, handler(o3))
  var f4 = new Proxy(function() {}, handler(o4))
  var f5 = new Proxy(function() {}, handler(o4))

  assertFalse(null instanceof f0)
  assertFalse(o0 instanceof f0)
  assertFalse(o0 instanceof f1)
  assertFalse(o0 instanceof f2)
  assertFalse(o0 instanceof f3)
  assertFalse(o0 instanceof f4)
  assertFalse(o0 instanceof f5)
  assertFalse(o1 instanceof f0)
  assertFalse(o1 instanceof f1)
  assertFalse(o1 instanceof f2)
  assertFalse(o1 instanceof f3)
  assertFalse(o1 instanceof f4)
  assertFalse(o1 instanceof f5)
  assertTrue(o2 instanceof f0)
  assertFalse(o2 instanceof f1)
  assertFalse(o2 instanceof f2)
  assertFalse(o2 instanceof f3)
  assertFalse(o2 instanceof f4)
  assertFalse(o2 instanceof f5)
  assertFalse(o3 instanceof f0)
  assertTrue(o3 instanceof f1)
  assertFalse(o3 instanceof f2)
  assertFalse(o3 instanceof f3)
  assertFalse(o3 instanceof f4)
  assertFalse(o3 instanceof f5)
  assertTrue(o4 instanceof f0)
  assertFalse(o4 instanceof f1)
  assertTrue(o4 instanceof f2)
  assertFalse(o4 instanceof f3)
  assertFalse(o4 instanceof f4)
  assertFalse(o4 instanceof f5)
  assertFalse(o5 instanceof f0)
  assertTrue(o5 instanceof f1)
  assertFalse(o5 instanceof f2)
  assertTrue(o5 instanceof f3)
  assertFalse(o5 instanceof f4)
  assertFalse(o5 instanceof f5)

  var f = new Proxy(function() {}, {})
  var ff = new Proxy(function() {}, handler(Function))
  assertTrue(f instanceof Function)
  assertFalse(f instanceof ff)
})();


// ---------------------------------------------------------------------------
// Prototype (Object.getPrototypeOf, Object.prototype.isPrototypeOf).

(function TestPrototype() {
  var o1 = {}
  var p1 = new Proxy({}, {})
  var p2 = new Proxy(o1, {})
  var p3 = new Proxy(p2, {})
  var o2 = Object.create(p3)

  assertSame(Object.getPrototypeOf(o1), Object.prototype)
  assertSame(Object.getPrototypeOf(p1), Object.prototype)
  assertSame(Object.getPrototypeOf(p2), Object.prototype)
  assertSame(Object.getPrototypeOf(p3), Object.prototype)
  assertSame(Object.getPrototypeOf(o2), p3)

  assertTrue(Object.prototype.isPrototypeOf(o1))
  assertTrue(Object.prototype.isPrototypeOf(p1))
  assertTrue(Object.prototype.isPrototypeOf(p2))
  assertTrue(Object.prototype.isPrototypeOf(p3))
  assertTrue(Object.prototype.isPrototypeOf(o2))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, o1))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, p1))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, p2))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, p3))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, o2))
  assertFalse(Object.prototype.isPrototypeOf.call(o1, o1))
  assertFalse(Object.prototype.isPrototypeOf.call(o1, p1))
  assertFalse(Object.prototype.isPrototypeOf.call(o1, p2))
  assertFalse(Object.prototype.isPrototypeOf.call(o1, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(o1, o2))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, p1))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, o1))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, p2))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, o2))
  assertFalse(Object.prototype.isPrototypeOf.call(p2, p1))
  assertFalse(Object.prototype.isPrototypeOf.call(p2, p2))
  assertFalse(Object.prototype.isPrototypeOf.call(p2, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(p2, o2))
  assertFalse(Object.prototype.isPrototypeOf.call(p3, p2))
  assertTrue(Object.prototype.isPrototypeOf.call(p3, o2))
  assertFalse(Object.prototype.isPrototypeOf.call(o2, o1))
  assertFalse(Object.prototype.isPrototypeOf.call(o2, p1))
  assertFalse(Object.prototype.isPrototypeOf.call(o2, p2))
  assertFalse(Object.prototype.isPrototypeOf.call(o2, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(o2, o2))

  var f = new Proxy(function() {}, {})
  assertSame(Object.getPrototypeOf(f), Function.prototype)
  assertTrue(Object.prototype.isPrototypeOf(f))
  assertTrue(Object.prototype.isPrototypeOf.call(Function.prototype, f))
})();


// ---------------------------------------------------------------------------
function TestPropertyNamesThrow(handler) {
  TestWithProxies(TestPropertyNamesThrow2, handler)
}

function TestPropertyNamesThrow2(create, handler) {
  var p = create(handler)
  assertThrowsEquals(function(){ Object.getOwnPropertyNames(p) }, "myexn")
}

TestPropertyNamesThrow({
  ownKeys() { throw "myexn" }
})

TestPropertyNamesThrow({
  ownKeys() { return this.getOwnPropertyNames2() },
  getOwnPropertyNames2() { throw "myexn" }
})

// ---------------------------------------------------------------------------

function TestKeys(names, handler) {
  var p = new Proxy({}, handler);
  assertArrayEquals(names, Object.keys(p))
}

TestKeys([], {
  ownKeys() { return [] }
})

TestKeys([], {
  ownKeys() { return ["a", "zz", " ", "0", "toString"] }
})

TestKeys(["a", "zz", " ", "0", "toString"], {
  ownKeys() { return ["a", "zz", " ", "0", "toString"] },
  getOwnPropertyDescriptor(t, p) {
    return {configurable: true, enumerable: true}
  }
})

TestKeys([], {
  ownKeys() { return this.keys2() },
  keys2() { return ["throw", "function "] }
})

TestKeys(["throw", "function "], {
  ownKeys() { return this.keys2() },
  keys2() { return ["throw", "function "] },
  getOwnPropertyDescriptor(t, p) {
    return {configurable: true, enumerable: true}
  }
})

TestKeys(["a", "0"], {
  ownKeys() { return ["a", "23", "zz", "", "0"] },
  getOwnPropertyDescriptor(t, k) {
    return k == "" ?
        undefined :
        { configurable: true, enumerable: k.length == 1}
  }
})

TestKeys(["23", "zz", ""], {
  ownKeys() { return this.getOwnPropertyNames2() },
  getOwnPropertyNames2() { return ["a", "23", "zz", "", "0"] },
  getOwnPropertyDescriptor(t, k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2(k) {
    return {configurable: true, enumerable: k.length != 1 }
  }
})

TestKeys([], {
  get ownKeys() {
    return function() { return ["a", "b", "c"] }
  },
  getOwnPropertyDescriptor: function(k) { return {configurable: true} }
})


// ---------------------------------------------------------------------------
function TestKeysThrow(handler) {
  TestWithProxies(TestKeysThrow2, handler)
}

function TestKeysThrow2(create, handler) {
  var p = create(handler);
  assertThrowsEquals(function(){ Object.keys(p) }, "myexn");
}

TestKeysThrow({
  ownKeys() { throw "myexn" }
})

TestKeysThrow({
  ownKeys() { return this.keys2() },
  keys2() { throw "myexn" }
})

TestKeysThrow({
  ownKeys() { return ['1'] },
  getOwnPropertyDescriptor: function() { throw "myexn" },
})

TestKeysThrow({
  ownKeys() { return this.getOwnPropertyNames2() },
  getOwnPropertyNames2() { return ['1', '2'] },
  getOwnPropertyDescriptor(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2(k) { throw "myexn" }
})

TestKeysThrow({
  get ownKeys() { throw "myexn" }
})

TestKeysThrow({
  get ownKeys() {
    return function() { throw "myexn" }
  },
})

TestKeysThrow({
  get ownKeys() {
    return function() { return ['1', '2'] }
  },
  getOwnPropertyDescriptor(k) { throw "myexn" }
})



// ---------------------------------------------------------------------------
// String conversion (Object.prototype.toString,
//                    Object.prototype.toLocaleString,
//                    Function.prototype.toString)

var key

function TestToString(handler) {
  var p = new Proxy({}, handler)
  key = ""
  assertEquals("[object Object]", Object.prototype.toString.call(p))
  assertEquals(Symbol.toStringTag, key)
  assertEquals("my_proxy", Object.prototype.toLocaleString.call(p))
  assertEquals("toString", key)

  var f = new Proxy(function() {}, handler)
  key = ""
  assertEquals("[object Function]", Object.prototype.toString.call(f))
  assertEquals(Symbol.toStringTag, key)
  assertEquals("my_proxy", Object.prototype.toLocaleString.call(f))
  assertEquals("toString", key)
  assertThrows(function(){ Function.prototype.toString.call(f) })

  var o = Object.create(p)
  key = ""
  assertEquals("[object Object]", Object.prototype.toString.call(o))
  assertEquals(Symbol.toStringTag, key)
  assertEquals("my_proxy", Object.prototype.toLocaleString.call(o))
  assertEquals("toString", key)
}

TestToString({
  get: function(r, k) { key = k; return function() { return "my_proxy" } }
})

TestToString({
  get: function(r, k) { return this.get2(r, k) },
  get2: function(r, k) { key = k; return function() { return "my_proxy" } }
})

TestToString(new Proxy({}, {
  get: function(pr, pk) {
    return function(r, k) { key = k; return function() { return "my_proxy" } }
  }
}))


function TestToStringThrow(handler) {
  var p = new Proxy({}, handler)
  assertThrowsEquals(() => Object.prototype.toString.call(p), "myexn")
  assertThrowsEquals(() => Object.prototype.toLocaleString.call(p), "myexn")

  var f = new Proxy(function(){}, handler)
  assertThrowsEquals(() => Object.prototype.toString.call(f), "myexn")
  assertThrowsEquals(() => Object.prototype.toLocaleString.call(f), "myexn")

  var o = Object.create(p)
  assertThrowsEquals(() => Object.prototype.toString.call(o), "myexn")
  assertThrowsEquals(() => Object.prototype.toLocaleString.call(o), "myexn")
}

TestToStringThrow({
  get: function(r, k) { throw "myexn" }
})

TestToStringThrow({
  get: function(r, k) { return this.get2(r, k) },
  get2: function(r, k) { throw "myexn" }
})

TestToStringThrow(new Proxy({}, {
  get: function(pr, pk) { throw "myexn" }
}))

TestToStringThrow(new Proxy({}, {
  get: function(pr, pk) {
    return function(r, k) { throw "myexn" }
  }
}))


// ---------------------------------------------------------------------------
// Value conversion (Object.prototype.toValue)

function TestValueOf(handler) {
  TestWithProxies(TestValueOf2, handler)
}

function TestValueOf2(create, handler) {
  var p = create(handler)
  assertSame(p, Object.prototype.valueOf.call(p))
}

TestValueOf({})



// ---------------------------------------------------------------------------
// Enumerability (Object.prototype.propertyIsEnumerable)

var key

function TestIsEnumerable(handler) {
  TestWithProxies(TestIsEnumerable2, handler)
}

function TestIsEnumerable2(create, handler) {
  var p = create(handler)
  assertTrue(Object.prototype.propertyIsEnumerable.call(p, "a"))
  assertEquals("a", key)
  assertTrue(Object.prototype.propertyIsEnumerable.call(p, 2))
  assertEquals("2", key)
  assertFalse(Object.prototype.propertyIsEnumerable.call(p, "z"))
  assertEquals("z", key)

  var o = Object.create(p)
  key = ""
  assertFalse(Object.prototype.propertyIsEnumerable.call(o, "a"))
  assertEquals("", key)  // trap not invoked
}

TestIsEnumerable({
  getOwnPropertyDescriptor(t, k) {
    key = k;
    return {enumerable: k < "z", configurable: true}
  },
})

TestIsEnumerable({
  getOwnPropertyDescriptor: function(t, k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) {
    key = k;
    return {enumerable: k < "z", configurable: true}
  },
})

TestIsEnumerable({
  getOwnPropertyDescriptor: function(t, k) {
    key = k;
    return {get enumerable() { return k < "z" }, configurable: true}
  },
})

TestIsEnumerable(new Proxy({}, {
  get: function(pt, pk, pr) {
    return function(t, k) {
      key = k;
      return {enumerable: k < "z", configurable: true}
    }
  }
}))


// ---------------------------------------------------------------------------
function TestIsEnumerableThrow(handler) {
  TestWithProxies(TestIsEnumerableThrow2, handler)
}

function TestIsEnumerableThrow2(create, handler) {
  var p = create(handler)
  assertThrowsEquals(() => Object.prototype.propertyIsEnumerable.call(p, "a"),
      "myexn")
  assertThrowsEquals(() => Object.prototype.propertyIsEnumerable.call(p, 11),
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

TestIsEnumerableThrow(new Proxy({}, {
  get: function(pr, pk) { throw "myexn" }
}))

TestIsEnumerableThrow(new Proxy({}, {
  get: function(pr, pk) {
    return function(k) { throw "myexn" }
  }
}));



// ---------------------------------------------------------------------------
// Constructor functions with proxy prototypes.

(function TestConstructorWithProxyPrototype() {
  TestWithProxies(TestConstructorWithProxyPrototype2, {})
})();

function TestConstructorWithProxyPrototype2(create, handler) {
  function C() {};
  C.prototype = create(handler);

  var o = new C;
  assertSame(C.prototype, Object.getPrototypeOf(o));
};


(function TestOptWithProxyPrototype() {
  var handler = {
    get(t, k) {
      return 10;
    }
  };

  function C() {};
  C.prototype = new Proxy({}, handler);
  var o = new C();

  function f() {
    return o.x;
  }
  assertEquals(10, f());
  assertEquals(10, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(10, f());
})();
