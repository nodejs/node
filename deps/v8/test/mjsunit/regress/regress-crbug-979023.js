// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo(arg) {
  var ret = { x: arg };
  Object.defineProperty(ret, "y", {
    get: function () { },
    configurable: true
  });
  return ret;
}
let v0 = foo(10);
let v1 = foo(10.5);
Object.defineProperty(v0, "y", {
  get: function () { },
  configurable: true
});
