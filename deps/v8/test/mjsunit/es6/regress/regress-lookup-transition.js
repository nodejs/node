// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

var proxy = new Proxy({}, { getOwnPropertyDescriptor:function() {
  gc();
}});

function f() { this.x = 23; }
f.prototype = proxy;
new f();
new f();
