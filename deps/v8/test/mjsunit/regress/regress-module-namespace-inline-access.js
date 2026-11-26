// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle --js-defer-import-eval --allow-natives-syntax --turbofan

// Optimized accesses to names that are absent from a module namespace on the
// receiver or its prototype chain. Each case uses its own modules, since
// evaluating a deferred module (or failing to) is observable by later cases.
// "then" is not an evaluation trigger of a deferred namespace, so it is used
// to collect feedback for the namespace's map without evaluating the module.

// JS_BUNDLE_MODULE:store.mjs
export const exported = 1;

// JS_BUNDLE_MODULE:deferred-store.mjs
export const x = 1;

// JS_BUNDLE_MODULE:deferred-load.mjs
globalThis.deferredLoadEvaluated = true;
export const x = 1;

// JS_BUNDLE_MODULE:throws-receiver.mjs
globalThis.throwsReceiverEvaluated = true;
export const y = 1;
throw new Error('throws');

// JS_BUNDLE_MODULE:throws-proto.mjs
globalThis.throwsProtoEvaluated = true;
export const z = 1;
throw new Error('throws');

// JS_BUNDLE_MODULE:fast-path.mjs
export const exported = 1;

// JS_BUNDLE_MODULE:deferred-fast-path.mjs
export const x = 1;

// JS_BUNDLE_MODULE_ENTRYPOINT
import * as storeNs from './store.mjs';
import defer * as deferredStoreNs from './deferred-store.mjs';
import defer * as deferredLoadNs from './deferred-load.mjs';
import defer * as throwsReceiverNs from './throws-receiver.mjs';
import defer * as throwsProtoNs from './throws-proto.mjs';
import * as fastPathNs from './fast-path.mjs';
import defer * as deferredFastPathNs from './deferred-fast-path.mjs';

const dummyObjs = [{a: 1}, {b: 2}, {c: 3}, {d: 4}, {e: 5}, {f: 6}];

// [[Set]] on an object inheriting from a JSModuleNamespace must always be
// rejected (throwing TypeError in strict mode), even when a map transition for
// the absent property exists on the receiver's map.
{
  const protoChild = Object.create(storeNs);
  Object.defineProperty(protoChild, 'foo', {
    value: 42,
    writable: true,
    enumerable: true,
    configurable: true,
  });

  function storeFoo(o, v) {
    o.foo = v;
  }

  %PrepareFunctionForOptimization(storeFoo);
  for (let i = 0; i < 3; i++) {
    assertThrows(() => storeFoo(Object.create(storeNs), 99), TypeError);
  }
  %OptimizeFunctionOnNextCall(storeFoo);
  const receiver = Object.create(storeNs);
  assertThrows(() => storeFoo(receiver, 99), TypeError);
  assertEquals(undefined, receiver.foo);
  assertFalse(Object.hasOwn(receiver, 'foo'));
}

// Stores through an evaluated deferred namespace on the prototype chain must
// still throw.
{
  assertEquals(1, deferredStoreNs.x);  // Evaluate the deferred module.

  function storeAbsent(o) { o.absent = 1; }
  const child = Object.create(deferredStoreNs);
  %PrepareFunctionForOptimization(storeAbsent);
  assertThrows(() => storeAbsent(child), TypeError);
  %OptimizeFunctionOnNextCall(storeAbsent);
  assertThrows(() => storeAbsent(child), TypeError);
  assertEquals(undefined, child.absent);
  assertFalse(Object.hasOwn(child, 'absent'));
}

// Accessing an absent non-symbol, non-"then" property on an unevaluated
// JSDeferredModuleNamespace in optimized code must still trigger synchronous
// evaluation of the deferred module.
{
  function getNonExistent(o, doRead) {
    if (!doRead) return 0;
    return o.nonExistent;
  }
  function caller(o, doRead) {
    let _ = o.then;
    return getNonExistent(o, doRead);
  }

  %PrepareFunctionForOptimization(getNonExistent);
  %PrepareFunctionForOptimization(caller);
  for (const obj of dummyObjs) getNonExistent(obj, true);
  caller(deferredLoadNs, false);
  %OptimizeFunctionOnNextCall(caller);
  caller(deferredLoadNs, false);
  assertOptimized(caller);
  assertEquals(undefined, globalThis.deferredLoadEvaluated);
  assertEquals(undefined, caller(deferredLoadNs, true));
  assertTrue(globalThis.deferredLoadEvaluated);
}

