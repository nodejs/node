// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --memory-corruption-via-watchpoints --sandbox-testing
// Flags: --allow-natives-syntax

let s = "========================================";
Sandbox.markForCorruptionOnAccess(s, 12);

try {
  s.replace(/=/g, "X");
} catch (e) {
  // Ignore any exceptions from corrupted string access.
}
