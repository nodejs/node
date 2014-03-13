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

// Flags: --harmony-proxies --allow-natives-syntax


// Helper.

function CreateFrozen(handler, callTrap, constructTrap) {
  if (handler.fix === undefined) handler.fix = function() { return {} }
  var f = Proxy.createFunction(handler, callTrap, constructTrap)
  Object.freeze(f)
  return f
}


// Ensures that checking the "length" property of a function proxy doesn't
// crash due to lack of a [[Get]] method.
var handler = {
  get : function(r, n) { return n == "length" ? 2 : undefined }
}


// Calling (call, Function.prototype.call, Function.prototype.apply,
//          Function.prototype.bind).

var global_object = this
var receiver

function TestCall(isStrict, callTrap) {
  assertEquals(42, callTrap(5, 37))
  assertSame(isStrict ? undefined : global_object, receiver)

  var handler = {
    get: function(r, k) {
      return k == "length" ? 2 : Function.prototype[k]
    }
  }
  var f = Proxy.createFunction(handler, callTrap)
  var o = {f: f}
  global_object.f = f

  receiver = 333
  assertEquals(42, f(11, 31))
  assertSame(isStrict ? undefined : global_object, receiver)
  receiver = 333
  assertEquals(42, o.f(10, 32))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, o["f"](9, 33))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, (1, o).f(8, 34))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, (1, o)["f"](7, 35))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, f.call(o, 32, 10))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, f.call(undefined, 33, 9))
  assertSame(isStrict ? undefined : global_object, receiver)
  receiver = 333
  assertEquals(42, f.call(null, 33, 9))
  assertSame(isStrict ? null : global_object, receiver)
  receiver = 333
  assertEquals(44, f.call(2, 21, 23))
  assertSame(2, receiver.valueOf())
  receiver = 333
  assertEquals(42, Function.prototype.call.call(f, o, 20, 22))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(43, Function.prototype.call.call(f, null, 20, 23))
  assertSame(isStrict ? null : global_object, receiver)
  assertEquals(44, Function.prototype.call.call(f, 2, 21, 23))
  assertEquals(2, receiver.valueOf())
  receiver = 333
  assertEquals(32, f.apply(o, [16, 16]))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(32, Function.prototype.apply.call(f, o, [17, 15]))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %Call(o, 11, 31, f))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %Call(null, 11, 31, f))
  assertSame(isStrict ? null : global_object, receiver)
  receiver = 333
  assertEquals(42, %Apply(f, o, [11, 31], 0, 2))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %Apply(f, null, [11, 31], 0, 2))
  assertSame(isStrict ? null : global_object, receiver)
  receiver = 333
  assertEquals(42, %_CallFunction(o, 11, 31, f))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %_CallFunction(null, 11, 31, f))
  assertSame(isStrict ? null : global_object, receiver)

  var ff = Function.prototype.bind.call(f, o, 12)
  assertTrue(ff.length <= 1)  // TODO(rossberg): Not spec'ed yet, be lax.
  receiver = 333
  assertEquals(42, ff(30))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(33, Function.prototype.call.call(ff, {}, 21))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(32, Function.prototype.apply.call(ff, {}, [20]))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(23, %Call({}, 11, ff))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(23, %Call({}, 11, 3, ff))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(24, %Apply(ff, {}, [12, 13], 0, 1))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(24, %Apply(ff, {}, [12, 13], 0, 2))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(34, %_CallFunction({}, 22, ff))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(34, %_CallFunction({}, 22, 3, ff))
  assertSame(o, receiver)

  var fff = Function.prototype.bind.call(ff, o, 30)
  assertEquals(0, fff.length)
  receiver = 333
  assertEquals(42, fff())
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, Function.prototype.call.call(fff, {}))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, Function.prototype.apply.call(fff, {}))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %Call({}, fff))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %Call({}, 11, 3, fff))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %Apply(fff, {}, [], 0, 0))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %Apply(fff, {}, [12, 13], 0, 0))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %Apply(fff, {}, [12, 13], 0, 2))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %_CallFunction({}, fff))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %_CallFunction({}, 3, 4, 5, fff))
  assertSame(o, receiver)

  var f = CreateFrozen({}, callTrap)
  receiver = 333
  assertEquals(42, f(11, 31))
  assertSame(isStrict ? undefined : global_object, receiver)
  var o = {f: f}
  receiver = 333
  assertEquals(42, o.f(10, 32))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, o["f"](9, 33))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, (1, o).f(8, 34))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, (1, o)["f"](7, 35))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, Function.prototype.call.call(f, o, 20, 22))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(32, Function.prototype.apply.call(f, o, [17, 15]))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(23, %Call(o, 11, 12, f))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(27, %Apply(f, o, [12, 13, 14], 1, 2))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %_CallFunction(o, 18, 24, f))
  assertSame(o, receiver)
}

