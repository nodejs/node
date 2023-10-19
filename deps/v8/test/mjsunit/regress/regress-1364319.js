// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function f() {
  return "abcd".charCodeAt(BigInt.asUintN(0, -1307n));
}

%PrepareFunctionForOptimization(f);
try { f(); } catch(e) {}
try { f(); } catch(e) {}
%OptimizeFunctionOnNextCall(f);
assertThrows(f, TypeError);
