// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --no-lazy-feedback-allocation

function probe(id, value) {
    let originalPrototype, newPrototype;
    let handler = {
        get(target, key, receiver) {
            if (key === '__proto__' && receiver === value) return originalPrototype;
            if (receiver === newPrototype) return ReflectGet(target, key);
            recordActionWithErrorHandling(PROPERTY_LOAD, id, target, key);
            return ReflectGet(target, key, receiver);
        },
    };
    try {} catch (e) {}
}

function f0() {
    return f0;
}

function f1(a2) {
    a2 <= a2;
    return a2;
}

function f4(a5) {
    function f6() {
        return a5;
    }
    for (const v7 in f6) {
    }
    const v8 = a5 >>> f0;
    return v8 & v8;
}

function f10(a11) {
    probe("v56", f1(f4(a11)));
    return a11;
}

%PrepareFunctionForOptimization(f10);
f1();
f10(0);
%OptimizeFunctionOnNextCall(f10);
f10(0);
