// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const limit = %TypedArrayMaxLength() + 1;

(function() {
  function foo() {
    try { new Int8Array(limit); } catch (e) { return e.stack; }
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(/new Int8Array/.test(foo()));
  assertTrue(/new Int8Array/.test(foo()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(/new Int8Array/.test(foo()));
})();

(function() {
  function foo() {
    try { new Uint8Array(limit); } catch (e) { return e.stack; }
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(/new Uint8Array/.test(foo()));
  assertTrue(/new Uint8Array/.test(foo()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(/new Uint8Array/.test(foo()));
})();

(function() {
  function foo() {
    try { new Uint8ClampedArray(limit); } catch (e) { return e.stack; }
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(/new Uint8ClampedArray/.test(foo()));
  assertTrue(/new Uint8ClampedArray/.test(foo()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(/new Uint8ClampedArray/.test(foo()));
})();

(function() {
  function foo() {
    try { new Int16Array(limit); } catch (e) { return e.stack; }
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(/new Int16Array/.test(foo()));
  assertTrue(/new Int16Array/.test(foo()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(/new Int16Array/.test(foo()));
})();

(function() {
  function foo() {
    try { new Uint16Array(limit); } catch (e) { return e.stack; }
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(/new Uint16Array/.test(foo()));
  assertTrue(/new Uint16Array/.test(foo()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(/new Uint16Array/.test(foo()));
})();

(function() {
  function foo() {
    try { new Int32Array(limit); } catch (e) { return e.stack; }
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(/new Int32Array/.test(foo()));
  assertTrue(/new Int32Array/.test(foo()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(/new Int32Array/.test(foo()));
})();

(function() {
  function foo() {
    try { new Uint32Array(limit); } catch (e) { return e.stack; }
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(/new Uint32Array/.test(foo()));
  assertTrue(/new Uint32Array/.test(foo()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(/new Uint32Array/.test(foo()));
})();

(function() {
  function foo() {
    try { new Float32Array(limit); } catch (e) { return e.stack; }
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(/new Float32Array/.test(foo()));
  assertTrue(/new Float32Array/.test(foo()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(/new Float32Array/.test(foo()));
})();


(function() {
  function foo() {
    try { new Float64Array(limit); } catch (e) { return e.stack; }
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(/new Float64Array/.test(foo()));
  assertTrue(/new Float64Array/.test(foo()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(/new Float64Array/.test(foo()));
})();

(function() {
  function foo() {
    try { new BigInt64Array(limit); } catch (e) { return e.stack; }
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(/new BigInt64Array/.test(foo()));
  assertTrue(/new BigInt64Array/.test(foo()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(/new BigInt64Array/.test(foo()));
})();

(function() {
  function foo() {
    try { new BigUint64Array(limit); } catch (e) { return e.stack; }
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(/new BigUint64Array/.test(foo()));
  assertTrue(/new BigUint64Array/.test(foo()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(/new BigUint64Array/.test(foo()));
})();
