// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(len) {
  return new Array(len);
};
%PrepareFunctionForOptimization(bar);
bar(0);
%OptimizeFunctionOnNextCall(bar);
bar(0);
