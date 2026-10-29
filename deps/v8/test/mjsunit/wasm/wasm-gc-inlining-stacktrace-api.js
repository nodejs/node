// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan
// Flags: --no-always-sparkplug

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function testOptimized(
  run, jsWasmCaller, wasmInstance,
  expectedLineNumber, expectedColumnNumber, expectedOuterFrames
) {
  // Check the properties of the structured stack frames.
  Error.prepareStackTrace = (error, frames) => {
    assertTrue(error instanceof WebAssembly.RuntimeError);
    assertEquals("dereferencing a null pointer", error.message);

    // Inspect the frame of the inlined Wasm function.
    let frame = frames[0];
    assertSame(wasmInstance, frame.getThis());
    // For Wasm frames, there is no JS receiver (no 'this' object with a constructor),
    // so the type name is null.
    assertSame(null, frame.getTypeName());
    assertEquals('arrayLenNull', frame.getFunctionName());
    assertSame(null, frame.getMethodName());
    assertMatches(/wasm:\/\/wasm\/[0-9a-f]+/,
      frame.getFileName());
    // For Wasm, getFunction() returns the Wasm function index.
    const wasmFunctionIndex = 0;
    assertSame(wasmFunctionIndex, frame.getFunction());
    // For Wasm functions, these properties are repurposed:
    // - the line number is always 1
    assertEquals(1, frame.getLineNumber());
    // - the bytecode offset (1-based)
    const wasmFunctionBytecodeOffset = 0x37;
    assertEquals(wasmFunctionBytecodeOffset + 1, frame.getColumnNumber());
    // - the bytecode offset (0-based)
    assertEquals(wasmFunctionBytecodeOffset, frame.getPosition());
    assertSame(undefined, frame.getEvalOrigin());
    assertFalse(frame.isToplevel());
    assertFalse(frame.isEval());
    assertFalse(frame.isNative());
    assertFalse(frame.isConstructor());
    assertFalse(frame.isAsync());
    assertFalse(frame.isPromiseAll());
    assertSame(null, frame.getPromiseIndex());

    // Inspect the frame of the caller JavaScript function.
    frame = frames[1];
    assertSame(globalThis, frame.getThis());
    assertSame(null, frame.getTypeName());
    assertSame(jsWasmCaller, frame.getFunction());
    assertEquals(jsWasmCaller.name, frame.getFunctionName());
    assertSame(null, frame.getMethodName());
    assertMatches(/.*wasm-gc-inlining-stacktrace-api\.js/,
      frame.getFileName());
    assertEquals(expectedLineNumber, frame.getLineNumber());
    assertEquals(expectedColumnNumber, frame.getColumnNumber());
    assertEquals('number', typeof frame.getPosition());
    assertSame(undefined, frame.getEvalOrigin());
    // The arrow function inherits 'this' from TestInliningTrapStackTrace,
    // which is executed in the global context, hence it is toplevel.
    assertTrue(frame.isToplevel());
    assertFalse(frame.isEval());
    assertFalse(frame.isNative());
    assertFalse(frame.isConstructor());
    assertFalse(frame.isAsync());
    assertFalse(frame.isPromiseAll());
    assertSame(null, frame.getPromiseIndex());

    // Do shallow tests for the outer JS functions.
    for (let i = 0; i < expectedOuterFrames.length; ++i) {
      assertEquals(expectedOuterFrames[i], frames[i + 2].getFunctionName());
    }

    return 'This is a custom stack trace';
  };

  // Run once unoptimized and once optimized.
  %PrepareFunctionForOptimization(jsWasmCaller);
  run();
  %OptimizeFunctionOnNextCall(jsWasmCaller);
  run();
  // Re-prepare the function immediately to make sure type feedback isn't
  // cleared by an untimely gc.
  %PrepareFunctionForOptimization(jsWasmCaller);
  assertOptimized(jsWasmCaller);
}

(function TestInliningTrapStackTrace() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  const array = builder.addArray(kWasmI32);

  // Inlined Wasm function that traps (due to null array).
  builder.addFunction('arrayLenNull', kSig_i_r)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCastNull, array,
      kGCPrefix, kExprArrayLen,
    ])
    .exportFunc();

  const instance = builder.instantiate({});
  const wasm = instance.exports;


  const getTrap = () => {
    return wasm.arrayLenNull(null);
  };

  const testTrap = () => {
    try {
      getTrap();
      assertUnreachable();
    } catch (e) {
      assertEquals('This is a custom stack trace', e.stack);
    }
  };

  testOptimized(testTrap, getTrap, instance, 112, 17, [
    'testTrap', 'testOptimized', 'TestInliningTrapStackTrace'
  ]);

  {
    print('Test with wasm call within a try catch block');

    // Note: At the moment of writing this test case, inlining of the wasm call
    // is not yet supported. This test only ensures that supporting it in the
    // future won't break the stack trace API behavior.
    const getTrapTryCatch = () => {
      try {
        return wasm.arrayLenNull(null);
      } catch (e) {
        assertEquals('This is a custom stack trace', e.stack);
      }
    };

    testOptimized(getTrapTryCatch, getTrapTryCatch, instance, 136, 21, [
      'testOptimized', 'TestInliningTrapStackTrace'
    ]);
  }
})();
