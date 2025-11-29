// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax


const f = async (...args) => {
    const x = typeof args;
    x.concat(f);
    const y = args.reduce(f, args);
    y[y];
};
%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
