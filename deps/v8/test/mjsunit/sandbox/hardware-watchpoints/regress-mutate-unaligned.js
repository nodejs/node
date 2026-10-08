// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --memory-corruption-via-watchpoints --sandbox-testing
// Flags: --allow-natives-syntax

// This test triggers watchpoint mutations on unaligned and boundary-crossing
// memory accesses during repeating string operations (like rep movs).
// Previously, unaligned accesses crossing an 8-byte boundary would cause a stack
// buffer overflow in the d8 process.

let s = "========================================";
// Set watchpoints at various tagged-aligned offsets. Offsets like 12, 20, 28
// are 4-byte aligned but not 8-byte aligned.
for (let offset = 8; offset < 32; offset += 4) {
  Sandbox.markForCorruptionOnAccess(s, offset);
}

try {
  s.replace(/=/g, "X");
} catch (e) {
  // Ignore any exceptions from corrupted string access.
}
