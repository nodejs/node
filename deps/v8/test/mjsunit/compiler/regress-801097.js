// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function GetFunction() {
  var source = "return ((dividend | 0) / ((";
  for (var i = 0; i < 0x8000; i++) {
    source += "a,"
  }
  source += "a) | 0)) | 0";
  return Function("dividend", source);
}

var func = GetFunction();
%PrepareFunctionForOptimization(func);
assertThrows("func();");
%OptimizeFunctionOnNextCall(func);
assertThrows("func()");
