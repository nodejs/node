// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

// Flags: --allow-natives-syntax

(()=> {
  function f(a) {
    return a.charAt(1) == "";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aaa"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aaa"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) < "";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aaa"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aaa"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) <= "";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aaa"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aaa"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) > "";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f("aaa"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f("aaa"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) >= "";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f("aaa"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f("aaa"));
})();


(()=> {
  function f(a) {
    return a.charAt(1) == a.charAt(2);
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aab"));
  assertEquals(true, f("aaa"));
  assertEquals(false, f("acb"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aab"));
  assertEquals(true, f("aaa"));
  assertEquals(false, f("acb"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) < a.charAt(2);
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f("aab"));
  assertEquals(false, f("aaa"));
  assertEquals(false, f("acb"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f("aab"));
  assertEquals(false, f("aaa"));
  assertEquals(false, f("acb"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) <= a.charAt(2);
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f("aab"));
  assertEquals(true, f("aaa"));
  assertEquals(false, f("acb"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f("aab"));
  assertEquals(true, f("aaa"));
  assertEquals(false, f("acb"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) > a.charAt(2);
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aab"));
  assertEquals(false, f("aaa"));
  assertEquals(true, f("acb"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aab"));
  assertEquals(false, f("aaa"));
  assertEquals(true, f("acb"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) >= a.charAt(2);
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aab"));
  assertEquals(true, f("aaa"));
  assertEquals(true, f("acb"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aab"));
  assertEquals(true, f("aaa"));
  assertEquals(true, f("acb"));
})();


(()=> {
  function f(a) {
    return a.charAt(1) == "b";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aaa"));
  assertEquals(true, f("bbb"));
  assertEquals(false, f("ccc"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aaa"));
  assertEquals(true, f("bbb"));
  assertEquals(false, f("ccc"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) == "bb";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aaa"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aaa"));
})();


(()=> {
  function f(a) {
    return a.charAt(1) < "b";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f("aaa"));
  assertEquals(false, f("bbb"));
  assertEquals(false, f("ccc"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f("aaa"));
  assertEquals(false, f("bbb"));
  assertEquals(false, f("ccc"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) < "bb";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f("aaa"));
  assertEquals(true, f("bbb"));
  assertEquals(false, f("ccc"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f("aaa"));
  assertEquals(true, f("bbb"));
  assertEquals(false, f("ccc"));
})();


(()=> {
  function f(a) {
    return a.charAt(1) <= "b";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f("aaa"));
  assertEquals(true, f("bbb"));
  assertEquals(false, f("ccc"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f("aaa"));
  assertEquals(true, f("bbb"));
  assertEquals(false, f("ccc"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) <= "bb";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f("aaa"));
  assertEquals(true, f("bbb"));
  assertEquals(false, f("ccc"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f("aaa"));
  assertEquals(true, f("bbb"));
  assertEquals(false, f("ccc"));
})();


(()=> {
  function f(a) {
    return a.charAt(1) > "b";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aaa"));
  assertEquals(false, f("bbb"));
  assertEquals(true, f("ccc"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aaa"));
  assertEquals(false, f("bbb"));
  assertEquals(true, f("ccc"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) > "bb";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aaa"));
  assertEquals(false, f("bbb"));
  assertEquals(true, f("ccc"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aaa"));
  assertEquals(false, f("bbb"));
  assertEquals(true, f("ccc"));
})();


(()=> {
  function f(a) {
    return a.charAt(1) >= "b";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aaa"));
  assertEquals(true, f("bbb"));
  assertEquals(true, f("ccc"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aaa"));
  assertEquals(true, f("bbb"));
  assertEquals(true, f("ccc"));
})();

(()=> {
  function f(a) {
    return a.charAt(1) >= "bb";
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f("aaa"));
  assertEquals(false, f("bbb"));
  assertEquals(true, f("ccc"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f("aaa"));
  assertEquals(false, f("bbb"));
  assertEquals(true, f("ccc"));
})();
