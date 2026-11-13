// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function under(a, b, c, d) {
  throw new Error('v8dbg args test: ' + a);
}

function over(a) {
  return under('only-one');
}

over(1, 2, 3);
