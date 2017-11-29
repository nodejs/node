// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export let value = 0;
export function set(x) { value = x };

export let a0 = 0;
export let a1 = 1;
export let a2 = 2;
export let a3 = 3;
export let a4 = 4;
export let a5 = 5;
export let a6 = 6;
export let a7 = 7;
export let a8 = 8;
export let a9 = 9;
export let a10 = 10;
export let a11 = 11;
export let a12 = 12;
export let a13 = 13;
export let a14 = 14;
export let a15 = 15;
export let a16 = 16;
export let a17 = 17;
export let a18 = 18;
export let a19 = 19;

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
