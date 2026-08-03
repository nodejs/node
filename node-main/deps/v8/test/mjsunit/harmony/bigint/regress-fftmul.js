// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function regress_1227752(power) {
  let a = 2n ** power;
  let a_squared = a * a;
  let expected = 2n ** (2n * power);
  assertEquals(expected, a_squared);
}
regress_1227752(48016n);  // This triggered the bug on 32-bit platforms.
regress_1227752(95960n);  // This triggered the bug on 64-bit platforms.
