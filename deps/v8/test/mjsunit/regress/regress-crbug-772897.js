// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function store(obj, name) {
  return obj[name] = 0;
}

function f(obj) {
  var key = {
    toString() { throw new Error("boom"); }
  };
  store(obj, key);
}

(function() {
  var proxy = new Proxy({}, {});
  store(proxy, 0)
  assertThrows(() => f(proxy), Error);
})();
