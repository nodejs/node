// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: regress/duplicates/input.js
let __v_1 = 0;
let __v_2 = 0;
console.log(42);
/* CrossOverMutator: Crossover from foo */
async (...__v_1) => {
  const __v_2 = 0;
};
/* CrossOverMutator: Crossover from foo */
async (...__v_1) => {
  const __v_2 = 0;
};
console.log(42);
/* CrossOverMutator: Crossover from foo */
async (...__v_2) => {
  const __v_1 = 0;
};
console.log(42);
