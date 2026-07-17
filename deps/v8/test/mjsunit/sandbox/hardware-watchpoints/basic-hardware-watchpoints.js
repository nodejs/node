// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --memory-corruption-via-watchpoints

let str = "abcde";
Sandbox.markForCorruptionOnAccess(str, 'length');

let values = new Set();
for (let i = 0; i < 10; i++) {
  values.add(str.length);
}

// If the fuzzer is working, we should have more than one unique value.
let length_values = Array.from(values).sort((a, b) => a - b);
print('Seen values for `str.length`: ' + length_values.join(', '));
assertTrue(values.size > 1, 'The string length should have been mutated');
