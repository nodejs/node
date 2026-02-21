// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --trace-turbo-graph --noconcurrent-recompilation

function bytes() {
}
function __f_4622() {
  var __v_22507 = {
  };
}
%PrepareFunctionForOptimization(__f_4622);
%OptimizeFunctionOnNextCall(__f_4622);
42 |  __f_4622();
