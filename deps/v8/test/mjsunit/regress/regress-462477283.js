// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

const __v_8 = new Worker(function() {}, { type: "function" });
const __v_9 = __v_8.terminateAndWait;

function __f_2() {}
__f_2.prototype.bar = function () { new __v_9(); };

function __f_3() {
  try { new __f_2().bar(); } catch (e) {}
}

%PrepareFunctionForOptimization(__f_2.prototype.bar);
__f_3();
__f_3();
%OptimizeFunctionOnNextCall(__f_2.prototype.bar);
__f_3();
