// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-bytecode-full

function f(x) {
  switch (x) {
    case 0: return 1;
    case 1: return 2;
    case 2: return 3;
    case 3: return 4;
    case 4: return 5;
    case 5: return 6;
  }
  return 0;
}
// Allocate feedback vector and populate embedded switch feedback naturally
// without setting kManual tiering (since %InstallBytecode discards
// FeedbackMetadata, which triggers a DCHECK in %OptimizeFunctionOnNextCall).
for (let i = 0; i < 40; i++) f(0);

const bc = %GetBytecode(f);
if (bc) {
  const bytes = new Uint8Array(bc.bytecode);
  for (let i = 0; i + 5 < bytes.length; i++) {
    if (bytes[i] === 0x0b && bytes[i + 1] === 0xf9 &&
        bytes[i + 3] === 0x00 && bytes[i + 4] === 0x06 &&
        bytes[i + 5] === 0x00) {
      bytes[i] = 0x0e;      // LdaUndefined
      bytes[i + 1] = 0x0e;  // LdaUndefined
      break;
    }
  }
  %InstallBytecode(f, bc);
}

for (let i = 0; i < 2000; i++) {
  assertEquals(0, f("skip"));
}
