// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --superspreading

function runNearStackLimit(f) {
  let count = -1;
  function t() {
    try {
      t();
      if (count > 0) {
        count--;
      }
      if (count == 1) {
        try {
          f();
        } catch (e) {
          if (e instanceof RangeError) {
            // TODO(olivf): Remove once all platforms support it
            console.log("Warning: Vararg optimization seems missing");
          } else {
            throw e;
          }
        }
      }
      return;
    } catch (e) {
      if (count == -1) {
        // Back off
        count = 1200;
      } else {
        throw e;
      }
    }
  }
  t();
}

const kSize = 32768;

runNearStackLimit(function () {
  let spread_array = new Array(kSize).fill(1);
  spread_array[kSize-1] = 33;
  Object.defineProperty(Array.prototype, "50", {
    set(value) {
      spread_array[kSize-1] = 42;
    },
    configurable: true,
    enumerable: true
  });
  let receiver = [];
  Array.prototype.push.apply(receiver, spread_array);
  assertEquals(receiver[kSize-1], 33);
});
