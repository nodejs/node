// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

const obj = {
  func() {
    const arrow = () => {
      let caught = false;
      try {
        return super.length;
      } catch (e) {
        caught = true;
      }
      return caught;
    };
    %PrepareFunctionForOptimization(arrow);
    %OptimizeFunctionOnNextCall(arrow);
    return arrow();
  }
};

const ta = new Uint8Array();
obj.__proto__ = ta;
assertTrue(obj.func());
assertTrue(obj.func());
