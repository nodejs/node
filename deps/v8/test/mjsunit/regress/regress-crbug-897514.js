// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Create transtion => 'get a'.
let o = {};
Object.defineProperty(o, 'a', {
    enumerable: true,
    configurable: true,
    get: function() { return 7 }
});

function spread(o) {
  let result = { ...o };
  %HeapObjectVerify(result);
  return result;
}

for (let i = 0; i<3; i++) {
  spread([]);
  // Use different transition => 'a'.
  spread({ a:0 });
  spread("abc");
}
