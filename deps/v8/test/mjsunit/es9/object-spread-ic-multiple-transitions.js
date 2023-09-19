// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testMegamorphicWithNonSimpleTransitionHandler() {
  function spread(o) { return { ...o }; }

  // Set up transition tree
  let obj = { ...{}, a: 0, b: 1, boom: 2};

  // make CloneObjectIC MEGAMORPHIC
  spread(new Proxy({}, {}));

  // Ensure we don't crash, and create the correct object
  assertEquals({ a: 0, b: 1, c: 2 }, spread({ a: 0, b: 1, c: 2 }));
})();
