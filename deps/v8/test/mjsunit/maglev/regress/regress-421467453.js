// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

async function* f2() {
  const v8 = (function(){}).apply;
  try { v8(); } catch (e) {}
  const v6 = [1.1];
  for (let v10 = 0; v10 < 1; v10++) {
    try { v8(); } catch (e) {}
    let [v14] = v6;
    function f17() { return v6; }
    return {};
  }
}
%PrepareFunctionForOptimization(f2);
f2().next();
%OptimizeMaglevOnNextCall(f2);
