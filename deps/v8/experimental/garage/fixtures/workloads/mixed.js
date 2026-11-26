// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The realistic case: several functions tiering up at different times, one of
// them caught in an optimize -> deopt -> reoptimize cycle. This is the fixture
// for chronological ordering, grouped-by-function mode, and multiple
// compilation instances per function.
function sum(arr) {
  let s = 0;
  for (let i = 0; i < arr.length; i++) s += arr[i];
  return s;
}

function scale(arr, k) {
  const out = new Array(arr.length);
  for (let i = 0; i < arr.length; i++) out[i] = arr[i] * k;
  return out;
}

// Flip-flops: alternately fed Smis and doubles.
function flipflop(x) {
  return x * 2 + 1;
}

const data = [];
for (let i = 0; i < 256; i++) data.push(i);

let acc = 0;
for (let round = 0; round < 400; round++) {
  acc += sum(data);
  acc += sum(scale(data, 3));
  for (let i = 0; i < 200; i++) {
    acc += flipflop(round % 7 === 6 ? i + 0.5 : i);
  }
}
print(acc);