TestCall(false, function(x, y) {
  receiver = this
  return x + y
})

TestCall(true, function(x, y) {
  "use strict"
  receiver = this
  return x + y
})

TestCall(false, function() {
  receiver = this
  return arguments[0] + arguments[1]
})

TestCall(false, Proxy.createFunction(handler, function(x, y) {
  receiver = this
  return x + y
}))

TestCall(true, Proxy.createFunction(handler, function(x, y) {
  "use strict"
  receiver = this
  return x + y
}))

TestCall(false, CreateFrozen(handler, function(x, y) {
  receiver = this
  return x + y
}))



// Using intrinsics as call traps.

function TestCallIntrinsic(type, callTrap) {
  var f = Proxy.createFunction({}, callTrap)
  var x = f()
  assertTrue(typeof x == type)
}

TestCallIntrinsic("boolean", Boolean)
TestCallIntrinsic("number", Number)
TestCallIntrinsic("string", String)
TestCallIntrinsic("object", Object)
TestCallIntrinsic("function", Function)



// Throwing from call trap.

function TestCallThrow(callTrap) {
  var f = Proxy.createFunction({}, callTrap)
  assertThrows(function(){ f(11) }, "myexn")
  assertThrows(function(){ ({x: f}).x(11) }, "myexn")
  assertThrows(function(){ ({x: f})["x"](11) }, "myexn")
  assertThrows(function(){ Function.prototype.call.call(f, {}, 2) }, "myexn")
  assertThrows(function(){ Function.prototype.apply.call(f, {}, [1]) }, "myexn")
  assertThrows(function(){ %Call({}, f) }, "myexn")
  assertThrows(function(){ %Call({}, 1, 2, f) }, "myexn")
  assertThrows(function(){ %Apply({}, f, [], 3, 0) }, "myexn")
  assertThrows(function(){ %Apply({}, f, [3, 4], 0, 1) }, "myexn")
  assertThrows(function(){ %_CallFunction({}, f) }, "myexn")
  assertThrows(function(){ %_CallFunction({}, 1, 2, f) }, "myexn")

  var f = CreateFrozen({}, callTrap)
  assertThrows(function(){ f(11) }, "myexn")
  assertThrows(function(){ ({x: f}).x(11) }, "myexn")
  assertThrows(function(){ ({x: f})["x"](11) }, "myexn")
  assertThrows(function(){ Function.prototype.call.call(f, {}, 2) }, "myexn")
  assertThrows(function(){ Function.prototype.apply.call(f, {}, [1]) }, "myexn")
  assertThrows(function(){ %Call({}, f) }, "myexn")
  assertThrows(function(){ %Call({}, 1, 2, f) }, "myexn")
  assertThrows(function(){ %Apply({}, f, [], 3, 0) }, "myexn")
  assertThrows(function(){ %Apply({}, f, [3, 4], 0, 1) }, "myexn")
  assertThrows(function(){ %_CallFunction({}, f) }, "myexn")
  assertThrows(function(){ %_CallFunction({}, 1, 2, f) }, "myexn")
}

TestCallThrow(function() { throw "myexn" })
TestCallThrow(Proxy.createFunction({}, function() { throw "myexn" }))
TestCallThrow(CreateFrozen({}, function() { throw "myexn" }))



// Construction (new).

var prototype = {myprop: 0}
var receiver

var handlerWithPrototype = {
  fix: function() { return { prototype: { value: prototype } }; },
  get: function(r, n) {
    if (n == "length") return 2;
    assertEquals("prototype", n);
    return prototype;
  }
}

