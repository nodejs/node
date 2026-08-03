// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function array_iterator() {
  let array_iterator_prototype = [].values().__proto__;
  let iter;
  array_iterator_prototype.return = function(value) {
    iter = this;
    return {value: value, done: true};
  };

  let array = [["good1"], ["good2"], "bad", "next", 5, 6, 7, 8];

  // Aborted iteration in a builtin.
  try {
    new WeakSet(array);
  } catch (e) {}
  // iter points at "bad" item, so next() must return "next" value.
  assertEquals(iter.next().value, "next");
})();
