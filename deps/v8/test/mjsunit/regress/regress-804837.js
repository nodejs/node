// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax

var __v_25662 = [, 1.8];
function __f_6214(__v_25668) {
  __v_25662.reduce(() => {
    1;
  });
};
%PrepareFunctionForOptimization(__f_6214);
__f_6214();
__f_6214();
%OptimizeFunctionOnNextCall(__f_6214);
__f_6214();
