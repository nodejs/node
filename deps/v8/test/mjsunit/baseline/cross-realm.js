// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sparkplug --no-always-sparkplug

// Tier-up across Realms

// Ensure a feedback vector is created when sharing baseline code.
(function() {
  function factory1() {
    return function(a) {
      return a;
    }
  }

  var realm1 = Realm.createAllowCrossRealmAccess();
  var realm2 = Realm.createAllowCrossRealmAccess();

  // f1 and f2 have the same code, so share a SharedFunctionInfo (i.e. share
  // bytecode and baseline code).
  let f1 = Realm.eval(realm1, "(" + factory1.toString() + ")")();
  let f2 = Realm.eval(realm2, "(" + factory1.toString() + ")")();
  %NeverOptimizeFunction(f1);
  %NeverOptimizeFunction(f2);

  // Compile f1 to baseline, f2 stays uncompiled
  %CompileBaseline(f1);
  assertEquals(0, f1(0));
  assertTrue(isBaseline(f1));
  assertFalse(isBaseline(f2));

  // f2 tiers up to baseline via lazy compile
  assertEquals(0, f2(0));
  assertTrue(isBaseline(f1));
  assertTrue(isBaseline(f2));
})();

// Ensure a feedback vector is created when sharing baseline code and a closure
// feedback cell array already exists.
(function() {
  function factory2() {
    return function(a) {
      return a;
    }
  }

  var realm1 = Realm.createAllowCrossRealmAccess();
  var realm2 = Realm.createAllowCrossRealmAccess();

  // f1, f2 and f3 have the same code, so share a SharedFunctionInfo (i.e. share
  // bytecode and baseline code).
  let f1 = Realm.eval(realm1, "(" + factory2.toString() + ")")();
  let realmFactory = Realm.eval(realm2, "(" + factory2.toString() + ")");
  // f2 and f3 are in the same realm, so share a feedback vector cell.
  let f2 = realmFactory();
  let f3 = realmFactory();
  %NeverOptimizeFunction(f1);
  %NeverOptimizeFunction(f2);
  %NeverOptimizeFunction(f3);

  // Compile f1 to baseline, f2 to interpreter, f3 stays uncompiled.
  assertEquals(0, f2(0));
  %CompileBaseline(f1);
  assertEquals(0, f1(0));
  assertTrue(isBaseline(f1));
  assertFalse(isBaseline(f2));
  assertFalse(isBaseline(f3));

  // Compile f3, tiers up to baseline via lazy compile and installs the feedback
  // vector
  assertEquals(0, f3(0));
  assertTrue(isBaseline(f3));
  assertFalse(isBaseline(f2));

  // Run f2, tiers up to baseline via interpreter entry.
  assertEquals(0, f2(0));
  assertTrue(isBaseline(f2));
})();

// Ensure a feedback vector is created when sharing baseline code and a closure
// feedback cell array already exists.
(function() {
  function factory3() {
    return function(a) {
      return a;
    }
  }

  var realm1 = Realm.createAllowCrossRealmAccess();
  var realm2 = Realm.createAllowCrossRealmAccess();

  // f1, f2 and f3 have the same code, so share a SharedFunctionInfo (i.e. share
  // bytecode and baseline code).
  let f1 = Realm.eval(realm1, "(" + factory3.toString() + ")")();
  let realmFactory = Realm.eval(realm2, "(" + factory3.toString() + ")");
  // f2 and f3 are in the same realm, so share a feedback vector cell.
  let f2 = realmFactory();
  let f3 = realmFactory();
  %NeverOptimizeFunction(f1);
  %NeverOptimizeFunction(f2);
  %NeverOptimizeFunction(f3);

  // Compile f1 to baseline, f2 to interpreter, f3 stays uncompiled.
  assertEquals(0, f2(0));
  %CompileBaseline(f1);
  assertEquals(0, f1(0));
  assertTrue(isBaseline(f1));
  assertFalse(isBaseline(f2));
  assertFalse(isBaseline(f3));

  // Run f2, tiers up to baseline via interpreter entry and installs the
  // feedback vector
  assertEquals(0, f2(0));
  assertTrue(isBaseline(f2));
  assertFalse(isBaseline(f3));

  // Compile f3, tiers up to baseline via lazy compile.
  assertEquals(0, f3(0));
  assertTrue(isBaseline(f3));
})();
