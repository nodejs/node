// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

(function() {
  const realm = Realm.createAllowCrossRealmAccess();
  const remote_object_prototype = Realm.eval(realm, "Object.prototype");
  Realm.eval(realm, `
    function* gen() {
      yield 1;
      yield 2;
      return 3;
    }
  `);

  const remote_gen = Realm.eval(realm, "gen");

  function* local_gen() {
    yield 10;
    yield 20;
    return 30;
  }

  // Test Maglev with cross-realm generator.
  function testMaglev(g) {
    return g.next();
  }
  %PrepareFunctionForOptimization(testMaglev);

  let g1 = remote_gen();
  let res1_1 = testMaglev(g1);
  assertEquals(1, res1_1.value);
  assertSame(remote_object_prototype, Object.getPrototypeOf(res1_1));

  let res1_2 = testMaglev(g1);
  assertEquals(2, res1_2.value);
  assertSame(remote_object_prototype, Object.getPrototypeOf(res1_2));

  %OptimizeMaglevOnNextCall(testMaglev);

  let res1_3 = testMaglev(g1);
  assertEquals(3, res1_3.value);
  assertSame(remote_object_prototype, Object.getPrototypeOf(res1_3));

  // Test TurboFan with cross-realm generator.
  function testTurboFan(g) {
    return g.next();
  }
  %PrepareFunctionForOptimization(testTurboFan);

  let g2 = remote_gen();
  let res2_1 = testTurboFan(g2);
  assertEquals(1, res2_1.value);
  assertSame(remote_object_prototype, Object.getPrototypeOf(res2_1));

  let res2_2 = testTurboFan(g2);
  assertEquals(2, res2_2.value);
  assertSame(remote_object_prototype, Object.getPrototypeOf(res2_2));

  %OptimizeFunctionOnNextCall(testTurboFan);

  let res2_3 = testTurboFan(g2);
  assertEquals(3, res2_3.value);
  assertSame(remote_object_prototype, Object.getPrototypeOf(res2_3));

  // Test polymorphic case in Maglev.
  function testPolyMaglev(g) {
    return g.next();
  }
  %PrepareFunctionForOptimization(testPolyMaglev);

  let g_local = local_gen();
  let g_remote = remote_gen();

  let res_poly_local_1 = testPolyMaglev(g_local);
  assertEquals(10, res_poly_local_1.value);
  assertSame(Object.prototype, Object.getPrototypeOf(res_poly_local_1));

  let res_poly_remote_1 = testPolyMaglev(g_remote);
  assertEquals(1, res_poly_remote_1.value);
  assertSame(remote_object_prototype, Object.getPrototypeOf(res_poly_remote_1));

  %OptimizeMaglevOnNextCall(testPolyMaglev);

  let res_poly_local_2 = testPolyMaglev(g_local);
  assertEquals(20, res_poly_local_2.value);
  assertSame(Object.prototype, Object.getPrototypeOf(res_poly_local_2));

  let res_poly_remote_2 = testPolyMaglev(g_remote);
  assertEquals(2, res_poly_remote_2.value);
  assertSame(remote_object_prototype, Object.getPrototypeOf(res_poly_remote_2));
})();

(function() {
  const realm = Realm.createAllowCrossRealmAccess();
  const remote_object_prototype = Realm.eval(realm, "Object.prototype");
  Realm.eval(realm, `
    function* gen() {
      yield 1;
      yield 2;
      return 3;
    }
  `);
  const remote_gen = Realm.eval(realm, "gen");
  const remote_next = Object.getPrototypeOf(remote_gen()).next;
  function testMaglev(g) {
    return remote_next.call(g);
  }
  %PrepareFunctionForOptimization(testMaglev);
  let g1 = remote_gen();
  testMaglev(g1);
  %OptimizeMaglevOnNextCall(testMaglev);
  let res1_2 = testMaglev(g1);
  assertSame(remote_object_prototype, Object.getPrototypeOf(res1_2));
  function testTurboFan(g) {
    return remote_next.call(g);
  }
  %PrepareFunctionForOptimization(testTurboFan);
  let g2 = remote_gen();
  testTurboFan(g2);
  %OptimizeFunctionOnNextCall(testTurboFan);
  let res2_2 = testTurboFan(g2);
  assertSame(remote_object_prototype, Object.getPrototypeOf(res2_2));
})();
