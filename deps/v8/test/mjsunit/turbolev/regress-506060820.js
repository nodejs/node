// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

let target;
function* g() {
  // Trigger a lazy deopt while we are inside the inlined
  // Generator.prototype.next call, so that on return we land in the
  // generator-next lazy-deopt continuation with an in-flight exception.
  %DeoptimizeFunction(target);
  throw new Error("boom");
}

function caller() {
  return g().next();
}
target = caller;

%PrepareFunctionForOptimization(caller);
try { caller(); } catch (e) {}
try { caller(); } catch (e) {}
%OptimizeFunctionOnNextCall(caller);

assertThrows(caller, Error, "boom");
