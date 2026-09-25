// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function NegativeIndexAndDictionaryElements() {
  function f(o, idx) {
    return o[idx];
  }

  f({}, 0);
  f({}, 0);  // Make the IC megamorphic/generic.

  var o = {};
  o[1000000] = "dictionary";
  var c = -21;
  o[c] = "foo";
  assertEquals("foo", f(o, c));
})();
