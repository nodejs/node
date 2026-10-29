// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function check(x) { assertEquals(x, "foo"); }

var r = Realm.createAllowCrossRealmAccess();
var f = Realm.eval(r, `
    function f(func) {
      // The call to Function.prototype.apply is across native contexts so
      // cannot be elided. However the compiler should be able to call the
      // builtin directly rather than via the trampoline Code object. This isn't
      // easy to test, but here we at least check that it doesn't crash due to
      // calling a builtin Code object incorrectly (Function.Prototype.apply).
      return func.apply(undefined, ["foo"]);
    }

    f;`);

%PrepareFunctionForOptimization(f);

f(check);
f(check);

%OptimizeFunctionOnNextCall(f);

f(check);
