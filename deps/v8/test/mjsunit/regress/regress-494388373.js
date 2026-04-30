// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --single-threaded

for (let i = 0; i < 50000; i++) {
  const result = [0].map(() => undefined)[0];
  var other = result;
  const test = (result === SharedArrayBuffer);
  assertFalse(test);
}
