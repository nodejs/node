// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle --js-defer-import-eval --allow-natives-syntax --turbofan

// JS_BUNDLE_MODULE:mod1.mjs
export const exported = 1;

// JS_BUNDLE_MODULE:mod2.mjs
globalThis.deferred_evaluated = true;
export const x = 1;

// JS_BUNDLE_MODULE_ENTRYPOINT
import * as ns from './mod1.mjs';
import defer * as dns from './mod2.mjs';

// 1. [[Set]] on an object inheriting from a JSModuleNamespace must always be
// rejected (throwing TypeError in strict mode), even when a map transition for
// the absent property exists on the receiver's map.
const protoChild = Object.create(ns);
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
assertThrows(() => storeFoo(Object.create(ns), 99), TypeError);
assertThrows(() => storeFoo(Object.create(ns), 99), TypeError);
%OptimizeFunctionOnNextCall(storeFoo);

const receiver = Object.create(ns);
assertThrows(() => storeFoo(receiver, 99), TypeError);
assertEquals(undefined, receiver.foo);

// 2. Accessing an absent non-symbol, non-"then" property on an unevaluated
// JSDeferredModuleNamespace in optimized code must still trigger synchronous
// evaluation of the deferred module.
function getNonExistent(o, doRead) {
  if (!doRead) return 0;
  return o.nonExistent;
}

// Make the load in {getNonExistent} megamorphic.
%PrepareFunctionForOptimization(getNonExistent);
for (const obj of [{a: 1}, {b: 2}, {c: 3}, {d: 4}, {e: 5}, {f: 6}]) {
  getNonExistent(obj, true);
}

function caller(o, doRead) {
  let _ = o.then;
  return getNonExistent(o, doRead);
}

%PrepareFunctionForOptimization(caller);
caller(dns, false);
caller(dns, false);
%OptimizeFunctionOnNextCall(caller);
caller(dns, false);

assertEquals(undefined, globalThis.deferred_evaluated);
assertEquals(undefined, caller(dns, true));
assertTrue(Boolean(globalThis.deferred_evaluated));
