// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let call_count = 0;
let reentrant_gc =
    function(iter) {
  [...iter];
  gc();
  call_count++;
}

let fg = new FinalizationRegistry(reentrant_gc);

(function() {
fg.register({}, 42);
})();

gc();

setTimeout(function() {
  assertEquals(1, call_count);
}, 0);
