// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

const a = {toString: () => {
  console.log("print arguments", print.arguments);
}};

function g(x) {
  print(x);
}

%PrepareFunctionForOptimization(g);
g(a);
g(a);
%OptimizeFunctionOnNextCall(g);
g(a);
