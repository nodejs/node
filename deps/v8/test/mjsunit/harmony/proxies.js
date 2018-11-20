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

// Flags: --harmony-proxies --sim-stack-size=500 --turbo-deoptimization


// Helper.

function TestWithProxies(test, x, y, z) {
  test(Proxy.create, x, y, z)
  test(function(h) {return Proxy.createFunction(h, function() {})}, x, y, z)
}



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
  getOwnPropertyDescriptor: function(k) {
    key = k
    return {value: 42, configurable: true}
  }
})

TestGetOwnProperty({
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) {
    key = k
    return {value: 42, configurable: true}
  }
})

TestGetOwnProperty({
  getOwnPropertyDescriptor: function(k) {
    key = k
    return {get value() { return 42 }, get configurable() { return true }}
  }
})

TestGetOwnProperty(Proxy.create({
  get: function(pr, pk) {
    return function(k) { key = k; return {value: 42, configurable: true} }
  }
}))


function TestGetOwnPropertyThrow(handler) {
  TestWithProxies(TestGetOwnPropertyThrow2, handler)
}

function TestGetOwnPropertyThrow2(create, handler) {
  var p = create(handler)
  assertThrows(function(){ Object.getOwnPropertyDescriptor(p, "a") }, "myexn")
  assertThrows(function(){ Object.getOwnPropertyDescriptor(p, 77) }, "myexn")
}

TestGetOwnPropertyThrow({
  getOwnPropertyDescriptor: function(k) { throw "myexn" }
})

TestGetOwnPropertyThrow({
  getOwnPropertyDescriptor: function(k) {
    return this.getPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) { throw "myexn" }
})

TestGetOwnPropertyThrow({
  getOwnPropertyDescriptor: function(k) {
    return {get value() { throw "myexn" }}
  }
})

TestGetOwnPropertyThrow(Proxy.create({
  get: function(pr, pk) {
    return function(k) { throw "myexn" }
  }
}))



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
  get: function(r, k) { key = k; return 42 }
})

TestGet({
  get: function(r, k) { return this.get2(r, k) },
  get2: function(r, k) { key = k; return 42 }
})

TestGet({
  getPropertyDescriptor: function(k) { key = k; return {value: 42} }
})

TestGet({
  getPropertyDescriptor: function(k) { return this.getPropertyDescriptor2(k) },
  getPropertyDescriptor2: function(k) { key = k; return {value: 42} }
})

TestGet({
  getPropertyDescriptor: function(k) {
    key = k;
    return {get value() { return 42 }}
  }
})

TestGet({
  get: undefined,
  getPropertyDescriptor: function(k) { key = k; return {value: 42} }
})

TestGet(Proxy.create({
  get: function(pr, pk) {
    return function(r, k) { key = k; return 42 }
  }
}))


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

