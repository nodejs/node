// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  function test(obj) {
    let it = obj.values();
    var closed = false;
    var next;
    it.return = function () {
      closed = true;
      next = it.next();
      return {};
    };
    assertThrows(() => Math.sumPrecise(it), TypeError);
    assertTrue(closed);
    assertEquals({value: "pos", done: false}, next);
  }
  test([1, "bad", "pos", 2]);
  test([1, , "pos", 2]);
  test([, "pos", 2]);
  test([, "pos", ]);
  test(new Set([1, "bad", "pos", 2]));
  let set = new Set([1, "bad", "pos"]);
  set.delete(1);
  test(set);
})();

(function TestArrayFastPathPrototypeReturn() {
  const ArrayIteratorPrototype = Object.getPrototypeOf([].values());
  const cases = [
    [["bad"], {value: undefined, done: true}],
    [[1, "bad", 2], {value: 2, done: false}],
    [[1, , 2], {value: 2, done: false}],
    [[1.1, , 2.2], {value: 2.2, done: false}],
    [[, "bad"], {value: "bad", done: false}],
  ];
  for (const [arr, expectedNext] of cases) {
    let closed = false;
    let nextResult;
    ArrayIteratorPrototype.return = function() {
      closed = true;
      nextResult = this.next();
      return {};
    };
    try {
      assertThrows(() => Math.sumPrecise(arr), TypeError);
    } finally {
      delete ArrayIteratorPrototype.return;
    }
    assertTrue(closed);
    assertEquals(expectedNext, nextResult);
  }
})();
