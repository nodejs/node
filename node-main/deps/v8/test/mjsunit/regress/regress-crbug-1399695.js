// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __f_3() {
  isNaN.prototype = 14;
}
%PrepareFunctionForOptimization(__f_3);
__f_3();
%OptimizeFunctionOnNextCall(__f_3);
__f_3();
