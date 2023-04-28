// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

import { getImportMeta } from 'modules-skip-import-meta-export.mjs';

function foo() {
  return import.meta;
}
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
// Optimize when import.meta hasn't been created yet.
assertEquals('object', typeof foo());
assertEquals(import.meta, foo());
assertOptimized(foo);

function bar() {
  return import.meta;
}
%PrepareFunctionForOptimization(bar);
// Optimize when import.meta already exists.
%OptimizeFunctionOnNextCall(bar);
assertEquals(import.meta, bar());
assertOptimized(bar);

%PrepareFunctionForOptimization(getImportMeta);
%OptimizeFunctionOnNextCall(getImportMeta);
assertEquals('object', typeof getImportMeta());
assertOptimized(getImportMeta);
assertNotEquals(import.meta, getImportMeta());
assertOptimized(getImportMeta);


function baz() {
  return getImportMeta();
}

// Test inlined (from another module) import.meta accesses.
%PrepareFunctionForOptimization(baz);
baz();
%OptimizeFunctionOnNextCall(baz);
assertEquals('object', typeof baz());
assertNotEquals(import.meta, baz());
assertEquals(baz(), getImportMeta());
assertOptimized(baz);
