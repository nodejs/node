// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f(a) {
  a.foo = {};
  a[0] = 1;
  a.__defineGetter__('foo', function() {});
  a[0] = {};
  a.bar = 0;
}
f(new Array());
