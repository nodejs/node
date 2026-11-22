// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

// 1. Cross-origin navigation (Realm.navigate) after Object.create(globalThis).
{
  const r = Realm.create();
  const proxy = Realm.global(r);
  const [preNavChild, preNavGrandchild, preLitVal] = Realm.eval(r, `
    globalThis.marker = 'before';
    function Sub() {}
    Sub.prototype = globalThis;
    Reflect.construct(Object, [], Sub);
    const c = Object.create(globalThis);
    const gc = Object.create(c);
    const lit = { __proto__: globalThis, method() { return marker; } };
    [c, gc, lit.method()];
  `);
  assertEquals('before', preLitVal);

  Realm.navigate(r);

  const [postNavRealm, o1, o2, o3, m1, m3, postLitVal] = Realm.eval(r, `
    globalThis.marker = 'after';
    const lit = { __proto__: globalThis, method() { return marker; } };
    function createFromGlobal() {
      return Object.create(globalThis);
    }
    %PrepareFunctionForOptimization(createFromGlobal);
    const o1 = createFromGlobal();
    const o2 = createFromGlobal();
    %OptimizeFunctionOnNextCall(createFromGlobal);
    const o3 = createFromGlobal();
    [Realm.current(), o1, o2, o3, o1.marker, o3.marker, lit.method()];
  `);
  assertTrue(%HaveSameMap(o1, o2));
  assertTrue(%HaveSameMap(o1, o3));
  assertEquals('after', m1);
  assertEquals('after', m3);
  assertEquals('after', postLitVal);
  assertEquals(postNavRealm, Realm.owner(o1));
  assertEquals(postNavRealm, Realm.owner(o3));

  // Cross-realm Object.create and Reflect.construct on navigated global proxy
  // from main realm.
  const fromMain = Object.create(proxy);
  assertEquals(Realm.current(), Realm.owner(fromMain));
  function MainSub() {}
  MainSub.prototype = proxy;
  const constructedFromMain = Reflect.construct(Object, [], MainSub);
  assertEquals(Realm.current(), Realm.owner(constructedFromMain));

  // Mutating pre-navigation prototype user registered on global proxy must
  // cleanly update prototype user registration after navigation.
  Object.setPrototypeOf(preNavChild, {});
  assertEquals(undefined, preNavGrandchild.marker);
}

// 2. Same-origin navigation (Realm.navigateSameOrigin) after Object.create(globalThis).
{
  const r = Realm.createAllowCrossRealmAccess();
  Realm.eval(r, `
    globalThis.value = 10;
    Object.create(globalThis);
  `);

  Realm.navigateSameOrigin(r);

  const [postNavRealm, o1, o2, val] = Realm.eval(r, `
    globalThis.value = 20;
    const o1 = Object.create(globalThis);
    const o2 = Object.create(globalThis);
    [Realm.current(), o1, o2, o1.value];
  `);
  assertTrue(%HaveSameMap(o1, o2));
  assertEquals(postNavRealm, Realm.owner(o1));
  assertEquals(20, val);
}

// 3. DetachGlobal without immediate navigation (Realm.detachGlobal), including
// deoptimization of pre-detach optimized Object.create(globalThis).
{
  const r = Realm.createAllowCrossRealmAccess();
  const [proxy, preDetachChild, optC, createInOldRealm] = Realm.eval(r, `
    const g = globalThis;
    function create() { return Object.create(g); }
    %PrepareFunctionForOptimization(create);
    create();
    const c = create();
    %OptimizeFunctionOnNextCall(create);
    const optC = create();
    [g, c, optC, create];
  `);
  assertTrue(%HaveSameMap(preDetachChild, optC));

  Realm.detachGlobal(r);

  const detachedChild = createInOldRealm();
  assertEquals(r, Realm.owner(detachedChild));
  assertFalse(%HaveSameMap(preDetachChild, detachedChild));
  const mainChild = Object.create(proxy);
  assertEquals(Realm.current(), Realm.owner(mainChild));
  assertFalse(%HaveSameMap(detachedChild, mainChild));
}
