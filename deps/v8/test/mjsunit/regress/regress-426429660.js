// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

function __wrapTC(f = true) {
    return f();
}

function __f_1() {
  let [__v_10] = __wrapTC(() => [undefined]);
  return __v_10;
}

assertEquals(undefined, __f_1());
assertEquals(undefined, __f_1());
