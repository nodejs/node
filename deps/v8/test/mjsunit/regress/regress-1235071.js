// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --noanalyze-environment-liveness --interrupt-budget=1000 --allow-natives-syntax

function __f_4() {
  var __v_3 = function() {};
  var __v_5 = __v_3.prototype;
  Number.prototype.__proto__ = __v_3;
  __v_5, __v_3.prototype;
}
%PrepareFunctionForOptimization(__f_4);
for (let i = 0; i < 100; i++) __f_4();
