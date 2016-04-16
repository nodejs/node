// Copyright 2010 the V8 project authors. All rights reserved.
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

// Tests the Function.prototype.bind method.


// Simple tests.
function foo(x, y, z) {
  return [this, arguments.length, x];
}

assertEquals(3, foo.length);

var f = foo.bind(foo);
assertEquals([foo, 3, 1], f(1, 2, 3));
assertEquals(3, f.length);
assertEquals("function () { [native code] }", f.toString());

f = foo.bind(foo, 1);
assertEquals([foo, 3, 1], f(2, 3));
assertEquals(2, f.length);
assertEquals("function () { [native code] }", f.toString());

f = foo.bind(foo, 1, 2);
assertEquals([foo, 3, 1], f(3));
assertEquals(1, f.length);
assertEquals("function () { [native code] }", f.toString());

f = foo.bind(foo, 1, 2, 3);
assertEquals([foo, 3, 1], f());
assertEquals(0, f.length);
assertEquals("function () { [native code] }", f.toString());

// Test that length works correctly even if more than the actual number
// of arguments are given when binding.
f = foo.bind(foo, 1, 2, 3, 4, 5, 6, 7, 8, 9);
assertEquals([foo, 9, 1], f());
assertEquals(0, f.length);
assertEquals("function () { [native code] }", f.toString());

// Use a different bound object.
var obj = {x: 42, y: 43};
// Values that would normally be in "this" when calling f_bound_this.
var x = 42;
var y = 44;

function f_bound_this(z) {
  return z + this.y - this.x;
}

assertEquals(3, f_bound_this(1))
f = f_bound_this.bind(obj);
assertEquals(2, f(1));
assertEquals(1, f.length);

f = f_bound_this.bind(obj, 2);
assertEquals(3, f());
assertEquals(0, f.length);
assertEquals('[object Function]', Object.prototype.toString.call(f));

// Test chained binds.

// When only giving the thisArg, any number of binds should have
// the same effect.
f = foo.bind(foo);
assertEquals([foo, 3, 1], f(1, 2, 3));

var not_foo = {};
f = foo.bind(foo).bind(not_foo).bind(not_foo).bind(not_foo);
assertEquals([foo, 3, 1], f(1, 2, 3));
assertEquals(3, f.length);

// Giving bound parameters should work at any place in the chain.
f = foo.bind(foo, 1).bind(not_foo).bind(not_foo).bind(not_foo);
assertEquals([foo, 3, 1], f(2, 3));
assertEquals(2, f.length);

f = foo.bind(foo).bind(not_foo, 1).bind(not_foo).bind(not_foo);
assertEquals([foo, 3, 1], f(2, 3));
assertEquals(2, f.length);

f = foo.bind(foo).bind(not_foo).bind(not_foo,1 ).bind(not_foo);
assertEquals([foo, 3, 1], f(2, 3));
assertEquals(2, f.length);

f = foo.bind(foo).bind(not_foo).bind(not_foo).bind(not_foo, 1);
assertEquals([foo, 3, 1], f(2, 3));
assertEquals(2, f.length);

// Several parameters can be given, and given in different bind invocations.
f = foo.bind(foo, 1, 2).bind(not_foo).bind(not_foo).bind(not_foo);
assertEquals([foo, 3, 1], f(3));
assertEquals(1, f.length);

f = foo.bind(foo).bind(not_foo, 1, 2).bind(not_foo).bind(not_foo);
assertEquals([foo, 3, 1], f(1));
assertEquals(1, f.length);

f = foo.bind(foo).bind(not_foo, 1, 2).bind(not_foo).bind(not_foo);
assertEquals([foo, 3, 1], f(3));
assertEquals(1, f.length);

f = foo.bind(foo).bind(not_foo).bind(not_foo, 1, 2).bind(not_foo);
assertEquals([foo, 3, 1], f(1));
assertEquals(1, f.length);

f = foo.bind(foo).bind(not_foo).bind(not_foo).bind(not_foo, 1, 2);
assertEquals([foo, 3, 1], f(3));
assertEquals(1, f.length);

f = foo.bind(foo, 1).bind(not_foo, 2).bind(not_foo).bind(not_foo);
assertEquals([foo, 3, 1], f(3));
assertEquals(1, f.length);

f = foo.bind(foo, 1).bind(not_foo).bind(not_foo, 2).bind(not_foo);
assertEquals([foo, 3, 1], f(3));
assertEquals(1, f.length);

f = foo.bind(foo, 1).bind(not_foo).bind(not_foo).bind(not_foo, 2);
assertEquals([foo, 3, 1], f(3));
assertEquals(1, f.length);

f = foo.bind(foo).bind(not_foo, 1).bind(not_foo).bind(not_foo, 2);
assertEquals([foo, 3, 1], f(3));
assertEquals(1, f.length);

// The wrong number of arguments can be given to bound functions too.
f = foo.bind(foo);
assertEquals(3, f.length);
assertEquals([foo, 0, undefined], f());
assertEquals([foo, 1, 1], f(1));
assertEquals([foo, 2, 1], f(1, 2));
assertEquals([foo, 3, 1], f(1, 2, 3));
assertEquals([foo, 4, 1], f(1, 2, 3, 4));

