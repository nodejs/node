// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

function g(
  // Too many arguments to fit here manually and overflow int32 calculation
) {
  return arguments.length + 1;
}

var num_args = 40000; // large number to cause overflow

// Construct argument list string
var argsList = "";
for (var i = 0; i < num_args; i++) argsList += "a" + i + ",";
argsList = argsList.slice(0, -1);

// Construct function source to return sum of all args
var body = "return arguments.length + 1;";

// Create large argument function dynamically
var bigArgFunc = new Function(argsList, body);

// Call many times to trigger OSR in v8 maglev
for (var i = 0; i < 2; i++) {
  bigArgFunc(0);
  %OptimizeOsr();
}
