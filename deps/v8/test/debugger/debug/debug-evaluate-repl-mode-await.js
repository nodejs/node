// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

const evaluate = Debug.evaluateGlobalREPL;

(async () => {
    // Test that the completion value of the REPL script is the resolve value of the
    // promise returned by evalute.
    let result = evaluate('5;');
    assertPromiseResult(result, (value) => {
        assertEquals(5, value);
    }, assertUnreachable);

    // Test that top-level await in REPL mode works.
    result = evaluate('let x = await Promise.resolve(42);');
    assertPromiseResult(result, (value) => {
        assertEquals(undefined, value);
        assertEquals(42, x);
    }, assertUnreachable);

    // Test that a throwing REPL script results in a rejected promise.
    result = evaluate('throw new Error("ba dum tsh");');
    assertPromiseResult(result, assertUnreachable, (error) => {
        assertEquals("ba dum tsh", error.message);
    });

    // Test that a rejected promise throws.
    result = evaluate('await Promise.reject("Reject promise!");');
    assertPromiseResult(result, assertUnreachable, (error) => {
        assertEquals('Reject promise!', error);
    });

    // Test that we can bind a promise in REPL mode.
    await evaluate('let y = Promise.resolve(21);');
    assertPromiseResult(y, (value) => {
        assertEquals(21, value);
    }, assertUnreachable);
})().then(() => {
    print("Async test completed successfully.");
}).catch(e => {
    print(e.stack);
    %AbortJS("Async test is failing");
});
