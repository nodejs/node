// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: regress/await/input.js
foo();
bar();
function __f_1() {
  foo();
  bar();
}
let __f_2 = function () {
  foo();
  bar();
};
async function __f_3() {
  foo();
  /* CrossOverMutator: Crossover from foo */
  await 3;
  /* CrossOverMutator: Crossover from foo */
  await 3;
  bar();
}
bar(async () => {
  foo();
  /* CrossOverMutator: Crossover from foo */
  await 3;
  bar();
  /* CrossOverMutator: Crossover from foo */
  await 3;
});
