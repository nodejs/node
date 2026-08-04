// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

(function() {
  function foo(x) {
    x.a = 1;
    x.a;
  }
  %PrepareFunctionForOptimization(foo);
  foo({});
  %OptimizeMaglevOnNextCall(foo);
  foo({});
  assertTrue(isMaglevved(foo));
})();
