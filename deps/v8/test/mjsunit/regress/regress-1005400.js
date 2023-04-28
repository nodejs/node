// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo(a, key) {
  a[key];
}

let obj = {};
let count = 0;

var key_obj = {
  toString: function() {
    count++;
    // Force string to be internalized during keyed lookup.
    return 'foo' + count;
  }
};

foo(obj, key_obj);

// We should only call toString once.
assertEquals(count, 1);
