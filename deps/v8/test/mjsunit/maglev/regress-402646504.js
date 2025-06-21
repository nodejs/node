// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan

const obj = {
  func(optimize) {
      const arrowFunc = () => {
        return super.length;
      };
      %PrepareFunctionForOptimization(arrowFunc);
      if (optimize) {
        %OptimizeMaglevOnNextCall(arrowFunc);
      }
      return arrowFunc();
  }
};

// Make super.length polymorphic:
// Case 1:
assertEquals(undefined, obj.func(false));

// Case 2:
const u8Arr = new Uint8Array(20);
obj.__proto__ = u8Arr;
assertThrows(() => { obj.func(false); }, TypeError);

// Optimize for Maglev.
assertThrows(() => { obj.func(true); }, TypeError);
