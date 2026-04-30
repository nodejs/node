// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --memory-corruption-via-watchpoints --sandbox-testing

let n = 1.1;
Sandbox.markForCorruptionOnAccess(n, 4);
Sandbox.markForCorruptionOnAccess(n, 8);
print(n + 1);

let values = new Set();
for (let i = 0; i < 10; i++) {
  values.add(n + 1);
}

// If the fuzzer is working, we should have more than one unique value.
let unique_values = Array.from(values).sort((a, b) => a - b);
print('Seen values for `n + 1`: ' + unique_values.join(', '));
assertTrue(values.size > 1, 'The HeapNumber value should have been mutated');
