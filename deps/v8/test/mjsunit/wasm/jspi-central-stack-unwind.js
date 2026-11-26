// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function probeNearStackLimit() {
  let nextTryDepth = Infinity;
  function recurse(depth) {
    let sentinel = depth ^ 0x5a5a5a;
    let result;
    try {
      result = recurse(depth + 1);
    } catch (e) {
      if (depth > nextTryDepth) throw e;
      nextTryDepth = depth - 64;
      print('at stack limit');
      result = new Error('probe').stack;
    }
    if (sentinel !== (depth ^ 0x5a5a5a)) {
      throw new Error('corrupted stack frame');
    }
    return result;
  }
  return recurse(0);
}

(function TestCentralStackGrowthUnwindToJspi() {
  const builder = new WasmModuleBuilder();
  const importIndex = builder.addImport('m', 'overflowCentralStack', kSig_v_v);
  builder.addFunction('main', kSig_v_v)
      .addBody([kExprCallFunction, importIndex])
      .exportFunc();

  function overflowCentralStack() {
    return overflowCentralStack();
  }

  const instance = builder.instantiate({m: {overflowCentralStack}});
  const promisingMain = WebAssembly.promising(instance.exports.main);

  // Grow the central stack inside a JSPI call and unwind the uncaught
  // RangeError back into the secondary stack's JSPI handler.
  assertPromiseResult(
      promisingMain(),
      () => assertUnreachable(),
      e => assertInstanceof(e, RangeError));

  // Recurse across the already-committed central stack pages and call into C++
  // at the stack limit. On Windows, if TEB->StackLimit was restored to a stale
  // pre-growth value, __chkstk will zero bytes across live caller frames.
  const stack = probeNearStackLimit();
  assertTrue(typeof stack === 'string' && stack.length > 0);
})();
