// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

async function __f_3() {
  return await __f_4();
}
async function __f_4() {
  await x.then();
  throw new Error();
};
%PrepareFunctionForOptimization(__f_4);
async function __f_5(f) {
  try {
    await f();
  } catch (e) {
  }
}
(async () => {
  ;
  %OptimizeFunctionOnNextCall(__f_4);
  await __f_5(__f_3);
})();
