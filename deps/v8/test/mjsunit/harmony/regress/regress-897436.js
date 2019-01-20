// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-slow-asserts

async function* gen() {
  const alwaysPending = new Promise(() => {});
  alwaysPending.then = "non-callable then";
  yield alwaysPending;
}
gen().next();
