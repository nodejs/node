// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan

// Test LdaModuleVariable with imports.
import {b} from './lda-module-variable-import.mjs'

function bar(y) {
  // b = 1.
  return b + y;
};
%PrepareFunctionForOptimization(bar);
assertEquals(2, bar(1));
assertEquals(3, bar(2));
%OptimizeMaglevOnNextCall(bar);
assertEquals(2, bar(1));
assertEquals(3, bar(2));
assertTrue(isMaglevved(bar));

// Test LdaModuleVariable with exports.
export let x = 1;
function foo(y) { return x + y };

%PrepareFunctionForOptimization(foo);
assertEquals(2, foo(1));
assertEquals(3, foo(2));
%OptimizeMaglevOnNextCall(foo);
assertEquals(2, foo(1));
assertEquals(3, foo(2));
assertTrue(isMaglevved(foo));

