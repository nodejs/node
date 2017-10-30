// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var realms = [Realm.current(), Realm.create()];

// Check stack trace filtering across security contexts.
var thrower_script =
    "(function () { Realm.eval(Realm.current(), 'throw Error()') })";
Realm.shared = {
  thrower_0: Realm.eval(realms[0], thrower_script),
  thrower_1: Realm.eval(realms[1], thrower_script),
};

var script = "                                                                 \
  Error.prepareStackTrace = function(a, b) { return b; };                      \
  try {                                                                        \
    Realm.shared.thrower_0();                                                  \
  } catch (e) {                                                                \
    Realm.shared.error_0 = e.stack;                                            \
  }                                                                            \
  try {                                                                        \
    Realm.shared.thrower_1();                                                  \
  } catch (e) {                                                                \
    Realm.shared.error_1 = e.stack;                                            \
  }                                                                            \
";

function assertNotIn(thrower, error) {
  for (var i = 0; i < error.length; i++) {
    assertFalse(false === error[i].getFunction());
  }
}

Realm.eval(realms[1], script);
assertSame(2, Realm.shared.error_0.length);
assertSame(3, Realm.shared.error_1.length);

assertTrue(Realm.shared.thrower_1 === Realm.shared.error_1[1].getFunction());
assertNotIn(Realm.shared.thrower_0, Realm.shared.error_0);
assertNotIn(Realm.shared.thrower_0, Realm.shared.error_1);

Realm.eval(realms[0], script);
assertSame(4, Realm.shared.error_0.length);
assertSame(3, Realm.shared.error_1.length);

assertTrue(Realm.shared.thrower_0 === Realm.shared.error_0[1].getFunction());
assertNotIn(Realm.shared.thrower_1, Realm.shared.error_0);
assertNotIn(Realm.shared.thrower_1, Realm.shared.error_1);


// Check .caller filtering across security contexts.
var caller_script = "(function (f) { f(); })";
Realm.shared = {
  caller_0 : Realm.eval(realms[0], caller_script),
  caller_1 : Realm.eval(realms[1], caller_script),
}

script = "                                                                     \
  function f_0() { Realm.shared.result_0 = arguments.callee.caller; };         \
  function f_1() { Realm.shared.result_1 = arguments.callee.caller; };         \
  Realm.shared.caller_0(f_0);                                                  \
  Realm.shared.caller_1(f_1);                                                  \
";

Realm.eval(realms[1], script);
assertSame(null, Realm.shared.result_0);
assertSame(Realm.shared.caller_1, Realm.shared.result_1);

Realm.eval(realms[0], script);
assertSame(Realm.shared.caller_0, Realm.shared.result_0);
assertSame(null, Realm.shared.result_1);


// test that do not pollute / leak a function prototype v8/4217
var realmIndex = Realm.create();
var otherObject = Realm.eval(realmIndex, "Object");

var f = Realm.eval(realmIndex, "function f(){}; f");
f.prototype = null;

var o = new f();
var proto = Object.getPrototypeOf(o);
assertFalse(proto === Object.prototype);
assertTrue(proto === otherObject.prototype);

o = Realm.eval(realmIndex, "new f()");
proto = Object.getPrototypeOf(o);
assertFalse(proto === Object.prototype);
assertTrue(proto === otherObject.prototype);

// Check function constructor.
var ctor_script = "Function";
var ctor_a_script =
    "(function() { return Function.apply(this, ['return 1;']); })";
var ctor_b_script = "Function.bind(this, 'return 1;')";
var ctor_c_script =
    "(function() { return Function.call(this, 'return 1;'); })";
