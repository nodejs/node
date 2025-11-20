// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

(function() {
function thingo(i, b) {
  var s = b ? "ac" : "abcd";
  i = i >>> 0;
  if (i < s.length) {
    var c = s.charCodeAt(i);
    gc();
    return c;
  }
};
%PrepareFunctionForOptimization(thingo);
thingo(0, true);
thingo(0, true);
%OptimizeFunctionOnNextCall(thingo);
thingo(0, true);
})();
