// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Spread calls get rewritten to CallRuntime, which should be aware of optional
// chaining.

for (let nullish of [undefined, null]) {
  const fn = nullish;
  const n = nullish;
  const o = {};

  assertEquals(fn?.(...[], 1), undefined);
  assertEquals(fn?.(...[], ...[]), undefined);
  assertEquals(o.method?.(...[], 1), undefined);
  assertEquals(n?.method(...[], 1), undefined);
}
