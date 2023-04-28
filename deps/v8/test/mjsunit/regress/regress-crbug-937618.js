// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let target = {0: 42, a: 42};

let proxy = new Proxy(target, {
  has: function() {
    return false;
  }
});

Object.preventExtensions(target);

function testLookupElementInProxy() {
  0 in proxy;
}

// 9.5.7 [[HasProperty]] 9. states that if the trap returns false, and the
// target hasOwnProperty, and the target is non-extensible, throw a type error.
;
%PrepareFunctionForOptimization(testLookupElementInProxy);
assertThrows(testLookupElementInProxy, TypeError);
assertThrows(testLookupElementInProxy, TypeError);
%OptimizeFunctionOnNextCall(testLookupElementInProxy);
assertThrows(testLookupElementInProxy, TypeError);

function testLookupPropertyInProxy() {
  "a" in proxy;
};
%PrepareFunctionForOptimization(testLookupPropertyInProxy);
assertThrows(testLookupPropertyInProxy, TypeError);
assertThrows(testLookupPropertyInProxy, TypeError);
%OptimizeFunctionOnNextCall(testLookupPropertyInProxy);
assertThrows(testLookupPropertyInProxy, TypeError);
