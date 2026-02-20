// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jit-fuzzing

for (let __v_6 = 0; __v_6 < 87; __v_6++) {
  async function* __f_3() { }
  __f_3(), __f_3(), __f_3().next();
}