// Also check Promise constructor.
var promise_ctor_script = "Promise";
Realm.shared = {
  ctor_0 : Realm.eval(realms[0], ctor_script),
  ctor_1 : Realm.eval(realms[1], ctor_script),
  ctor_a_0 : Realm.eval(realms[0], ctor_a_script),
  ctor_a_1 : Realm.eval(realms[1], ctor_a_script),
  ctor_b_0 : Realm.eval(realms[0], ctor_b_script),
  ctor_b_1 : Realm.eval(realms[1], ctor_b_script),
  ctor_c_0 : Realm.eval(realms[0], ctor_c_script),
  ctor_c_1 : Realm.eval(realms[1], ctor_c_script),
  promise_ctor_0 : Realm.eval(realms[0], promise_ctor_script),
  promise_ctor_1 : Realm.eval(realms[1], promise_ctor_script),
}
var script_0 = "                                                               \
  var ctor_0 = Realm.shared.ctor_0;                                            \
  var promise_ctor_0 = Realm.shared.promise_ctor_0;                            \
  Realm.shared.direct_0 = ctor_0('return 1');                                  \
  Realm.shared.indirect_0 = (function() { return ctor_0('return 1;'); })();    \
  Realm.shared.apply_0 = ctor_0.apply(this, ['return 1']);                     \
  Realm.shared.bind_0 = ctor_0.bind(this, 'return 1')();                       \
  Realm.shared.call_0 = ctor_0.call(this, 'return 1');                         \
  Realm.shared.proxy_0 = new Proxy(ctor_0, {})('return 1');                    \
  Realm.shared.reflect_0 = Reflect.apply(ctor_0, this, ['return 1']);          \
  Realm.shared.a_0 = Realm.shared.ctor_a_0();                                  \
  Realm.shared.b_0 = Realm.shared.ctor_b_0();                                  \
  Realm.shared.c_0 = Realm.shared.ctor_c_0();                                  \
  Realm.shared.p_0 = new promise_ctor_0((res,rej) => res(1));                  \
";
script = script_0 + script_0.replace(/_0/g, "_1");
Realm.eval(realms[0], script);
assertSame(1, Realm.shared.direct_0());
assertSame(1, Realm.shared.indirect_0());
assertSame(1, Realm.shared.apply_0());
assertSame(1, Realm.shared.bind_0());
assertSame(1, Realm.shared.call_0());
assertSame(1, Realm.shared.proxy_0());
assertSame(1, Realm.shared.reflect_0());
assertSame(1, Realm.shared.a_0());
assertSame(1, Realm.shared.b_0());
assertSame(1, Realm.shared.c_0());
assertInstanceof(Realm.shared.p_0, Realm.shared.promise_ctor_0);
assertSame(undefined, Realm.shared.direct_1);
assertSame(undefined, Realm.shared.indirect_1);
assertSame(undefined, Realm.shared.apply_1);
assertSame(undefined, Realm.shared.bind_1);
assertSame(undefined, Realm.shared.call_1);
assertSame(undefined, Realm.shared.proxy_1);
assertSame(undefined, Realm.shared.reflect_1);
assertSame(undefined, Realm.shared.a_1);
assertSame(undefined, Realm.shared.b_1);
assertSame(undefined, Realm.shared.c_1);
assertSame(undefined, Realm.shared.p_1);
Realm.eval(realms[1], script);
assertSame(undefined, Realm.shared.direct_0);
assertSame(undefined, Realm.shared.indirect_0);
assertSame(undefined, Realm.shared.apply_0);
assertSame(undefined, Realm.shared.bind_0);
assertSame(undefined, Realm.shared.call_0);
assertSame(undefined, Realm.shared.proxy_0);
assertSame(undefined, Realm.shared.reflect_0);
assertSame(undefined, Realm.shared.a_0);
assertSame(undefined, Realm.shared.b_0);
assertSame(undefined, Realm.shared.c_0);
assertSame(undefined, Realm.shared.p_0);
assertSame(1, Realm.shared.direct_1());
assertSame(1, Realm.shared.indirect_1());
assertSame(1, Realm.shared.apply_1());
assertSame(1, Realm.shared.bind_1());
assertSame(1, Realm.shared.call_1());
assertSame(1, Realm.shared.proxy_1());
assertSame(1, Realm.shared.reflect_1());
assertSame(1, Realm.shared.a_1());
assertSame(1, Realm.shared.b_1());
assertSame(1, Realm.shared.c_1());
assertInstanceof(Realm.shared.p_1, Realm.shared.promise_ctor_1);
