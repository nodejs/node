// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that Array.prototype.sort is stable.

function TestStability() {
  const size = 100;
  let arr = new Array(size);

  // Fill array with objects.
  // We use a small range of keys to ensure many equal keys.
  for (let i = 0; i < size; i++) {
    arr[i] = {
      key: i % 10,
      orig_idx: i
    };
  }

  // Sort by key.
  arr.sort((a, b) => a.key - b.key);

  // Verify stability.
  for (let i = 1; i < size; i++) {
    if (arr[i].key === arr[i-1].key) {
      assertTrue(arr[i].orig_idx > arr[i-1].orig_idx,
                 `Stability violated at index ${i}`);
    }
  }
}

TestStability();

// Test with strings as well.
function TestStringStability() {
  const size = 100;
  let arr = new Array(size);

  for (let i = 0; i < size; i++) {
    arr[i] = {
      key: String.fromCharCode(65 + (i % 5)), // A, B, C, D, E
      orig_idx: i
    };
  }

  arr.sort((a, b) => a.key.localeCompare(b.key));

  for (let i = 1; i < size; i++) {
    if (arr[i].key === arr[i-1].key) {
      assertTrue(arr[i].orig_idx > arr[i-1].orig_idx,
                 `String stability violated at index ${i}`);
    }
  }
}

TestStringStability();
