// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var f32 = new Float32Array(40);

function foo(f32, deopt) {
  var v0 = f32[0];
  var v1 = f32[1];
  var v2 = f32[2];
  var v3 = f32[3];
  var v4 = f32[4];
  var v5 = f32[5];
  var v6 = f32[6];
  var v7 = f32[7];
  var v8 = f32[8];
  var v9 = f32[9];
  var v10 = f32[10];
  var v11 = f32[11];
  var v12 = f32[12];
  var v13 = f32[13];
  var v14 = f32[14];
  var v15 = f32[15];
  var v16 = f32[16];
  var v17 = f32[17];
  var v18 = f32[18];
  var v19 = f32[19];
  var v20 = f32[20];
  var v21 = f32[21];
  var v22 = f32[22];
  var v23 = f32[23];
  var v24 = f32[24];
  var v25 = f32[25];
  var v26 = f32[26];
  var v27 = f32[27];
  var v28 = f32[28];
  var v29 = f32[29];
  var v30 = f32[30];
  var v31 = f32[31];
  var v32 = f32[32];
  var v33 = f32[33];
  var v34 = f32[34];
  var v35 = f32[35];
  var v36 = f32[36];
  var v37 = f32[37];
  var v38 = f32[38];
  var v39 = f32[39];
  // Side effect to force the deopt after the store.
  f32[0] = v1 - 1;
  // Here we deopt once we warm up with numbers, but then we
  // pass a string as {deopt}.
  return deopt + v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 +
      v12 + v13 + v14 + v15 + v16 + v17 + v18 + v19 + v20 + v21 + v22 + v23 +
      v24 + v25 + v26 + v27 + v28 + v29 + v30 + v31 + v32 + v33 + v34 + v35 +
      v36 + v37 + v38 + v39;
}

var s = "";
for (var i = 0; i < f32.length; i++) {
  f32[i] = i;
  s += i;
}

%PrepareFunctionForOptimization(foo);
foo(f32, 0);
foo(f32, 0);
%OptimizeFunctionOnNextCall(foo);
assertEquals("x" + s, foo(f32, "x"));
