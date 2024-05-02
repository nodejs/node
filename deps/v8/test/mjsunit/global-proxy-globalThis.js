// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

{
  const realm = Realm.createAllowCrossRealmAccess();
  const foo = Realm.eval(realm, "function foo() { return globalThis.foo }; foo");

  %PrepareFunctionForOptimization(foo);
  assertSame(foo(), foo);
  %OptimizeFunctionOnNextCall(foo);
  assertSame(foo(), foo);
}

// detachGlobal, old map
{
  const realm = Realm.createAllowCrossRealmAccess();
  const foo = Realm.eval(realm, "function foo() { return globalThis.foo }; foo");

  %PrepareFunctionForOptimization(foo);
  assertSame(foo(), foo);
  Realm.detachGlobal(realm);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo);
}

// navigate, old map
{
  const realm = Realm.createAllowCrossRealmAccess();
  const foo = Realm.eval(realm, "function foo() { return globalThis.foo }; foo");

  %PrepareFunctionForOptimization(foo);
  assertSame(foo(), foo);
  Realm.navigate(realm);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo);
}

// detachGlobal, new map
{
  const realm = Realm.createAllowCrossRealmAccess();
  const foo = Realm.eval(realm, "function foo() { return globalThis.foo }; foo");

  assertSame(foo(), foo);
  Realm.detachGlobal(realm);
  %PrepareFunctionForOptimization(foo);
  assertThrows(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo);
}

// navigate, new map
{
  const realm = Realm.createAllowCrossRealmAccess();
  const foo = Realm.eval(realm, "function foo() { return globalThis.foo }; foo");

  assertSame(foo(), foo);
  Realm.navigate(realm);
  %PrepareFunctionForOptimization(foo);
  assertThrows(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo);
}

// detachGlobal, old and new map
{
  const realm = Realm.createAllowCrossRealmAccess();
  const foo = Realm.eval(realm, "function foo() { return globalThis.foo }; foo");

  %PrepareFunctionForOptimization(foo);
  assertSame(foo(), foo);
  Realm.detachGlobal(realm);
  assertThrows(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo);
}

// navigate, old and new map
{
  const realm = Realm.createAllowCrossRealmAccess();
  const foo = Realm.eval(realm, "function foo() { return globalThis.foo }; foo");

  %PrepareFunctionForOptimization(foo);
  assertSame(foo(), foo);
  Realm.navigate(realm);
  assertThrows(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo);
}
