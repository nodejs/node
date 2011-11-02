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

function CreateFrozen(handler, callTrap, constructTrap) {
  if (handler.fix === undefined) handler.fix = function() { return {} }
  var f = Proxy.createFunction(handler, callTrap, constructTrap)
  Object.freeze(f)
  return f
}


// Calling (call, Function.prototype.call, Function.prototype.apply,
//          Function.prototype.bind).

var global_object = this
var receiver

function TestCall(isStrict, callTrap) {
  assertEquals(42, callTrap(5, 37))
  // TODO(rossberg): unrelated bug: this does not succeed for optimized code:
  // assertEquals(isStrict ? undefined : global_object, receiver)

  var f = Proxy.createFunction({}, callTrap)
  receiver = 333
  assertEquals(42, f(11, 31))
  assertEquals(isStrict ? undefined : global_object, receiver)
  var o = {}
  assertEquals(42, Function.prototype.call.call(f, o, 20, 22))
  assertEquals(o, receiver)
  assertEquals(43, Function.prototype.call.call(f, null, 20, 23))
  assertEquals(isStrict ? null : global_object, receiver)
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

  var f = CreateFrozen({}, callTrap)
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

TestCall(false, CreateFrozen({}, function(x, y) {
  receiver = this; return x + y
}))


function TestCallThrow(callTrap) {
  var f = Proxy.createFunction({}, callTrap)
  assertThrows(function(){ f(11) }, "myexn")
  assertThrows(function(){ Function.prototype.call.call(f, {}, 2) }, "myexn")
  assertThrows(function(){ Function.prototype.apply.call(f, {}, [1]) }, "myexn")

  var f = CreateFrozen({}, callTrap)
  assertThrows(function(){ f(11) }, "myexn")
  assertThrows(function(){ Function.prototype.call.call(f, {}, 2) }, "myexn")
  assertThrows(function(){ Function.prototype.apply.call(f, {}, [1]) }, "myexn")
}

TestCallThrow(function() { throw "myexn" })
TestCallThrow(Proxy.createFunction({}, function() { throw "myexn" }))
TestCallThrow(CreateFrozen({}, function() { throw "myexn" }))



// Construction (new).

var prototype = {}
var receiver

var handlerWithPrototype = {
  fix: function() { return {prototype: prototype} },
  get: function(r, n) { assertEquals("prototype", n); return prototype }
}

var handlerSansPrototype = {
  fix: function() { return {} },
  get: function(r, n) { assertEquals("prototype", n); return undefined }
}

function ReturnUndef(x, y) { "use strict"; receiver = this; this.sum = x + y }
function ReturnThis(x, y) { "use strict"; receiver = this; this.sum = x + y; return this }
function ReturnNew(x, y) { "use strict"; receiver = this; return {sum: x + y} }
function ReturnNewWithProto(x, y) {
  "use strict";
  receiver = this;
  var result = Object.create(prototype)
  result.sum = x + y
  return result
}

function TestConstruct(proto, constructTrap) {
  TestConstruct2(proto, constructTrap, handlerWithPrototype)
  TestConstruct2(proto, constructTrap, handlerSansPrototype)
}

function TestConstruct2(proto, constructTrap, handler) {
  var f = Proxy.createFunction(handler, function() {}, constructTrap)
  var o = new f(11, 31)
  // TODO(rossberg): doesn't hold, due to unrelated bug.
  // assertEquals(undefined, receiver)
  assertEquals(42, o.sum)
  assertSame(proto, Object.getPrototypeOf(o))

  var f = CreateFrozen(handler, function() {}, constructTrap)
  var o = new f(11, 32)
  // TODO(rossberg): doesn't hold, due to unrelated bug.
  // assertEquals(undefined, receiver)
  assertEquals(43, o.sum)
  assertSame(proto, Object.getPrototypeOf(o))
}

TestConstruct(Object.prototype, ReturnNew)
TestConstruct(prototype, ReturnNewWithProto)

TestConstruct(Object.prototype, Proxy.createFunction({}, ReturnNew))
TestConstruct(prototype, Proxy.createFunction({}, ReturnNewWithProto))

TestConstruct(Object.prototype, CreateFrozen({}, ReturnNew))
TestConstruct(prototype, CreateFrozen({}, ReturnNewWithProto))


function TestConstructFromCall(proto, returnsThis, callTrap) {
  TestConstructFromCall2(proto, returnsThis, callTrap, handlerWithPrototype)
  TestConstructFromCall2(proto, returnsThis, callTrap, handlerSansPrototype)
}

function TestConstructFromCall2(proto, returnsThis, callTrap, handler) {
  var f = Proxy.createFunction(handler, callTrap)
  var o = new f(11, 31)
  if (returnsThis) assertEquals(o, receiver)
  assertEquals(42, o.sum)
  assertSame(proto, Object.getPrototypeOf(o))

  var f = CreateFrozen(handler, callTrap)
  var o = new f(11, 32)
  if (returnsThis) assertEquals(o, receiver)
  assertEquals(43, o.sum)
  assertSame(proto, Object.getPrototypeOf(o))
}

TestConstructFromCall(Object.prototype, true, ReturnUndef)
TestConstructFromCall(Object.prototype, true, ReturnThis)
TestConstructFromCall(Object.prototype, false, ReturnNew)
TestConstructFromCall(prototype, false, ReturnNewWithProto)

TestConstructFromCall(Object.prototype, true, Proxy.createFunction({}, ReturnUndef))
TestConstructFromCall(Object.prototype, true, Proxy.createFunction({}, ReturnThis))
TestConstructFromCall(Object.prototype, false, Proxy.createFunction({}, ReturnNew))
TestConstructFromCall(prototype, false, Proxy.createFunction({}, ReturnNewWithProto))

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

TestConstructFromCall(Object.prototype, true, Proxy.createFunction({}, ReturnUndef))
TestConstructFromCall(Object.prototype, true, Proxy.createFunction({}, ReturnThis))
TestConstructFromCall(Object.prototype, false, Proxy.createFunction({}, ReturnNew))
TestConstructFromCall(prototype, false, Proxy.createFunction({}, ReturnNewWithProto))

TestConstructFromCall(prototype, true, Proxy.createFunction(handlerWithPrototype, ReturnUndef))
TestConstructFromCall(prototype, true, Proxy.createFunction(handlerWithPrototype, ReturnThis))
TestConstructFromCall(Object.prototype, false, Proxy.createFunction(handlerWithPrototype, ReturnNew))
TestConstructFromCall(prototype, false, Proxy.createFunction(handlerWithPrototype, ReturnNewWithProto))

TestConstructFromCall(prototype, true, CreateFrozen(handlerWithPrototype, ReturnUndef))
TestConstructFromCall(prototype, true, CreateFrozen(handlerWithPrototype, ReturnThis))
TestConstructFromCall(Object.prototype, false, CreateFrozen(handlerWithPrototype, ReturnNew))
TestConstructFromCall(prototype, false, CreateFrozen(handlerWithPrototype, ReturnNewWithProto))


function TestConstructThrow(trap) {
  TestConstructThrow2(Proxy.createFunction({fix: function() {return {}}}, trap))
  TestConstructThrow2(Proxy.createFunction({fix: function() {return {}}},
    function() {}, trap))
}

function TestConstructThrow2(f) {
  assertThrows(function(){ new f(11) }, "myexn")
  Object.freeze(f)
  assertThrows(function(){ new f(11) }, "myexn")
}

TestConstructThrow(function() { throw "myexn" })
TestConstructThrow(Proxy.createFunction({}, function() { throw "myexn" }))
TestConstructThrow(CreateFrozen({}, function() { throw "myexn" }))



// Getters and setters.

var value
var receiver

function TestAccessorCall(getterCallTrap, setterCallTrap) {
  var handler = {fix: function() { return {} }}
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
  assertSame(o, receiver)
  receiver = ""
  assertEquals(undefined, oo.c)
  assertEquals("", receiver)
  receiver = ""
  assertEquals(43, oo["a"])
  assertEquals("", receiver)
  receiver = ""
  assertEquals(42, oo[3])
  assertSame(o, receiver)

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