var handlerSansPrototype = {
  fix: function() { return { length: { value: 2 } } },
  get: function(r, n) {
    if (n == "length") return 2;
    assertEquals("prototype", n);
    return undefined;
  }
}

function ReturnUndef(x, y) {
  "use strict";
  receiver = this;
  this.sum = x + y;
}

function ReturnThis(x, y) {
  "use strict";
  receiver = this;
  this.sum = x + y;
  return this;
}

function ReturnNew(x, y) {
  "use strict";
  receiver = this;
  return {sum: x + y};
}

function ReturnNewWithProto(x, y) {
  "use strict";
  receiver = this;
  var result = Object.create(prototype);
  result.sum = x + y;
  return result;
}

function TestConstruct(proto, constructTrap) {
  TestConstruct2(proto, constructTrap, handlerWithPrototype)
  TestConstruct2(proto, constructTrap, handlerSansPrototype)
}

function TestConstruct2(proto, constructTrap, handler) {
  var f = Proxy.createFunction(handler, function() {}, constructTrap)
  var o = new f(11, 31)
  assertEquals(undefined, receiver)
  assertEquals(42, o.sum)
  assertSame(proto, Object.getPrototypeOf(o))

  var f = CreateFrozen(handler, function() {}, constructTrap)
  var o = new f(11, 32)
  assertEquals(undefined, receiver)
  assertEquals(43, o.sum)
  assertSame(proto, Object.getPrototypeOf(o))
}

TestConstruct(Object.prototype, ReturnNew)
TestConstruct(prototype, ReturnNewWithProto)

TestConstruct(Object.prototype, Proxy.createFunction(handler, ReturnNew))
TestConstruct(prototype, Proxy.createFunction(handler, ReturnNewWithProto))

TestConstruct(Object.prototype, CreateFrozen(handler, ReturnNew))
TestConstruct(prototype, CreateFrozen(handler, ReturnNewWithProto))



// Construction with derived construct trap.

function TestConstructFromCall(proto, returnsThis, callTrap) {
  TestConstructFromCall2(prototype, returnsThis, callTrap, handlerWithPrototype)
  TestConstructFromCall2(proto, returnsThis, callTrap, handlerSansPrototype)
}

function TestConstructFromCall2(proto, returnsThis, callTrap, handler) {
  // TODO(rossberg): handling of prototype for derived construct trap will be
  // fixed in a separate change. Commenting out checks below for now.
  var f = Proxy.createFunction(handler, callTrap)
  var o = new f(11, 31)
  if (returnsThis) assertEquals(o, receiver)
  assertEquals(42, o.sum)
  // assertSame(proto, Object.getPrototypeOf(o))

  var g = CreateFrozen(handler, callTrap)
  // assertSame(f.prototype, g.prototype)
  var o = new g(11, 32)
  if (returnsThis) assertEquals(o, receiver)
  assertEquals(43, o.sum)
  // assertSame(proto, Object.getPrototypeOf(o))
}

TestConstructFromCall(Object.prototype, true, ReturnUndef)
TestConstructFromCall(Object.prototype, true, ReturnThis)
TestConstructFromCall(Object.prototype, false, ReturnNew)
TestConstructFromCall(prototype, false, ReturnNewWithProto)

TestConstructFromCall(Object.prototype, true,
                      Proxy.createFunction(handler, ReturnUndef))
TestConstructFromCall(Object.prototype, true,
                      Proxy.createFunction(handler, ReturnThis))
TestConstructFromCall(Object.prototype, false,
                      Proxy.createFunction(handler, ReturnNew))
TestConstructFromCall(prototype, false,
                      Proxy.createFunction(handler, ReturnNewWithProto))

TestConstructFromCall(Object.prototype, true, CreateFrozen({}, ReturnUndef))
TestConstructFromCall(Object.prototype, true, CreateFrozen({}, ReturnThis))
TestConstructFromCall(Object.prototype, false, CreateFrozen({}, ReturnNew))
TestConstructFromCall(prototype, false, CreateFrozen({}, ReturnNewWithProto))

ReturnUndef.prototype = prototype
ReturnThis.prototype = prototype
ReturnNew.prototype = prototype
ReturnNewWithProto.prototype = prototype

