// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
  v_0 = {};
  Object.defineProperty(v_0, '0', {});
  v_0.p_0 = 0;
  assertArrayEquals(['0', 'p_0'],
                    Object.getOwnPropertyNames(v_0));
  assertArrayEquals(['0', 'p_0'],
                    Object.getOwnPropertyNames(v_0));
}
f();
