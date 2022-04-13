// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let __v_20 = new Int32Array();
__v_20.set({
    get length() {
      %ArrayBufferDetach(__v_20.buffer);
    }
  });

function bar() { return array[0]; }
var array = new Float32Array(1000);
%PrepareFunctionForOptimization(bar);
bar();
bar();
%OptimizeFunctionOnNextCall(bar);
bar();
