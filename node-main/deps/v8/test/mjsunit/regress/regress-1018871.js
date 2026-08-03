// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var v_this = this;
function f() {
  var obj = {y: 0};
  var proxy = new Proxy(obj, {
    has() { y; },
  });
  Object.setPrototypeOf(v_this, proxy);
  x;
}
assertThrows(f, RangeError);
