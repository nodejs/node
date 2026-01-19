// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Adding a property via Object.defineProperty should not be taken as hint that
// we construct a dictionary, quite the opposite.
var obj = {};

for (var i = 0; i < 100; i++) {
  Object.defineProperty(obj, "x" + i, { value: 31415 });
  Object.defineProperty(obj, "y" + i, {
    get: function() { return 42; },
    set: function(value) { }
  });
  assertTrue(%HasFastProperties(obj));
}
