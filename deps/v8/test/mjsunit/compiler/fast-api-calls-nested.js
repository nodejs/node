// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --expose-fast-api --allow-natives-syntax --turbofan --deopt-every-n-times=0 --stress-compaction

const my_object = {
    toString: function() {
        gc();
        return 2;
    }
};

function foo(fast_c_api) {
    // Fast API call that calls back to JS because of ToNumber() on my_object:
    return fast_c_api.call_to_number(my_object);
}

const fast_c_api = new d8.test.FastCAPI();

%PrepareFunctionForOptimization(foo);
foo(fast_c_api);

%OptimizeFunctionOnNextCall(foo);
foo(fast_c_api);

assertEquals(1, fast_c_api.fast_call_count());
assertEquals(1, fast_c_api.slow_call_count());
