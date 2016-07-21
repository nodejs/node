// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:  --expose-wasm

try {
  Wasm.instantiateModuleFromAsm("");
  assertTrue(false);
} catch (e) {
  print("Caught: " + e);
}
