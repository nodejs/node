// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Regression test for a call_indirect bug in the Wasm interpreter. The
// interpreter cached the FunctionSig* derived from the first sig_index used on
// a given table entry. A subsequent call_indirect on the same entry with a
// narrower subtype signature passed the canonical subtype check but then used
// the stale cached signature to coerce the JS import's return value. As a
// result, a JS value returned from `() -> anyref` could end up on the typed
// Wasm stack as a `(ref struct ...)` / `(ref array ...)`, causing later typed
// accesses to observe the wrong type. The interpreter must instead coerce
// JS<->Wasm values against the callee's declared signature, matching the
// behavior of compiled wrappers, so that the narrow callsite still rejects a
// non-conforming return value.
(function testCallIndirectStaleSignature() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const array_type = builder.addArray(kWasmI32, {mutable: true});

  // Wide cache-poisoning signature.
  const sig_any =
      builder.addType(makeSig([], [kWasmAnyRef]), kNoSuperType,
                      /* is_final */ false);
  // Narrow signature: a subtype of sig_any.
  const sig_array =
      builder.addType(makeSig([], [wasmRefType(array_type)]), sig_any,
                      /* is_final */ false);

  builder.addImport('m', 'imp', sig_array);
  builder.addTable(kWasmFuncRef, 1, 1);
  builder.addActiveElementSegment(0, wasmI32Const(0), [0]);

  // First call: invoke the entry through the wider signature. Without the
  // fix, this populates the interpreter's per-entry cache with sig_any's
  // FunctionSig*.
  builder.addFunction('warm_any', kSig_v_v).addBody([
    kExprI32Const, 0,
    kExprCallIndirect, sig_any, kTableZero,
    kExprDrop,
  ]).exportFunc();

  // Second call: invoke the same entry through the narrower signature and
  // immediately use the result as the typed array reference that the
  // signature promises. With the bug present, the JS return value (a Smi)
  // is coerced under the cached sig_any and reaches array.get unchecked.
  builder.addFunction('trigger', kSig_i_v).addBody([
    kExprI32Const, 0,
    kExprCallIndirect, sig_array, kTableZero,
    kExprI32Const, 0,
    kGCPrefix, kExprArrayGet, array_type,
  ]).exportFunc();

  // The JS import advertises sig_array, but actually returns a Smi.
  // The Wasm-to-JS boundary must reject this for sig_array, regardless of
  // any sig_any-warmed cache state.
  const instance = builder.instantiate({m: {imp() { return 13; }}});

  // Attempt to warm the cache through the wider sig_any callsite. With the
  // fix in place, JS<->Wasm coercion uses the callee's declared signature
  // (sig_array), so returning a Smi here already throws. The test only
  // requires that whatever cache state results cannot be used to bypass the
  // type check at the narrow callsite below.
  try {
    instance.exports.warm_any();
  } catch (e) {
    // Ignore.
  }

  // Now invoke through sig_array. The runtime must coerce the JS return
  // value against sig_array, so returning a Smi must trap with a typed-
  // conversion error rather than smuggling a Smi onto the typed stack.
  assertThrows(
      () => instance.exports.trigger(),
      TypeError);
})();
