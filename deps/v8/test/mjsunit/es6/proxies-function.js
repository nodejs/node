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

// Flags: --allow-natives-syntax


var handler = {
  get : function(r, n) { return n == "length" ? 2 : undefined }
}


// Calling (call, Function.prototype.call, Function.prototype.apply,
//          Function.prototype.bind).

var global_object = this
var receiver

function TestCall(isStrict, callTrap) {
  assertEquals(42, callTrap(undefined, undefined, [5, 37]))

  var handler = {
    get: function(r, k) {
      return k == "length" ? 2 : Function.prototype[k]
    },
    apply: callTrap
  }
  var f = new Proxy(()=>{}, handler)
  var o = {f: f}
  global_object.f = f

  receiver = 333
  assertEquals(42, f(11, 31))
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
  receiver = 333
  assertEquals(42, f.call(null, 33, 9))
  receiver = 333
  assertEquals(44, f.call(2, 21, 23))
  assertSame(2, receiver.valueOf())
  receiver = 333
  assertEquals(42, Function.prototype.call.call(f, o, 20, 22))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(43, Function.prototype.call.call(f, null, 20, 23))
  assertEquals(44, Function.prototype.call.call(f, 2, 21, 23))
  assertEquals(2, receiver.valueOf())
  receiver = 333
  assertEquals(32, f.apply(o, [16, 16]))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(32, Function.prototype.apply.call(f, o, [17, 15]))
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %Call(f, o, 11, 31));
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %Call(f, null, 11, 31));
  receiver = 333

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
  assertEquals(23, %Call(ff, {}, 11));
  assertSame(o, receiver)
  receiver = 333
  assertEquals(23, %Call(ff, {}, 11, 3));
  assertSame(o, receiver)
  receiver = 333

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
  assertEquals(42, %Call(fff, {}));
  assertSame(o, receiver)
  receiver = 333
  assertEquals(42, %Call(fff, {}, 11, 3))
  assertSame(o, receiver)
  receiver = 333

  var f = new Proxy(()=>{}, {apply: callTrap})
  receiver = 333
  assertEquals(42, f(11, 31))
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
  assertEquals(23, %Call(f, o, 11, 12))
  assertSame(o, receiver)
  receiver = 333
}

TestCall(false, function(_, that, [x, y]) {
  receiver = that
  return x + y
})

TestCall(true, function(_, that, args) {
  "use strict"
  receiver = that
  return args[0] + args[1]
})

TestCall(false, function() {
  receiver = arguments[1]
  return arguments[2][0] + arguments[2][1]
})

TestCall(false, new Proxy(function(_, that, [x, y]) {
    receiver = that
    return x + y
  }, handler))

TestCall(true, new Proxy(function(_, that, args) {
    "use strict"
    receiver = that
    return args[0] + args[1]
  }, handler))

TestCall(false, Object.freeze(new Proxy(function(_, that, [x, y]) {
    receiver = that
    return x + y
  }, handler)))



// Using intrinsics as call traps.

function TestCallIntrinsic(type, callTrap) {
  var f = new Proxy(()=>{}, {apply: (_, that, args) => callTrap(...args)})
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
  var f = new Proxy(()=>{}, {apply: callTrap})
  assertThrowsEquals(() => f(11), "myexn")
  assertThrowsEquals(() => ({x: f}).x(11), "myexn")
  assertThrowsEquals(() => ({x: f})["x"](11), "myexn")
  assertThrowsEquals(() => Function.prototype.call.call(f, {}, 2), "myexn")
  assertThrowsEquals(() => Function.prototype.apply.call(f, {}, [1]), "myexn")
  assertThrowsEquals(() => %Call(f, {}), "myexn")
  assertThrowsEquals(() => %Call(f, {}, 1, 2), "myexn")

  var f = Object.freeze(new Proxy(()=>{}, {apply: callTrap}))
  assertThrowsEquals(() => f(11), "myexn")
  assertThrowsEquals(() => ({x: f}).x(11), "myexn")
  assertThrowsEquals(() => ({x: f})["x"](11), "myexn")
  assertThrowsEquals(() => Function.prototype.call.call(f, {}, 2), "myexn")
  assertThrowsEquals(() => Function.prototype.apply.call(f, {}, [1]), "myexn")
  assertThrowsEquals(() => %Call(f, {}), "myexn")
  assertThrowsEquals(() => %Call(f, {}, 1, 2), "myexn")
}

TestCallThrow(function() { throw "myexn" })
TestCallThrow(new Proxy(() => {throw "myexn"}, {}))
TestCallThrow(Object.freeze(new Proxy(() => {throw "myexn"}, {})))



// Construction (new).

var prototype = {myprop: 0}
var receiver

var handlerWithPrototype = {
  get: function(r, n) {
    if (n == "length") return 2;
    assertEquals("prototype", n);
    return prototype;
  }
}

var handlerSansPrototype = {
  get: function(r, n) {
    if (n == "length") return 2;
    assertEquals("prototype", n);
    return undefined;
  }
}

function ReturnUndef(_, args, newt) {
  "use strict";
  newt.sum = args[0] + args[1];
}

function ReturnThis(x, y) {
  "use strict";
  receiver = this;
  this.sum = x + y;
  return this;
}

