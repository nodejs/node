// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --cache-property-key-string-adds --no-lazy-feedback-allocation

var counter = 0;
var val = {
  valueOf: function () {
    counter++;
    return "foo";
  }
};

var o = {a_foo:42};
assertEquals(42, o["a_" + val]);
assertEquals(1, counter);
