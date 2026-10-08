// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {
  "use strict";

  const N = 1536;

  function tryExposeUninitializedHole() {
    globalThis.dynamic = 1;
    const children = [];
    for (let i = 0; i < N; ++i) {
      children.push(`{a:.1,p${i}:0,c:0}`);
    }
    eval(`
      (() => {
        const baseline = {a:0,b:0,c:0};
        globalThis.corrupted = {
          a:globalThis.dynamic,
          b:0,
          c:[${children.join(",")}]
        };
        globalThis.baseline = baseline;
      })();
    `);
    return globalThis.corrupted.c;
  }

  const failedToExposeValue = tryExposeUninitializedHole();

  assertEquals(Object.keys(globalThis.corrupted), ["a","b","c"]);
  assertTrue(Array.isArray(failedToExposeValue));
  for (let i = 0; i < N; ++i) {
    assertEquals(failedToExposeValue[i], {"a":0.1,[`p${i}`]:0,"c":0});
  }
})();
