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
assertSame(3, Realm.shared.error_0.length);
assertSame(4, Realm.shared.error_1.length);

assertTrue(Realm.shared.thrower_1 === Realm.shared.error_1[2].getFunction());
assertNotIn(Realm.shared.thrower_0, Realm.shared.error_0);
assertNotIn(Realm.shared.thrower_0, Realm.shared.error_1);

Realm.eval(realms[0], script);
assertSame(5, Realm.shared.error_0.length);
assertSame(4, Realm.shared.error_1.length);

assertTrue(Realm.shared.thrower_0 === Realm.shared.error_0[2].getFunction());
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
