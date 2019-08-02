// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var count = 0;
function load(a) {
  var prop = {
    toString: function() {
      count++;
      return 'z';
    }
  };

  a[prop] ^= 1;
}

function f(null_or_undefined) {
  // Turn the LoadIC megamorphic
  load({a0:1, z:2});
  load({a1:1, z:2});
  load({a2:1, z:2});
  load({a3:1, z:2});
  load({a4:1, z:2});
  // Now try null to check if generic IC handles this correctly.
  // It shouldn't call prop.toString.
  load(null_or_undefined);
}

try {
  f(null);
} catch(error) {
  assertInstanceof(error, TypeError);
  assertSame(10, count);
}

try {
  count = 0;
  f(undefined);
} catch(error) {
  assertInstanceof(error, TypeError);
  assertSame(10, count);
}
