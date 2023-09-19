// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as m from "value.js";

export function bench() {
  for (let i = 0; i < iterations; ++i) {
    let accumulator = m.value;
    accumulator += m.a0;
    accumulator += m.a1;
    accumulator += m.a2;
    accumulator += m.a3;
    accumulator += m.a4;
    accumulator += m.a5;
    accumulator += m.a6;
    accumulator += m.a7;
    accumulator += m.a8;
    accumulator += m.a9;
    accumulator += m.a10;
    accumulator += m.a11;
    accumulator += m.a12;
    accumulator += m.a13;
    accumulator += m.a14;
    accumulator += m.a15;
    accumulator += m.a16;
    accumulator += m.a17;
    accumulator += m.a18;
    accumulator += m.a19;
    m.set(accumulator);
  }

  if (m.value !== 190 * iterations) throw new Error;

  m.set(0);
}
