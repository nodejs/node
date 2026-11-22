// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

globalThis.y = 1.5;  // PropertyCellType::kConstant (HeapNumber)
globalThis.z = 1.5;
globalThis.z = 2.5;  // PropertyCellType::kConstantType (HeapNumber)
globalThis.z = 1.5;

function store() {
  y = z;
}

%PrepareFunctionForOptimization(store);
store();
store();
%OptimizeFunctionOnNextCall(store);
store();
assertOptimized(store);
