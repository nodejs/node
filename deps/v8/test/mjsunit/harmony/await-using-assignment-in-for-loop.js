// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await assertThrowsAsync(
      (async () => {
        for (await using x = {value: 42, [Symbol.asyncDispose]() {}};
             x.value < 44; x.value++) {
          // This assignment should throw a TypeError because `x` is immutable.
          x = { [Symbol.asyncDdispose]() {} }
        }
      })(),
      TypeError);
})();
