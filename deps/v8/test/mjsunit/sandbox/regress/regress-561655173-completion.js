// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-memory-corruption-api --expose-gc --expose-async-hooks

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/sandbox/wasm-jspi.js');

const rejectOffset =
    Sandbox.getFieldOffset(kPromiseReaction, 'reject_handler');

function handler(promise, reject = false) {
  const reaction = getField(getPtr(promise), kJSPromiseReactionsOrResultOffset);
  const offset = reject ? rejectOffset : kPromiseReactionFulfillHandlerOffset;
  return Sandbox.getObjectAt(getField(reaction, offset));
}

function makeWasm(body, imported) {
  const builder = new WasmModuleBuilder();
  builder.addImport('m', 's', kSig_i_v);
  builder.addFunction('f', kSig_i_v).addBody(body).exportFunc();
  return builder.instantiate({m: {s: new WebAssembly.Suspending(imported)}})
      .exports.f;
}

const once = [kExprCallFunction, 0];
const twice = [...once, kExprDrop, ...once];
const caught = [
  kExprTry, kWasmI32, ...once,
  kExprCatchAll, ...wasmI32Const(73),
  kExprEnd
];

function start(body = once) {
  const pending = [];
  const f = makeWasm(body, () => {
    const promise = new Promise(() => {});
    pending.push(promise);
    return promise;
  });
  return {pending, result: WebAssembly.promising(f)()};
}

function invoke(callback, value) {
  return value instanceof {[Symbol.hasInstance]: callback};
}

const sync = WebAssembly.promising(makeWasm([kExprI32Const, 42], () => 0));
assertPromiseResult(sync(), value => assertEquals(42, value));
const reason = {};
const throws = WebAssembly.promising(makeWasm(once, () => { throw reason; }));
assertPromiseResult(throws(), assertUnreachable,
                    value => assertSame(reason, value));

{
  const state = start();
  assertFalse(invoke(handler(state.pending[0]), 42));
  assertPromiseResult(state.result, value => assertEquals(42, value));
}
{
  const state = start();
  assertEquals(undefined, handler(state.pending[0])(42));
  assertPromiseResult(state.result, value => assertEquals(42, value));
}
{
  const state = start();
  assertEquals(undefined, handler(state.pending[0], true)(reason));
  assertPromiseResult(state.result, assertUnreachable,
                      value => assertSame(reason, value));
}
{
  const state = start(caught);
  assertFalse(invoke(handler(state.pending[0], true), reason));
  assertPromiseResult(state.result, value => assertEquals(73, value));
}
{
  const state = start(twice);
  assertFalse(invoke(handler(state.pending[0]), 1));
  assertEquals(2, state.pending.length);
  assertFalse(invoke(handler(state.pending[1]), 42));
  assertPromiseResult(state.result, value => assertEquals(42, value));
}

// A resolve hook can switch stacks again and replace the parent's jump buffer.
// It must not replace the return address of the outer completion/rejection.
function testReentry(reject, installHook) {
  let state;
  let nested;
  let hookCount = 0;
  const disable = installHook(promise => {
    if (!state || promise !== state.result) return;
    hookCount++;
    gc();
    nested = sync();
  });
  try {
    state = start();
    assertFalse(invoke(handler(state.pending[0], reject), 42));
  } finally {
    disable();
  }
  assertEquals(1, hookCount);
  assertPromiseResult(nested, value => assertEquals(42, value));
  const check = value => assertEquals(42, value);
  if (reject) {
    assertPromiseResult(state.result, assertUnreachable, check);
  } else {
    assertPromiseResult(state.result, check);
  }
}

// RejectPromise invokes the embedder hook on all d8 configurations.
testReentry(true, resolve => {
  const promises = new Map();
  const hook = async_hooks.createHook({
    init(id, type, triggerId, promise) { promises.set(id, promise); },
    promiseResolve(id) { resolve(promises.get(id)); }
  });
  hook.enable();
  return () => hook.disable();
});

// Direct fulfillment invokes the optional JavaScript context hook.
let hasContextHooks = false;
try {
  d8.promise.setHooks();
  hasContextHooks = true;
} catch {}
if (hasContextHooks) {
  for (const reject of [false, true]) {
    testReentry(reject, resolve => {
      d8.promise.setHooks(undefined, undefined, undefined, resolve);
      return () => d8.promise.setHooks();
    });
  }
}
