// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-fast-api-calls --expose-fast-api --allow-natives-syntax --wasm-test-streaming

const fapi = new d8.test.FastCAPI();
let global = 42;
function fctWithFastApiCall() {
    return fapi.call_to_number(global);
}
%PrepareFunctionForOptimization(fctWithFastApiCall);
fctWithFastApiCall();
%OptimizeFunctionOnNextCall(fctWithFastApiCall);

global = {
    valueOf: function() {
        throw {};
    }
};

function f19(executor) {
    let resolve = function() { throw "never"; }
    let reject = fctWithFastApiCall;
    executor(resolve, reject);
}
Object.defineProperty(Promise, Symbol.species, { value: f19 });
WebAssembly.instantiateStreaming({}, Symbol.species).then();