function TestGetThrow2(create, handler) {
  var p = create(handler)
  assertThrows(function(){ p.a }, "myexn")
  assertThrows(function(){ p["b"] }, "myexn")
  assertThrows(function(){ p[3] }, "myexn")
  assertThrows(function(){ (function(n) { p[n] })("c") }, "myexn")
  assertThrows(function(){ (function(n) { p[n] })(99) }, "myexn")

  var o = Object.create(p, {x: {value: 88}, '4': {value: 89}})
  assertThrows(function(){ o.a }, "myexn")
  assertThrows(function(){ o["b"] }, "myexn")
  assertThrows(function(){ o[3] }, "myexn")
  assertThrows(function(){ (function(n) { o[n] })("c") }, "myexn")
  assertThrows(function(){ (function(n) { o[n] })(99) }, "myexn")
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


function TestSetThrow(handler) {
  TestWithProxies(TestSetThrow2, handler)
}

function TestSetThrow2(create, handler) {
  var p = create(handler)
  assertThrows(function(){ p.a = 42 }, "myexn")
  assertThrows(function(){ p["b"] = 42 }, "myexn")
  assertThrows(function(){ p[22] = 42 }, "myexn")
  assertThrows(function(){ (function(n) { p[n] = 45 })("c") }, "myexn")
  assertThrows(function(){ (function(n) { p[n] = 46 })(99) }, "myexn")
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


var rec
var key
var val

function TestSetForDerived(trap) {
  TestWithProxies(TestSetForDerived2, trap)
}

function TestSetForDerived2(create, trap) {
  var p = create({getPropertyDescriptor: trap, getOwnPropertyDescriptor: trap})
  var o = Object.create(p, {x: {value: 88, writable: true},
                            '1': {value: 89, writable: true}})

  key = ""
  assertEquals(48, o.x = 48)
  assertEquals("", key)  // trap not invoked
  assertEquals(48, o.x)

  assertEquals(47, o[1] = 47)
  assertEquals("", key)  // trap not invoked
  assertEquals(47, o[1])

  assertEquals(49, o.y = 49)
  assertEquals("y", key)
  assertEquals(49, o.y)

  assertEquals(50, o[2] = 50)
  assertEquals("2", key)
  assertEquals(50, o[2])

  assertEquals(44, o.p_writable = 44)
  assertEquals("p_writable", key)
  assertEquals(44, o.p_writable)

  assertEquals(45, o.p_nonwritable = 45)
  assertEquals("p_nonwritable", key)
  assertFalse(Object.prototype.hasOwnProperty.call(o, "p_nonwritable"))

  assertThrows(function(){ "use strict"; o.p_nonwritable = 45 }, TypeError)
  assertEquals("p_nonwritable", key)
  assertFalse(Object.prototype.hasOwnProperty.call(o, "p_nonwritable"))

  val = ""
  assertEquals(46, o.p_setter = 46)
  assertEquals("p_setter", key)
  assertSame(o, rec)
  assertEquals(46, val)  // written to parent
  assertFalse(Object.prototype.hasOwnProperty.call(o, "p_setter"))

  val = ""
  assertEquals(47, o.p_nosetter = 47)
  assertEquals("p_nosetter", key)
  assertEquals("", val)  // not written at all
  assertFalse(Object.prototype.hasOwnProperty.call(o, "p_nosetter"));

  key = ""
  assertThrows(function(){ "use strict"; o.p_nosetter = 50 }, TypeError)
  assertEquals("p_nosetter", key)
  assertEquals("", val)  // not written at all
  assertFalse(Object.prototype.hasOwnProperty.call(o, "p_nosetter"));

  assertThrows(function(){ o.p_nonconf = 53 }, TypeError)
  assertEquals("p_nonconf", key)
  assertFalse(Object.prototype.hasOwnProperty.call(o, "p_nonconf"));

  assertThrows(function(){ o.p_throw = 51 }, "myexn")
  assertEquals("p_throw", key)
  assertFalse(Object.prototype.hasOwnProperty.call(o, "p_throw"));

  assertThrows(function(){ o.p_setterthrow = 52 }, "myexn")
  assertEquals("p_setterthrow", key)
  assertFalse(Object.prototype.hasOwnProperty.call(o, "p_setterthrow"));
}


TestSetForDerived(
  function(k) {
    // TODO(yangguo): issue 2398 - throwing an error causes formatting of
    // the message string, which can be observable through this handler.
    // We ignore keys that occur when formatting the message string.
    if (k == "toString" || k == "valueOf") return;

    key = k;
    switch (k) {
      case "p_writable": return {writable: true, configurable: true}
      case "p_nonwritable": return {writable: false, configurable: true}
      case "p_setter": return {
        set: function(x) { rec = this; val = x },
        configurable: true
      }
      case "p_nosetter": return {
        get: function() { return 1 },
        configurable: true
      }
      case "p_nonconf": return {}
      case "p_throw": throw "myexn"
      case "p_setterthrow": return {set: function(x) { throw "myexn" }}
      default: return undefined
    }
  }
)


// Evil proxy-induced side-effects shouldn't crash.
// TODO(rossberg): proper behaviour isn't really spec'ed yet, so ignore results.

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



// TODO(rossberg): TestSetReject, returning false
// TODO(rossberg): TestGetProperty, TestSetProperty



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

  var d = create({
    get: function(r, k) { return (k === "value") ? 77 : void 0 },
    getOwnPropertyNames: function() { return ["value"] },
    enumerate: function() { return ["value"] }
  })
  assertEquals(1, Object.getOwnPropertyNames(d).length)
  assertEquals(77, d.value)
  assertEquals(p, Object.defineProperty(p, "p", d))
  assertEquals("p", key)
  assertEquals(1, Object.getOwnPropertyNames(desc).length)
  assertEquals(77, desc.value)

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

function TestDefineThrow2(create, handler) {
  var p = create(handler)
  assertThrows(function(){ Object.defineProperty(p, "a", {value: 44})}, "myexn")
  assertThrows(function(){ Object.defineProperty(p, 0, {value: 44})}, "myexn")

  var d1 = create({
    get: function(r, k) { throw "myexn" },
    getOwnPropertyNames: function() { return ["value"] }
  })
  assertThrows(function(){ Object.defineProperty(p, "p", d1) }, "myexn")
  var d2 = create({
    get: function(r, k) { return 77 },
    getOwnPropertyNames: function() { throw "myexn" }
  })
  assertThrows(function(){ Object.defineProperty(p, "p", d2) }, "myexn")

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

function TestDeleteThrow2(create, handler) {
  var p = create(handler)
  assertThrows(function(){ delete p.a }, "myexn")
  assertThrows(function(){ delete p["b"] }, "myexn");
  assertThrows(function(){ delete p[3] }, "myexn");

  (function() {
    "use strict"
    assertThrows(function(){ delete p.c }, "myexn")
    assertThrows(function(){ delete p["d"] }, "myexn")
    assertThrows(function(){ delete p[4] }, "myexn");
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

function TestDescriptorThrow2(create, handler) {
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
  has: undefined,
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

function TestInThrow2(create, handler) {
  var p = create(handler)
  assertThrows(function(){ return "a" in o }, "myexn")
  assertThrows(function(){ return 99 in o }, "myexn")
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
  has: undefined,
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


function TestInForDerived(handler) {
  TestWithProxies(TestInForDerived2, handler)
}

function TestInForDerived2(create, handler) {
  var p = create(handler)
  var o = Object.create(p)

  assertTrue("a" in o)
  assertEquals("a", key)
  assertTrue(99 in o)
  assertEquals("99", key)
  assertFalse("z" in o)
  assertEquals("z", key)

  assertEquals(2, ("a" in o) ? 2 : 0)
  assertEquals(0, !("a" in o) ? 2 : 0)
  assertEquals(0, ("zzz" in o) ? 2 : 0)
  assertEquals(2, !("zzz" in o) ? 2 : 0)

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

TestInForDerived({
  getPropertyDescriptor: function(k) {
    key = k; return k < "z" ? {value: 42, configurable: true} : void 0
  }
})

TestInForDerived({
  getPropertyDescriptor: function(k) { return this.getPropertyDescriptor2(k) },
  getPropertyDescriptor2: function(k) {
    key = k; return k < "z" ? {value: 42, configurable: true} : void 0
  }
})

TestInForDerived({
  getPropertyDescriptor: function(k) {
    key = k;
    return k < "z" ? {get value() { return 42 }, configurable: true} : void 0
  }
})

/* TODO(rossberg): this will work once we implement the newest proposal
 * regarding default traps for getPropertyDescriptor.
TestInForDerived({
  getOwnPropertyDescriptor: function(k) {
    key = k; return k < "z" ? {value: 42, configurable: true} : void 0
  }
})

TestInForDerived({
  getOwnPropertyDescriptor: function(k) {
    return this.getOwnPropertyDescriptor2(k)
  },
  getOwnPropertyDescriptor2: function(k) {
    key = k; return k < "z" ? {value: 42, configurable: true} : void 0
  }
})

TestInForDerived({
  getOwnPropertyDescriptor: function(k) {
    key = k;
    return k < "z" ? {get value() { return 42 }, configurable: true} : void 0
  }
})
*/

TestInForDerived(Proxy.create({
  get: function(pr, pk) {
    return function(k) {
      key = k; return k < "z" ? {value: 42, configurable: true} : void 0
    }
  }
}))



// Property descriptor conversion.

var descget

function TestDescriptorGetOrder(handler) {
  var p = Proxy.create(handler)
  var o = Object.create(p, {b: {value: 0}})
  TestDescriptorGetOrder2(function(n) { return p[n] }, "vV")
  TestDescriptorGetOrder2(function(n) { return n in p }, "")
  TestDescriptorGetOrder2(function(n) { return o[n] }, "vV")
  TestDescriptorGetOrder2(function(n) { return n in o }, "eEcCvVwWgs")
}

function TestDescriptorGetOrder2(f, access) {
  descget = ""
  assertTrue(f("a"))
  assertEquals(access, descget)
  descget = ""
  assertTrue(f(99))
  assertEquals(access, descget)
  descget = ""
  assertFalse(!!f("z"))
  assertEquals("", descget)
}

TestDescriptorGetOrder({
  getPropertyDescriptor: function(k) {
    if (k >= "z") return void 0
    // Return a proxy as property descriptor, so that we can log accesses.
    return Proxy.create({
      get: function(r, attr) {
        descget += attr[0].toUpperCase()
        return true
      },
      has: function(attr) {
        descget += attr[0]
        switch (attr) {
          case "writable":
          case "enumerable":
          case "configurable":
          case "value":
            return true
          case "get":
          case "set":
            return false
          default:
            assertUnreachable()
        }
      }
    })
  }
})



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

function TestHasOwnThrow2(create, handler) {
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

function TestProxyInstanceof() {
  var o1 = {}
  var p1 = Proxy.create({})
  var p2 = Proxy.create({}, o1)
  var p3 = Proxy.create({}, p2)
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
  assertFalse(p1 instanceof Object)
  assertFalse(p1 instanceof f0)
  assertFalse(p1 instanceof f1)
  assertFalse(p1 instanceof f2)
  assertFalse(p1 instanceof f3)
  assertTrue(p2 instanceof Object)
  assertTrue(p2 instanceof f0)
  assertFalse(p2 instanceof f1)
  assertFalse(p2 instanceof f2)
  assertFalse(p2 instanceof f3)
  assertTrue(p3 instanceof Object)
  assertTrue(p3 instanceof f0)
  assertFalse(p3 instanceof f1)
  assertTrue(p3 instanceof f2)
  assertFalse(p3 instanceof f3)
  assertTrue(o2 instanceof Object)
  assertTrue(o2 instanceof f0)
  assertFalse(o2 instanceof f1)
  assertTrue(o2 instanceof f2)
  assertFalse(o2 instanceof f3)

  var f = Proxy.createFunction({}, function() {})
  assertTrue(f instanceof Function)
}

TestProxyInstanceof()


function TestInstanceofProxy() {
  var o0 = Object.create(null)
  var o1 = {}
  var o2 = Object.create(o0)
  var o3 = Object.create(o1)
  var o4 = Object.create(o2)
  var o5 = Object.create(o3)

  function handler(o) { return {get: function() { return o } } }
  var f0 = Proxy.createFunction(handler(o0), function() {})
  var f1 = Proxy.createFunction(handler(o1), function() {})
  var f2 = Proxy.createFunction(handler(o2), function() {})
  var f3 = Proxy.createFunction(handler(o3), function() {})
  var f4 = Proxy.createFunction(handler(o4), function() {})
  var f5 = Proxy.createFunction(handler(o4), function() {})

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

  var f = Proxy.createFunction({}, function() {})
  var ff = Proxy.createFunction(handler(Function), function() {})
  assertTrue(f instanceof Function)
  assertFalse(f instanceof ff)
}

TestInstanceofProxy()



// Prototype (Object.getPrototypeOf, Object.prototype.isPrototypeOf).

function TestPrototype() {
  var o1 = {}
  var p1 = Proxy.create({})
  var p2 = Proxy.create({}, o1)
  var p3 = Proxy.create({}, p2)
  var p4 = Proxy.create({}, null)
  var o2 = Object.create(p3)

  assertSame(Object.getPrototypeOf(o1), Object.prototype)
  assertSame(Object.getPrototypeOf(p1), null)
  assertSame(Object.getPrototypeOf(p2), o1)
  assertSame(Object.getPrototypeOf(p3), p2)
  assertSame(Object.getPrototypeOf(p4), null)
  assertSame(Object.getPrototypeOf(o2), p3)

  assertTrue(Object.prototype.isPrototypeOf(o1))
  assertFalse(Object.prototype.isPrototypeOf(p1))
  assertTrue(Object.prototype.isPrototypeOf(p2))
  assertTrue(Object.prototype.isPrototypeOf(p3))
  assertFalse(Object.prototype.isPrototypeOf(p4))
  assertTrue(Object.prototype.isPrototypeOf(o2))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, o1))
  assertFalse(Object.prototype.isPrototypeOf.call(Object.prototype, p1))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, p2))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(Object.prototype, p4))
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, o2))
  assertFalse(Object.prototype.isPrototypeOf.call(o1, o1))
  assertFalse(Object.prototype.isPrototypeOf.call(o1, p1))
  assertTrue(Object.prototype.isPrototypeOf.call(o1, p2))
  assertTrue(Object.prototype.isPrototypeOf.call(o1, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(o1, p4))
  assertTrue(Object.prototype.isPrototypeOf.call(o1, o2))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, p1))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, o1))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, p2))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, p4))
  assertFalse(Object.prototype.isPrototypeOf.call(p1, o2))
  assertFalse(Object.prototype.isPrototypeOf.call(p2, p1))
  assertFalse(Object.prototype.isPrototypeOf.call(p2, p2))
  assertTrue(Object.prototype.isPrototypeOf.call(p2, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(p2, p4))
  assertTrue(Object.prototype.isPrototypeOf.call(p2, o2))
  assertFalse(Object.prototype.isPrototypeOf.call(p3, p2))
  assertTrue(Object.prototype.isPrototypeOf.call(p3, o2))
  assertFalse(Object.prototype.isPrototypeOf.call(o2, o1))
  assertFalse(Object.prototype.isPrototypeOf.call(o2, p1))
  assertFalse(Object.prototype.isPrototypeOf.call(o2, p2))
  assertFalse(Object.prototype.isPrototypeOf.call(o2, p3))
  assertFalse(Object.prototype.isPrototypeOf.call(o2, p4))
  assertFalse(Object.prototype.isPrototypeOf.call(o2, o2))

  var f = Proxy.createFunction({}, function() {})
  assertSame(Object.getPrototypeOf(f), Function.prototype)
  assertTrue(Object.prototype.isPrototypeOf(f))
  assertTrue(Object.prototype.isPrototypeOf.call(Function.prototype, f))
}

