// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

let hex_b = 0x0b;
let hex_d = 0x0d;
let hex_20 = 0x20;
let hex_52 = 0x52;
let hex_fe = 0xfe;

function f(a) {
  let unused = [ a / 8, ...[ ...[ ...[], a / 8, ...[ 7, hex_fe, a, 0, 0, hex_20,
    6, hex_52, hex_d, 0, hex_b], 0, hex_b], hex_b]];
}

%PrepareFunctionForOptimization(f)
f(64)
f(64);
%OptimizeFunctionOnNextCall(f);
f(64);
