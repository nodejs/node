// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

/* Test MapCheck behavior */

(function testForEachMapCheck() {
    function f(v,n,o) {
        Object.freeze(o);
    }
    function g() {
        [1,2,3].forEach(f);
    }
    %PrepareFunctionForOptimization(g);
    g();
    g();
    %OptimizeFunctionOnNextCall(g);
    g();
    %PrepareFunctionForOptimization(g);
    %OptimizeFunctionOnNextCall(g);
    g();
    assertOptimized(g);
})();


(function testFindMapCheck() {
    function f(v,n,o) {
        Object.freeze(o);
        return false;
    }
    function g() {
        [1,2,3].find(f);
    }
    %PrepareFunctionForOptimization(g);
    g();
    g();
    %OptimizeFunctionOnNextCall(g);
    g();
    %PrepareFunctionForOptimization(g);
    %OptimizeFunctionOnNextCall(g);
    g();
    assertOptimized(g);
})();

(function testMapMapCheck() {
    function f(v,n,o) {
        Object.freeze(o);
        return false;
    }
    function g() {
        [1,2,3].map(f);
    }
    %PrepareFunctionForOptimization(g);
    g();
    g();
    %OptimizeFunctionOnNextCall(g);
    g();
    %PrepareFunctionForOptimization(g);
    %OptimizeFunctionOnNextCall(g);
    g();
    assertOptimized(g);
})();

(function testFilterMapCheck() {
    function f(v,n,o) {
        Object.freeze(o);
        return true;
    }
    function g() {
        [1,2,3].filter(f);
    }
    %PrepareFunctionForOptimization(g);
    g();
    g();
    %OptimizeFunctionOnNextCall(g);
    g();
    %PrepareFunctionForOptimization(g);
    %OptimizeFunctionOnNextCall(g);
    g();
    assertOptimized(g);
})();


/* Test CheckBounds behavior */

(function testForEachCheckBounds() {
    function f(v,n,o) {
        o.length=2;
    }
    function g() {
        [1,2,3].forEach(f);
    }
    %PrepareFunctionForOptimization(g);
    g();
    g();
    %OptimizeFunctionOnNextCall(g);
    g();
    %PrepareFunctionForOptimization(g);
    %OptimizeFunctionOnNextCall(g);
    g();
    assertOptimized(g);
})();


(function testFindCheckBounds() {
    function f(v,n,o) {
        o.length=2;
        return false;
    }
    function g() {
        [1,2,3].find(f);
    }
    %PrepareFunctionForOptimization(g);
    g();
    g();
    %OptimizeFunctionOnNextCall(g);
    g();
    %PrepareFunctionForOptimization(g);
    %OptimizeFunctionOnNextCall(g);
    g();
    assertOptimized(g);
})();

(function testMapCheckBounds() {
    function f(v,n,o) {
        o.length=2;
        return false;
    }
    function g() {
        [1,2,3].map(f);
    }
    %PrepareFunctionForOptimization(g);
    g();
    g();
    %OptimizeFunctionOnNextCall(g);
    g();
    %PrepareFunctionForOptimization(g);
    %OptimizeFunctionOnNextCall(g);
    g();
    assertOptimized(g);
})();

(function testFilterCheckBounds() {
    function f(v,n,o) {
        o.length = 2;
        return true;
    }
    function g() {
        [1,2,3].filter(f);
    }
    %PrepareFunctionForOptimization(g);
    g();
    g();
    %OptimizeFunctionOnNextCall(g);
    g();
    g();
    %PrepareFunctionForOptimization(g);
    %OptimizeFunctionOnNextCall(g);
    g();
    g();
    assertOptimized(g);
})();
