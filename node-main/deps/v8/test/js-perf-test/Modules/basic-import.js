// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { value,
         set,
         a0,
         a1,
         a2,
         a3,
         a4,
         a5,
         a6,
         a7,
         a8,
         a9,
         a10,
         a11,
         a12,
         a13,
         a14,
         a15,
         a16,
         a17,
         a18,
         a19 } from "value.js";

export function bench() {
  for (let i = 0; i < iterations; ++i) {
    let accumulator = value;
    accumulator += a0;
    accumulator += a1;
    accumulator += a2;
    accumulator += a3;
    accumulator += a4;
    accumulator += a5;
    accumulator += a6;
    accumulator += a7;
    accumulator += a8;
    accumulator += a9;
    accumulator += a10;
    accumulator += a11;
    accumulator += a12;
    accumulator += a13;
    accumulator += a14;
    accumulator += a15;
    accumulator += a16;
    accumulator += a17;
    accumulator += a18;
    accumulator += a19;
    set(accumulator);
  }

  if (value !== 190 * iterations) throw new Error;

  set(0);
}
