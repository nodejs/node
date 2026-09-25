// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --stress-compaction --detailed-error-stack-trace --gc-interval=6

function add(a, b) {
  throw new Error();
}
for (let i = 0; i < 100; ++i) {
  try {
    add(1, 2);
  } catch (e) {
  }
}
