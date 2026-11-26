// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On-stack replacement: a single call whose loop runs long enough to OSR.
// The resulting compilation carries an OSR bytecode offset and must be
// distinguishable from a regular tier-up compilation (PLAN 5.1).
function hotLoop(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    s = (s + i * 3) | 0;
  }
  return s;
}
print(hotLoop(3000000));
