// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This test makes sure that the Runtime call to MajorGCForCompilerTesting
// correctly sets the Context register. To do so, we creates lots of temporary
// variables so that all registers (including kContextRegister) are being used
// and contain non-zero Smis. Then we call %MajorGCForCompilerTesting(), and if
// it doesn't set the context register, the runtime function will receive a
// non-zero Smi as context, and will crash.

function foo(a) {
  let v0 = a + 0;
  let v1 = a + 1;
  let v2 = a + 2;
  let v3 = a + 3;
  let v4 = a + 4;
  let v5 = a + 5;
  let v6 = a + 6;
  let v7 = a + 7;
  let v8 = a + 8;
  let v9 = a + 9;
  let v10 = a + 10;
  let v11 = a + 11;
  let v12 = a + 12;
  let v13 = a + 13;
  let v14 = a + 14;
  let v15 = a + 15;
  let v16 = a + 16;
  let v17 = a + 17;
  let v18 = a + 18;
  let v19 = a + 19;

  %MajorGCForCompilerTesting();

  return v0+v1+v2+v3+v4+v5+v6+v7+v8+v9+v10+
      v11+v12+v13+v14+v15+v16+v17+v18+v19;
}

%PrepareFunctionForOptimization(foo);
assertEquals(210, foo(1));
assertEquals(210, foo(1));

%OptimizeMaglevOnNextCall(foo);
assertEquals(210, foo(1));
