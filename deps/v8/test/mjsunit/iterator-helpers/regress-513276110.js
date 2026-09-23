// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {
  let helper;

  const iter = {
    next() {
      return { value: 1, done: false };
    },
    return() {
      assertThrows(() => helper.next(), TypeError);
      return { done: true };
    }
  };

  helper = Iterator.from(iter).take(1);
  assertEquals(1, helper.next().value);
  assertEquals(true, helper.next().done);
})();
