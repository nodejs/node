// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval --bundle

// Regression test for crbug.com/564879562: on a deferred module namespace,
// "then" must be treated as an ordinary (absent) property per
// IsSymbolLikeNamespaceKey. Accessing it must not evaluate the module, read
// the export Cell, or observe a TDZ ReferenceError, and dynamic import.defer()
// must resolve to the namespace without invoking an exported `then`.

// JS_BUNDLE_MODULE:mod_then_fn.mjs
globalThis.fn_module_evaluated = true;
export let tdz_binding = 'initialized';
export function then(resolve) {
  globalThis.then_ran_while_unevaluated = !globalThis.fn_module_evaluated;
  resolve('hijacked-thenable');
}

// JS_BUNDLE_MODULE:mod_then_tdz.mjs
globalThis.tdz_module_evaluated = true;
export const then = 42;

// JS_BUNDLE_MODULE_ENTRYPOINT
import defer * as ns_fn from './mod_then_fn.mjs';
import defer * as ns_tdz from './mod_then_tdz.mjs';

function readThenIC(o) {
  return o.then;
}

// 1. Per IsSymbolLikeNamespaceKey("then", ns), [[Get]], [[GetOwnProperty]],
// [[HasProperty]], and [[DefineOwnProperty]] on a deferred namespace must
// treat "then" as an ordinary property (not an export lookup) and must NOT
// evaluate the module or read the export Cell.
for (let i = 0; i < 10; i++) {
  assertEquals(undefined, readThenIC(ns_fn));
  assertEquals(undefined, readThenIC(ns_tdz));
}
assertEquals(undefined, Object.getOwnPropertyDescriptor(ns_fn, 'then'));
assertEquals(undefined, Object.getOwnPropertyDescriptor(ns_tdz, 'then'));
assertFalse('then' in ns_fn);
assertFalse('then' in ns_tdz);
assertThrows(() => {
  Object.defineProperty(ns_tdz, 'then', { value: 1 });
}, TypeError);

assertEquals(undefined, globalThis.fn_module_evaluated);
assertEquals(undefined, globalThis.tdz_module_evaluated);

// 2. Dynamic import.defer() must resolve to the deferred namespace without
// invoking the exported `then` function or rejecting with a TDZ ReferenceError.
const imported_fn = await import.defer('./mod_then_fn.mjs');
assertSame(ns_fn, imported_fn);
assertEquals(undefined, globalThis.then_ran_while_unevaluated);
assertEquals(undefined, globalThis.fn_module_evaluated);

const imported_tdz = await import.defer('./mod_then_tdz.mjs');
assertSame(ns_tdz, imported_tdz);
assertEquals(undefined, globalThis.tdz_module_evaluated);

// 3. "then" is excluded from the deferred namespace's exports, so it stays
// absent once the module has been evaluated.
assertEquals('initialized', ns_fn.tdz_binding);
assertTrue(globalThis.fn_module_evaluated);
assertEquals(['tdz_binding'], Reflect.ownKeys(ns_fn).filter(
    k => typeof k === 'string'));
assertEquals(['tdz_binding'], Object.keys(ns_fn));
for (const k in ns_fn) assertEquals('tdz_binding', k);
assertEquals(undefined, readThenIC(ns_fn));
assertEquals(undefined, Object.getOwnPropertyDescriptor(ns_fn, 'then'));
assertEquals(undefined, Object.prototype.__lookupGetter__.call(ns_fn, 'then'));
assertFalse('then' in ns_fn);
assertTrue(delete ns_fn.then);
