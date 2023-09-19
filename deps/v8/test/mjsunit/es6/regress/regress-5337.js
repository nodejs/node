// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testNestedSpreadsInPatterns() {
  (function () {
    var [...[...x]] = [42, 17];
    assertArrayEquals([42, 17], x);
  })();
  (function () {
    let [...[...x]] = [42, 17];
    assertArrayEquals([42, 17], x);
  })();
  (function () {
    const [...[...x]] = [42, 17];
    assertArrayEquals([42, 17], x);
  })();
  (function () {
    var x; [...[...x]] = [42, 17];
    assertArrayEquals([42, 17], x);
  })();

  function f1([...[...x]] = [42, 17]) { return x; }
  assertArrayEquals([42, 17], f1());
  assertArrayEquals([1, 2, 3], f1([1, 2, 3]));

  var f2 = function ([...[...x]] = [42, 17]) { return x; }
  assertArrayEquals([42, 17], f2());
  assertArrayEquals([1, 2, 3], f2([1, 2, 3]));

  // The following two were failing in debug mode, until v8:5337 was fixed.
  var f3 = ([...[...x]] = [42, 17]) => { return x; };
  assertArrayEquals([42, 17], f3());
  assertArrayEquals([1, 2, 3], f3([1, 2, 3]));

  var f4 = ([...[...x]] = [42, 17]) => x;
  assertArrayEquals([42, 17], f4());
  assertArrayEquals([1, 2, 3], f4([1, 2, 3]));
})();
