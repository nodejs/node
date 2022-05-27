// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab --allow-natives-syntax --turbofan

"use strict";

d8.file.execute('test/mjsunit/typedarray-helpers.js');

(function TypedArrayLength() {
  for(let ctor of ctors) {
    // We have to make sure that we construct a new string for each case to
    // prevent the compiled function from being reused with spoiled feedback.
    const test = new Function('\
      const rab = CreateResizableArrayBuffer(16, 40); \
      const ta = new ' + ctor.name + '(rab); \
      rab.resize(32); \
      return ta.length;');

    %PrepareFunctionForOptimization(test);
    assertEquals(32 / ctor.BYTES_PER_ELEMENT, test(ctor));
    assertEquals(32 / ctor.BYTES_PER_ELEMENT, test(ctor));
    %OptimizeFunctionOnNextCall(test);
    assertEquals(32 / ctor.BYTES_PER_ELEMENT, test(ctor));
  }
})();