TestConstructFromCall(prototype, true, ReturnUndef)
TestConstructFromCall(prototype, true, ReturnThis)
TestConstructFromCall(Object.prototype, false, ReturnNew)
TestConstructFromCall(prototype, false, ReturnNewWithProto)

TestConstructFromCall(Object.prototype, true,
                      Proxy.createFunction(handler, ReturnUndef))
TestConstructFromCall(Object.prototype, true,
                      Proxy.createFunction(handler, ReturnThis))
TestConstructFromCall(Object.prototype, false,
                      Proxy.createFunction(handler, ReturnNew))
TestConstructFromCall(prototype, false,
                      Proxy.createFunction(handler, ReturnNewWithProto))

TestConstructFromCall(prototype, true,
                      Proxy.createFunction(handlerWithPrototype, ReturnUndef))
TestConstructFromCall(prototype, true,
                      Proxy.createFunction(handlerWithPrototype, ReturnThis))
TestConstructFromCall(Object.prototype, false,
                      Proxy.createFunction(handlerWithPrototype, ReturnNew))
TestConstructFromCall(prototype, false,
                      Proxy.createFunction(handlerWithPrototype,
                                           ReturnNewWithProto))

TestConstructFromCall(prototype, true,
                      CreateFrozen(handlerWithPrototype, ReturnUndef))
TestConstructFromCall(prototype, true,
                      CreateFrozen(handlerWithPrototype, ReturnThis))
TestConstructFromCall(Object.prototype, false,
                      CreateFrozen(handlerWithPrototype, ReturnNew))
TestConstructFromCall(prototype, false,
                      CreateFrozen(handlerWithPrototype, ReturnNewWithProto))



// Throwing from the construct trap.

function TestConstructThrow(trap) {
  TestConstructThrow2(Proxy.createFunction({ fix: function() {return {};} },
                                           trap))
  TestConstructThrow2(Proxy.createFunction({ fix: function() {return {};} },
                                           function() {},
                                           trap))
}

function TestConstructThrow2(f) {
  assertThrows(function(){ new f(11) }, "myexn")
  Object.freeze(f)
  assertThrows(function(){ new f(11) }, "myexn")
}

TestConstructThrow(function() { throw "myexn" })
TestConstructThrow(Proxy.createFunction({}, function() { throw "myexn" }))
TestConstructThrow(CreateFrozen({}, function() { throw "myexn" }))



// Using function proxies as getters and setters.

var value
var receiver

function TestAccessorCall(getterCallTrap, setterCallTrap) {
  var handler = { fix: function() { return {} } }
  var pgetter = Proxy.createFunction(handler, getterCallTrap)
  var psetter = Proxy.createFunction(handler, setterCallTrap)

  var o = {}
  var oo = Object.create(o)
  Object.defineProperty(o, "a", {get: pgetter, set: psetter})
  Object.defineProperty(o, "b", {get: pgetter})
  Object.defineProperty(o, "c", {set: psetter})
  Object.defineProperty(o, "3", {get: pgetter, set: psetter})
  Object.defineProperty(oo, "a", {value: 43})

  receiver = ""
  assertEquals(42, o.a)
  assertSame(o, receiver)
  receiver = ""
  assertEquals(42, o.b)
  assertSame(o, receiver)
  receiver = ""
  assertEquals(undefined, o.c)
  assertEquals("", receiver)
  receiver = ""
  assertEquals(42, o["a"])
  assertSame(o, receiver)
  receiver = ""
  assertEquals(42, o[3])
  assertSame(o, receiver)

  receiver = ""
  assertEquals(43, oo.a)
  assertEquals("", receiver)
  receiver = ""
  assertEquals(42, oo.b)
  assertSame(oo, receiver)
  receiver = ""
  assertEquals(undefined, oo.c)
  assertEquals("", receiver)
  receiver = ""
  assertEquals(43, oo["a"])
  assertEquals("", receiver)
  receiver = ""
  assertEquals(42, oo[3])
  assertSame(oo, receiver)

  receiver = ""
  assertEquals(50, o.a = 50)
  assertSame(o, receiver)
  assertEquals(50, value)
  receiver = ""
  assertEquals(51, o.b = 51)
  assertEquals("", receiver)
  assertEquals(50, value)  // no setter
  assertThrows(function() { "use strict"; o.b = 51 }, TypeError)
  receiver = ""
  assertEquals(52, o.c = 52)
  assertSame(o, receiver)
  assertEquals(52, value)
  receiver = ""
  assertEquals(53, o["a"] = 53)
  assertSame(o, receiver)
  assertEquals(53, value)
  receiver = ""
  assertEquals(54, o[3] = 54)
  assertSame(o, receiver)
  assertEquals(54, value)

  value = 0
  receiver = ""
  assertEquals(60, oo.a = 60)
  assertEquals("", receiver)
  assertEquals(0, value)  // oo has own 'a'
  assertEquals(61, oo.b = 61)
  assertSame("", receiver)
  assertEquals(0, value)  // no setter
  assertThrows(function() { "use strict"; oo.b = 61 }, TypeError)
  receiver = ""
  assertEquals(62, oo.c = 62)
  assertSame(oo, receiver)
  assertEquals(62, value)
  receiver = ""
  assertEquals(63, oo["c"] = 63)
  assertSame(oo, receiver)
  assertEquals(63, value)
  receiver = ""
  assertEquals(64, oo[3] = 64)
  assertSame(oo, receiver)
  assertEquals(64, value)
}

