// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo() {
  for (let x in x) {
  }
}

%PrepareFunctionForOptimization(foo);
assertThrows(()=>{ foo(); });
assertThrows(()=>{ foo(); });
%OptimizeFunctionOnNextCall(foo);
assertThrows(()=>{ foo(); });
