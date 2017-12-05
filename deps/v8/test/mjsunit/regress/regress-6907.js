// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestDematerializedContextInBuiltin() {
  var f = function() {
    var b = [1,2,3];
    var callback = function(v,i,o) {
      %_DeoptimizeNow();
    };
    try { throw 0 } catch(e) {
      return b.forEach(callback);
    }
  }
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
})();