// When the deferred module throws during evaluation, an optimized load of an
// absent name on the deferred namespace must evaluate the module and rethrow
// the error, not fold to undefined.
{
  function loadNope(o, doRead) {
    if (!doRead) return 0;
    return o.nope;
  }
  function thenAndNope(o, doRead) {
    let _ = o.then;
    return loadNope(o, doRead);
  }

  %PrepareFunctionForOptimization(loadNope);
  %PrepareFunctionForOptimization(thenAndNope);
  for (const obj of dummyObjs) loadNope(obj, true);
  thenAndNope(throwsReceiverNs, false);
  %OptimizeFunctionOnNextCall(thenAndNope);
  thenAndNope(throwsReceiverNs, false);
  assertOptimized(thenAndNope);
  assertEquals(undefined, globalThis.throwsReceiverEvaluated);
  assertThrows(() => thenAndNope(throwsReceiverNs, true), Error, 'throws');
  assertTrue(globalThis.throwsReceiverEvaluated);
  // Later accesses keep rethrowing the evaluation error.
  assertThrows(() => thenAndNope(throwsReceiverNs, true), Error, 'throws');
  assertThrows(() => throwsReceiverNs.y, Error, 'throws');
}

// Same as the previous case, but with the deferred namespace on the prototype
// chain.
{
  function loadNope(o, doRead) {
    if (!doRead) return 0;
    return o.nope;
  }
  function thenAndNope(o, doRead) {
    let _ = o.then;
    return loadNope(o, doRead);
  }

  const child = Object.create(throwsProtoNs);
  %PrepareFunctionForOptimization(loadNope);
  %PrepareFunctionForOptimization(thenAndNope);
  for (const obj of dummyObjs) loadNope(obj, true);
  thenAndNope(child, false);
  %OptimizeFunctionOnNextCall(thenAndNope);
  thenAndNope(child, false);
  assertOptimized(thenAndNope);
  assertEquals(undefined, globalThis.throwsProtoEvaluated);
  assertThrows(() => thenAndNope(child, true), Error, 'throws');
  assertTrue(globalThis.throwsProtoEvaluated);
  assertThrows(() => child.z, Error, 'throws');
}

// Loads and has checks of absent names through a namespace on the prototype
// chain may still be optimized: they yield undefined / false. This holds for a
// regular namespace and for an already evaluated deferred namespace.
{
  assertEquals(1, deferredFastPathNs.x);  // Evaluate the deferred module.

  function loadAbsent(o) { return o.absent; }
  function hasAbsent(o) { return 'absent' in o; }
  %PrepareFunctionForOptimization(loadAbsent);
  %PrepareFunctionForOptimization(hasAbsent);
  const nsChild = Object.create(fastPathNs);
  const dnsChild = Object.create(deferredFastPathNs);
  for (const o of [nsChild, dnsChild]) {
    assertEquals(undefined, loadAbsent(o));
    assertFalse(hasAbsent(o));
  }
  %OptimizeFunctionOnNextCall(loadAbsent);
  %OptimizeFunctionOnNextCall(hasAbsent);
  for (const o of [nsChild, dnsChild]) {
    assertEquals(undefined, loadAbsent(o));
    assertFalse(hasAbsent(o));
  }
  assertOptimized(loadAbsent);
  assertOptimized(hasAbsent);

  // The same holds when the evaluated deferred namespace is accessed directly,
  // i.e. when it is the receiver instead of being on the prototype chain.
  function loadAbsentDirect(o) { return o.absent; }
  function hasAbsentDirect(o) { return 'absent' in o; }
  %PrepareFunctionForOptimization(loadAbsentDirect);
  %PrepareFunctionForOptimization(hasAbsentDirect);
  assertEquals(undefined, loadAbsentDirect(deferredFastPathNs));
  assertFalse(hasAbsentDirect(deferredFastPathNs));
  %OptimizeFunctionOnNextCall(loadAbsentDirect);
  %OptimizeFunctionOnNextCall(hasAbsentDirect);
  assertEquals(undefined, loadAbsentDirect(deferredFastPathNs));
  assertFalse(hasAbsentDirect(deferredFastPathNs));
  assertOptimized(loadAbsentDirect);
  assertOptimized(hasAbsentDirect);
}
