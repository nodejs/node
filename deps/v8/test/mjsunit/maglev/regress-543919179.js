// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev-non-eager-inlining --maglev-disable-builtin-reducers

let dummy;

function inner() {
  try {
    for (var j = 0; j < 33; j++) {
    }
  } catch (e) {}
}

function foo(n) {
  var limit = Math.pow(2, n);
  for (var i = 0; i < limit; i = i * 1.01 + 1) {
    inner(), Math.clz32({});
  }
}
foo(40);
