// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

"use strict"

function f(c) {
  if (c) { throw new Error(); }
  throw new Error();
};

function Error()  {
  return arguments.length;
}

%PrepareFunctionForOptimization(f);
assertThrows(function() { f(true); });
assertThrows(function() { f(false); });
%OptimizeFunctionOnNextCall(f);
assertThrows(function() { f(true); });
