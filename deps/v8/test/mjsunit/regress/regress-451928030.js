// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --trace-migration

function test(v) {
  v++;
  this[Symbol.dispose] = v;
}
const a = new test(42);
new test();
let b = 43;
({'b': b, ...Int32Array} = a);
