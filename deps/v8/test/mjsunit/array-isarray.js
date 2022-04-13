// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


assertTrue(Array.isArray([]));
assertFalse(Array.isArray({}));
assertFalse(Array.isArray(null));
assertFalse(Array.isArray(0));
assertFalse(Array.isArray(0.1));
assertFalse(Array.isArray(""));
assertFalse(Array.isArray(undefined));

assertTrue(Array.isArray(new Proxy([], {})));
assertFalse(Array.isArray(new Proxy({}, {})));

assertTrue(Array.isArray(new Proxy(new Proxy([], {}), {})));
assertFalse(Array.isArray(new Proxy(new Proxy({}, {}), {})));

(function TestIsArrayStackOverflow() {
  var proxy = new Proxy([], {});
  for(var i=0; i<200*1024; i++) {
      proxy = new Proxy(proxy, {});
  }
  assertThrows(()=>Array.isArray(proxy), RangeError);
})();
