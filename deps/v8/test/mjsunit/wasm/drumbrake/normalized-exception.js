// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Regression test: DrumBrake reads a WasmExceptionPackage's private tag and
// values fields at fixed in-object offsets computed from the object's current
// map. Adding and then deleting an ordinary expando property on a
// WebAssembly.Exception normalizes it to a dictionary-mode map, which no
// longer backs those offsets with real storage. The interpreter must still
// recognize such an exception and correctly decode its payload, instead of
// crashing or reading out of bounds.
(function testCatchDecodesPayloadAfterPropertyDeletionNormalizesException() {
  const kSentinel = 0x1234;

  const builder = new WasmModuleBuilder();
  const tagIndex = builder.addImportedTag("m", "tag", kSig_v_i);
  const thrower = builder.addImport("m", "thrower", kSig_v_v);
  builder.addFunction("run", kSig_i_v)
      .addBody([
        kExprTry, kWasmI32,
          kExprCallFunction, thrower,
          kExprI32Const, 0,
        kExprCatch, tagIndex,
        kExprEnd,
      ])
      .exportFunc();

  const tag = new WebAssembly.Tag({parameters: ["i32"]});
  let exception;
  const instance = builder.instantiate(
      {m: {tag, thrower: () => { throw exception; }}});

  exception = new WebAssembly.Exception(tag, [kSentinel]);
  assertTrue(%HasFastProperties(exception));
  assertEquals(kSentinel, instance.exports.run());

  // Add then delete a plain expando property. This normalizes the exception
  // object to a dictionary-mode map (CLEAR_INOBJECT_PROPERTIES), shrinking
  // its real in-object storage while leaving it recognized by the JS API.
  exception.a = 1;
  exception.b = 2;
  delete exception.a;
  assertFalse(%HasFastProperties(exception));
  assertTrue(exception.is(tag));

  // The matching catch must still correctly recognize the tag and decode the
  // actual payload value after normalization, not just return some FixedArray.
  assertEquals(kSentinel, instance.exports.run());
})();
