// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --no-lazy-feedback-allocation
// Flags: --invocation-count-for-turbofan=1

function f() { this.x = this.x.x; }
gc();
f.prototype.x = { x:1 }
new f();
new f();

function g() {
  function h() {};
  h.prototype = { set x(value) { } };
  new f();
}
g();