TestAccessorCall(
  function() { receiver = this; return 42 },
  function(x) { receiver = this; value = x }
)

TestAccessorCall(
  function() { "use strict"; receiver = this; return 42 },
  function(x) { "use strict"; receiver = this; value = x }
)

TestAccessorCall(
  Proxy.createFunction({}, function() { receiver = this; return 42 }),
  Proxy.createFunction({}, function(x) { receiver = this; value = x })
)

TestAccessorCall(
  CreateFrozen({}, function() { receiver = this; return 42 }),
  CreateFrozen({}, function(x) { receiver = this; value = x })
)



// Passing a proxy function to higher-order library functions.

function TestHigherOrder(f) {
  assertEquals(6, [6, 2].map(f)[0])
  assertEquals(4, [5, 2].reduce(f, 4))
  assertTrue([1, 2].some(f))
  assertEquals("a.b.c", "a.b.c".replace(".", f))
}

TestHigherOrder(function(x) { return x })
TestHigherOrder(function(x) { "use strict"; return x })
TestHigherOrder(Proxy.createFunction({}, function(x) { return x }))
TestHigherOrder(CreateFrozen({}, function(x) { return x }))



// TODO(rossberg): Ultimately, I want to have the following test function
// run through, but it currently fails on so many cases (some not even
// involving proxies), that I leave that for later...
/*
function TestCalls() {
  var handler = {
    get: function(r, k) {
      return k == "length" ? 2 : Function.prototype[k]
    }
  }
  var bind = Function.prototype.bind
  var o = {}

  var traps = [
    function(x, y) {
      return {receiver: this, result: x + y, strict: false}
    },
    function(x, y) { "use strict";
      return {receiver: this, result: x + y, strict: true}
    },
    function() {
      var x = arguments[0], y = arguments[1]
      return {receiver: this, result: x + y, strict: false}
    },
    Proxy.createFunction(handler, function(x, y) {
      return {receiver: this, result: x + y, strict: false}
    }),
    Proxy.createFunction(handler, function() {
      var x = arguments[0], y = arguments[1]
      return {receiver: this, result: x + y, strict: false}
    }),
    Proxy.createFunction(handler, function(x, y) { "use strict"
      return {receiver: this, result: x + y, strict: true}
    }),
    CreateFrozen(handler, function(x, y) {
      return {receiver: this, result: x + y, strict: false}
    }),
    CreateFrozen(handler, function(x, y) { "use strict"
      return {receiver: this, result: x + y, strict: true}
    }),
  ]
  var creates = [
    function(trap) { return trap },
    function(trap) { return CreateFrozen({}, callTrap) },
    function(trap) { return Proxy.createFunction(handler, callTrap) },
    function(trap) {
      return Proxy.createFunction(handler, CreateFrozen({}, callTrap))
    },
    function(trap) {
      return Proxy.createFunction(handler, Proxy.createFunction(handler, callTrap))
    },
  ]
  var binds = [
    function(f, o, x, y) { return f },
    function(f, o, x, y) { return bind.call(f, o) },
    function(f, o, x, y) { return bind.call(f, o, x) },
    function(f, o, x, y) { return bind.call(f, o, x, y) },
    function(f, o, x, y) { return bind.call(f, o, x, y, 5) },
    function(f, o, x, y) { return bind.call(bind.call(f, o), {}, x, y) },
    function(f, o, x, y) { return bind.call(bind.call(f, o, x), {}, y) },
    function(f, o, x, y) { return bind.call(bind.call(f, o, x, y), {}, 5) },
  ]
  var calls = [
    function(f, x, y) { return f(x, y) },
    function(f, x, y) { var g = f; return g(x, y) },
    function(f, x, y) { with ({}) return f(x, y) },
    function(f, x, y) { var g = f; with ({}) return g(x, y) },
    function(f, x, y, o) { with (o) return f(x, y) },
    function(f, x, y, o) { return f.call(o, x, y) },
    function(f, x, y, o) { return f.apply(o, [x, y]) },
    function(f, x, y, o) { return Function.prototype.call.call(f, o, x, y) },
    function(f, x, y, o) { return Function.prototype.apply.call(f, o, [x, y]) },
    function(f, x, y, o) { return %_CallFunction(o, x, y, f) },
    function(f, x, y, o) { return %Call(o, x, y, f) },
    function(f, x, y, o) { return %Apply(f, o, [null, x, y, null], 1, 2) },
    function(f, x, y, o) { return %Apply(f, o, arguments, 2, 2) },
    function(f, x, y, o) { if (typeof o == "object") return o.f(x, y) },
    function(f, x, y, o) { if (typeof o == "object") return o["f"](x, y) },
    function(f, x, y, o) { if (typeof o == "object") return (1, o).f(x, y) },
    function(f, x, y, o) { if (typeof o == "object") return (1, o)["f"](x, y) },
  ]
  var receivers = [o, global_object, undefined, null, 2, "bla", true]
  var expectedNonStricts = [o, global_object, global_object, global_object]

  for (var t = 0; t < traps.length; ++t) {
    for (var i = 0; i < creates.length; ++i) {
      for (var j = 0; j < binds.length; ++j) {
        for (var k = 0; k < calls.length; ++k) {
          for (var m = 0; m < receivers.length; ++m) {
            for (var n = 0; n < receivers.length; ++n) {
              var bound = receivers[m]
              var receiver = receivers[n]
              var func = binds[j](creates[i](traps[t]), bound, 31, 11)
              var expected = j > 0 ? bound : receiver
              var expectedNonStrict = expectedNonStricts[j > 0 ? m : n]
              o.f = func
              global_object.f = func
              var x = calls[k](func, 11, 31, receiver)
              if (x !== undefined) {
                assertEquals(42, x.result)
                if (calls[k].length < 4)
                  assertSame(x.strict ? undefined : global_object, x.receiver)
                else if (x.strict)
                  assertSame(expected, x.receiver)
                else if (expectedNonStrict === undefined)
                  assertSame(expected, x.receiver.valueOf())
                else
                  assertSame(expectedNonStrict, x.receiver)
              }
            }
          }
        }
      }
    }
  }
}

TestCalls()
*/

var realms = [Realm.create(), Realm.create()];
Realm.shared = {};

Realm.eval(realms[0], "function f() { return this; };");
Realm.eval(realms[0], "Realm.shared.f = f;");
Realm.eval(realms[0], "Realm.shared.fg = this;");
Realm.eval(realms[1], "function g() { return this; };");
Realm.eval(realms[1], "Realm.shared.g = g;");
Realm.eval(realms[1], "Realm.shared.gg = this;");

var fp = Proxy.createFunction({}, Realm.shared.f);
var gp = Proxy.createFunction({}, Realm.shared.g);

for (var i = 0; i < 10; i++) {
  assertEquals(Realm.shared.fg, fp());
  assertEquals(Realm.shared.gg, gp());

  with (this) {
    assertEquals(this, fp());
    assertEquals(this, gp());
  }

  with ({}) {
    assertEquals(Realm.shared.fg, fp());
    assertEquals(Realm.shared.gg, gp());
  }
}