f = foo.bind(foo, 1);
assertEquals(2, f.length);
assertEquals([foo, 1, 1], f());
assertEquals([foo, 2, 1], f(2));
assertEquals([foo, 3, 1], f(2, 3));
assertEquals([foo, 4, 1], f(2, 3, 4));

f = foo.bind(foo, 1, 2);
assertEquals(1, f.length);
assertEquals([foo, 2, 1], f());
assertEquals([foo, 3, 1], f(3));
assertEquals([foo, 4, 1], f(3, 4));

f = foo.bind(foo, 1, 2, 3);
assertEquals(0, f.length);
assertEquals([foo, 3, 1], f());
assertEquals([foo, 4, 1], f(4));

f = foo.bind(foo, 1, 2, 3, 4);
assertEquals(0, f.length);
assertEquals([foo, 4, 1], f());

// Test constructor calls.

function bar(x, y, z) {
  this.x = x;
  this.y = y;
  this.z = z;
}

f = bar.bind(bar);
var obj2 = new f(1,2,3);
assertEquals(1, obj2.x);
assertEquals(2, obj2.y);
assertEquals(3, obj2.z);

f = bar.bind(bar, 1);
obj2 = new f(2,3);
assertEquals(1, obj2.x);
assertEquals(2, obj2.y);
assertEquals(3, obj2.z);

f = bar.bind(bar, 1, 2);
obj2 = new f(3);
assertEquals(1, obj2.x);
assertEquals(2, obj2.y);
assertEquals(3, obj2.z);

f = bar.bind(bar, 1, 2, 3);
obj2 = new f();
assertEquals(1, obj2.x);
assertEquals(2, obj2.y);
assertEquals(3, obj2.z);


// Test bind chains when used as a constructor.
f = bar.bind(bar, 1).bind(bar, 2).bind(bar, 3);
obj2 = new f();
assertEquals(1, obj2.x);
assertEquals(2, obj2.y);
assertEquals(3, obj2.z);

// Test obj2 is instanceof both bar and f.
assertTrue(obj2 instanceof bar);
assertTrue(obj2 instanceof f);

// This-args are not relevant to instanceof.
f = bar.bind(foo.prototype, 1).
    bind(String.prototype, 2).
    bind(Function.prototype, 3);
var obj3 = new f();
assertTrue(obj3 instanceof bar);
assertTrue(obj3 instanceof f);
assertFalse(obj3 instanceof foo);
assertFalse(obj3 instanceof Function);
assertFalse(obj3 instanceof String);

// thisArg is converted to object.
f = foo.bind(undefined);
assertEquals([this, 0, undefined], f());

f = foo.bind(null);
assertEquals([this, 0, undefined], f());

f = foo.bind(42);
assertEquals([Object(42), 0, undefined], f());

f = foo.bind("foo");
assertEquals([Object("foo"), 0, undefined], f());

f = foo.bind(true);
assertEquals([Object(true), 0, undefined], f());

// Strict functions don't convert thisArg.
function soo(x, y, z) {
  "use strict";
  return [this, arguments.length, x];
}

var s = soo.bind(undefined);
assertEquals([undefined, 0, undefined], s());

s = soo.bind(null);
assertEquals([null, 0, undefined], s());

s = soo.bind(42);
assertEquals([42, 0, undefined], s());

s = soo.bind("foo");
assertEquals(["foo", 0, undefined], s());

s = soo.bind(true);
assertEquals([true, 0, undefined], s());

// Test that .arguments and .caller are poisoned according to the ES5 spec.

// Check that property descriptors are correct (unconfigurable, unenumerable,
// and both get and set is the ThrowTypeError function).
//
// Poisoned accessors are no longer own properties --- get them from the
// prototype
var f_proto = Object.getPrototypeOf(f);
var cdesc = Object.getOwnPropertyDescriptor(f_proto, "caller");
var adesc = Object.getOwnPropertyDescriptor(f_proto, "arguments");

assertFalse(cdesc.enumerable);
assertTrue(cdesc.configurable);

assertFalse(adesc.enumerable);
assertTrue(adesc.configurable);

assertSame(cdesc.get, cdesc.set);
assertSame(cdesc.get, adesc.get);
assertSame(cdesc.get, adesc.set);

assertTrue(cdesc.get instanceof Function);
assertEquals(0, cdesc.get.length);
assertThrows(cdesc.get, TypeError);

assertThrows(function() { return f.caller; }, TypeError);
assertThrows(function() { f.caller = 42; }, TypeError);
assertThrows(function() { return f.arguments; }, TypeError);
assertThrows(function() { f.arguments = 42; }, TypeError);

// Shouldn't throw. Accessing the functions caller must throw if
// the caller is strict and the callee isn't. A bound function is built-in,
// but not considered strict.
(function foo() { return foo.caller; }).bind()();


(function TestProtoIsPreserved() {
  function fun() {}

  function proto() {}
  Object.setPrototypeOf(fun, proto);
  var bound = fun.bind({});
  assertEquals(proto, Object.getPrototypeOf(bound));

  var bound2 = fun.bind({});
  assertTrue(%HaveSameMap(new bound, new bound2));

  Object.setPrototypeOf(fun, null);
  bound = Function.prototype.bind.call(fun, {});
  assertEquals(null, Object.getPrototypeOf(bound));
})();
