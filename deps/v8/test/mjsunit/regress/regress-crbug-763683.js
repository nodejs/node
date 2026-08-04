// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(function test() {
  var proxy = new Proxy({}, {});
  var key_or_proxy = 0;

  function failing_get() {
    return proxy[key_or_proxy];
  }

  failing_get();

  key_or_proxy = new Proxy({
    toString() {
      throw new TypeError('error');
    }
  }, {});

  failing_get();
}, TypeError);
