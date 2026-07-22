// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function opt() {
  const buffer = new ArrayBuffer(64);
  const view = new DataView(buffer);
  let i = 1n;
  i += 1n;
  view.setUint8(i);
}

%PrepareFunctionForOptimization(opt);
assertThrows(opt, TypeError);
%OptimizeFunctionOnNextCall(opt);
assertThrows(opt, TypeError);
