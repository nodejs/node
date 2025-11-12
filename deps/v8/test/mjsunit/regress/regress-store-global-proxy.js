// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

delete Object.prototype.__proto__;

function f() {
  this.toString = 1;
}

f.apply({});
f();
