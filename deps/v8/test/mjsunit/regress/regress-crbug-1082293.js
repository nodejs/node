// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  let buffer = new ArrayBuffer(8);
  let a32 = new Float32Array(buffer);
  let a8 = new Uint32Array(buffer);
  let a = { value: NaN };
  Object.defineProperty(a32, 0, { value: NaN });
  return a8[0];
}

let value = f();
%EnsureFeedbackVectorForFunction(f);
assertEquals(value, f());