TestPrototype()



// Property names (Object.getOwnPropertyNames, Object.keys).

function TestPropertyNames(names, handler) {
  TestWithProxies(TestPropertyNames2, handler, names)
}

function TestPropertyNames2(create, handler, names) {
  var p = create(handler)
  assertArrayEquals(names, Object.getOwnPropertyNames(p))
}

TestPropertyNames([], {
  getOwnPropertyNames: function() { return [] }
})

TestPropertyNames(["a", "zz", " ", "0", "toString"], {
  getOwnPropertyNames: function() { return ["a", "zz", " ", 0, "toString"] }
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

function TestPropertyNamesThrow2(create, handler) {
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
  TestWithProxies(TestKeys2, handler, names)
}

function TestKeys2(create, handler, names) {
  var p = create(handler)
  assertArrayEquals(names, Object.keys(p))
}

TestKeys([], {
  keys: function() { return [] }
})

TestKeys(["a", "zz", " ", "0", "toString"], {
  keys: function() { return ["a", "zz", " ", 0, "toString"] }
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
  getOwnPropertyDescriptor: function(k) {
    return k == "" ? undefined : {enumerable: k.length == 1}
  }
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
    return function() { return ["0", 4, "a", "b", "c", 5, "ety"] }
  },
  get getOwnPropertyDescriptor() {
    return function(k) {
      return k == "ety" ? undefined : {enumerable: k >= "44"}
    }
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

function TestKeysThrow2(create, handler) {
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

TestKeysThrow({
  get getOwnPropertyNames() {
    return function() { return [1, 2] }
  },
  getOwnPropertyDescriptor: function(k) { throw "myexn" }
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

  var p = Proxy.create(handler, proto)
  var o = Object.create(p)
  assertFixing(p, false, false, true)
  assertFixing(o, false, false, true)
  Object.freeze(o)
  assertFixing(p, false, false, true)
  assertFixing(o, true, true, false)
}

TestFix([], {
  fix: function() { return {} }
})

TestFix(["a", "b", "c", "3", "zz"], {
  fix: function() {
    return {
      a: {value: "a", writable: true, configurable: false, enumerable: true},
      b: {value: 33, writable: false, configurable: false, enumerable: true},
      c: {value: 0, writable: true, configurable: true, enumerable: true},
      '3': {value: true, writable: false, configurable: true, enumerable: true},
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

function TestFixThrow2(create, handler) {
  var p = create(handler, {})
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


// Freeze a proxy in the middle of operations on it.
// TODO(rossberg): actual behaviour not specified consistently at the moment,
// just make sure that we do not crash.
function TestReentrantFix(f) {
  TestWithProxies(f, Object.freeze)
  TestWithProxies(f, Object.seal)
  TestWithProxies(f, Object.preventExtensions)
}

TestReentrantFix(function(create, freeze) {
  var handler = {
    get get() { freeze(p); return undefined },
    fix: function() { return {} }
  }
  var p = create(handler)
  // Freeze while getting get trap.
  try { p.x } catch (e) { assertInstanceof(e, Error) }
})

TestReentrantFix(function(create, freeze) {
  var handler = {
    get: function() { freeze(p); return 3 },
    fix: function() { return {} }
  }
  var p = create(handler)
  // Freeze while executing get trap.
  try { p.x } catch (e) { assertInstanceof(e, Error) }
})

TestReentrantFix(function(create, freeze) {
  var handler = {
    getPropertyDescriptor: function() { freeze(p); return undefined },
    fix: function() { return {} }
  }
  var p = create(handler)
  // Freeze while executing default get trap.
  try { p.x } catch (e) { assertInstanceof(e, Error) }
})

TestReentrantFix(function(create, freeze) {
  var handler = {
    getPropertyDescriptor: function() { freeze(p); return {get: function(){}} },
    fix: function() { return {} }
  }
  var p = create(handler)
  var o = Object.create(p)
  // Freeze while getting a property from prototype.
  try { o.x } catch (e) { assertInstanceof(e, Error) }
})

TestReentrantFix(function(create, freeze) {
  var handler = {
    get set() { freeze(p); return undefined },
    fix: function() { return {} }
  }
  var p = create(handler)
  // Freeze while getting set trap.
  try { p.x = 4 } catch (e) { assertInstanceof(e, Error) }
})

TestReentrantFix(function(create, freeze) {
  var handler = {
    set: function() { freeze(p); return true },
    fix: function() { return {} }
  }
  var p = create(handler)
  // Freeze while executing set trap.
  try { p.x = 4 } catch (e) { assertInstanceof(e, Error) }
})

TestReentrantFix(function(create, freeze) {
  var handler = {
    getOwnPropertyDescriptor: function() { freeze(p); return undefined },
    fix: function() { return {} }
  }
  var p = create(handler)
  // Freeze while executing default set trap.
  try { p.x } catch (e) { assertInstanceof(e, Error) }
})

TestReentrantFix(function(create, freeze) {
  var handler = {
    getPropertyDescriptor: function() { freeze(p); return {set: function(){}} },
    fix: function() { return {} }
  }
  var p = create(handler)
  var o = Object.create(p)
  // Freeze while setting a property in prototype, dropping the property!
  try { o.x = 4 } catch (e) { assertInstanceof(e, Error) }
})

TestReentrantFix(function(create, freeze) {
  var handler = {
    getPropertyDescriptor: function() { freeze(p); return {set: function(){}} },
    fix: function() { return {x: {get: function(){}}} }
  }
  var p = create(handler)
  var o = Object.create(p)
  // Freeze while setting a property in prototype, making it read-only!
  try { o.x = 4 } catch (e) { assertInstanceof(e, Error) }
})

TestReentrantFix(function(create, freeze) {
  var handler = {
    get fix() { freeze(p); return function(){} }
  }
  var p = create(handler)
  // Freeze while getting fix trap.
  try { Object.freeze(p) } catch (e) { assertInstanceof(e, Error) }
  p = create(handler)
  try { Object.seal(p) } catch (e) { assertInstanceof(e, Error) }
  p = create(handler)
  try { Object.preventExtensions(p) } catch (e) { assertInstanceof(e, Error) }
})

TestReentrantFix(function(create, freeze) {
  var handler = {
    fix: function() { freeze(p); return {} }
  }
  var p = create(handler)
  // Freeze while executing fix trap.
  try { Object.freeze(p) } catch (e) { assertInstanceof(e, Error) }
  p = create(handler)
  try { Object.seal(p) } catch (e) { assertInstanceof(e, Error) }
  p = create(handler)
  try { Object.preventExtensions(p) } catch (e) { assertInstanceof(e, Error) }
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

  var o = Object.create(p)
  key = ""
  assertEquals("[object Object]", Object.prototype.toString.call(o))
  assertEquals("", key)
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

  var o = Object.create(p)
  assertEquals("[object Object]", Object.prototype.toString.call(o))
  assertThrows(function(){ Object.prototype.toLocaleString.call(o) }, "myexn")
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

function TestValueOf2(create, handler) {
  var p = create(handler)
  assertSame(p, Object.prototype.valueOf.call(p))
}

TestValueOf({})



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

function TestIsEnumerableThrow2(create, handler) {
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



// Constructor functions with proxy prototypes.

function TestConstructorWithProxyPrototype() {
  TestWithProxies(TestConstructorWithProxyPrototype2, {})
}

function TestConstructorWithProxyPrototype2(create, handler) {
  function C() {};
  C.prototype = create(handler);

  var o = new C;
  assertSame(C.prototype, Object.getPrototypeOf(o));
}

TestConstructorWithProxyPrototype();