function ReturnNew(_, args, newt) {
  "use strict";
  return {sum: args[0] + args[1]};
}

function ReturnNewWithProto(_, args, newt) {
  "use strict";
  var result = Object.create(prototype);
  result.sum = args[0] + args[1];
  return result;
}

function TestConstruct(proto, constructTrap) {
  TestConstruct2(proto, constructTrap, handlerWithPrototype)
  TestConstruct2(proto, constructTrap, handlerSansPrototype)
}

function TestConstruct2(proto, constructTrap, handler) {
  var f = new Proxy(function(){}, {construct: constructTrap})
  var o = new f(11, 31)
  assertEquals(42, o.sum)
  assertSame(proto, Object.getPrototypeOf(o))

  var f = Object.freeze(new Proxy(function(){}, {construct: constructTrap}))
  var o = new f(11, 32)
  assertEquals(43, o.sum)
  assertSame(proto, Object.getPrototypeOf(o))
}

TestConstruct(Object.prototype, ReturnNew)
TestConstruct(prototype, ReturnNewWithProto)

TestConstruct(Object.prototype, new Proxy(ReturnNew, {}))
TestConstruct(prototype, new Proxy(ReturnNewWithProto, {}))

TestConstruct(Object.prototype, Object.freeze(new Proxy(ReturnNew, {})))
TestConstruct(prototype, Object.freeze(new Proxy(ReturnNewWithProto, {})))



// Throwing from the construct trap.

function TestConstructThrow(trap) {
  var f = new Proxy(function(){}, {construct: trap});
  assertThrowsEquals(() => new f(11), "myexn")
  Object.freeze(f)
  assertThrowsEquals(() => new f(11), "myexn")
}

TestConstructThrow(function() { throw "myexn" })
TestConstructThrow(new Proxy(function() { throw "myexn" }, {}))
TestConstructThrow(Object.freeze(new Proxy(function() { throw "myexn" }, {})))



// Using function proxies as getters and setters.

var value
var receiver

function TestAccessorCall(getterCallTrap, setterCallTrap) {
  var pgetter = new Proxy(()=>{}, {apply: getterCallTrap})
  var psetter = new Proxy(()=>{}, {apply: setterCallTrap})

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
  function(_, that) { receiver = that; return 42 },
  function(_, that, [x]) { receiver = that; value = x }
)

TestAccessorCall(
  function(_, that) { "use strict"; receiver = that; return 42 },
  function(_, that, args) { "use strict"; receiver = that; value = args[0] }
)

TestAccessorCall(
  new Proxy(function(_, that) { receiver = that; return 42 }, {}),
  new Proxy(function(_, that, [x]) { receiver = that; value = x }, {})
)

TestAccessorCall(
  Object.freeze(
    new Proxy(function(_, that) { receiver = that; return 42 }, {})),
  Object.freeze(
    new Proxy(function(_, that, [x]) { receiver = that; value = x }, {}))
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
TestHigherOrder(new Proxy(function(x) { return x }, {}))
TestHigherOrder(Object.freeze(new Proxy(function(x) { return x }, {})))



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
    function(f, x, y, o) { return %Call(f, o, x, y) },
    function(f, x, y, o) { return %Apply(f, o, [null, x, y, null], 1, 2) },
    function(f, x, y, o) { return %Apply(f, o, arguments, 2, 2) },
    function(f, x, y, o) { if (typeof o == "object") return o.f(x, y) },
    function(f, x, y, o) { if (typeof o == "object") return o["f"](x, y) },
    function(f, x, y, o) { if (typeof o == "object") return (1, o).f(x, y) },
    function(f, x, y, o) { if (typeof o == "object") return (1, o)["f"](x, y) },
  ]
  var receivers = [o, global_object, undefined, null, 2, "bla", true]
  var expectedSloppies = [o, global_object, global_object, global_object]

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
              var expectedSloppy = expectedSloppies[j > 0 ? m : n]
              o.f = func
              global_object.f = func
              var x = calls[k](func, 11, 31, receiver)
              if (x !== undefined) {
                assertEquals(42, x.result)
                if (calls[k].length < 4)
                  assertSame(x.strict ? undefined : global_object, x.receiver)
                else if (x.strict)
                  assertSame(expected, x.receiver)
                else if (expectedSloppy === undefined)
                  assertSame(expected, x.receiver.valueOf())
                else
                  assertSame(expectedSloppy, x.receiver)
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

Realm.eval(realms[0], "function f(_, that) { return that; };");
Realm.eval(realms[0], "Realm.shared.f = f;");
Realm.eval(realms[0], "Realm.shared.fg = this;");
Realm.eval(realms[1], "function g(_, that) { return that; };");
Realm.eval(realms[1], "Realm.shared.g = g;");
Realm.eval(realms[1], "Realm.shared.gg = this;");

var fp = new Proxy(()=>{}, {apply: Realm.shared.f});
var gp = new Proxy(()=>{}, {apply: Realm.shared.g});

for (var i = 0; i < 10; i++) {
  assertEquals(undefined, fp());
  assertEquals(undefined, gp());

  with (this) {
    assertEquals(this, fp());
    assertEquals(this, gp());
  }

  with ({}) {
    assertEquals(undefined, fp());
    assertEquals(undefined, gp());
  }
}
