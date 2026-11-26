// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Float64 arithmetic consumed only through a truncating operator (`| 0`), so the
// Maglev truncation pass has something to say. Paired with
// --trace-maglev-truncation this is the fixture that pins down the
// annotation-attachment rules (PLAN 6.1): free-form trace lines printed between
// and inside graph dumps.
function trunc(a, b) {
  const p = a * b;
  const q = p + a;
  // Only the truncated value escapes.
  return (q | 0) + ((p * 2) | 0);
}

let acc = 0;
for (let i = 0; i < 100000; i++) {
  acc = (acc + trunc(i * 1.5, i * 0.25)) | 0;
}
print(acc);
