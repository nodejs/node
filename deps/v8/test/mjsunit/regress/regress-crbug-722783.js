// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function set_x(v) {
  x = v;
}

var o = {};
set_x(o);
set_x(o);
assertEquals(o, x);
Object.defineProperty(this, "x", { value:5, writable:false, configurable:true });
assertEquals(5, x);
set_x(o);
set_x(o);
assertEquals(5, x);
Object.defineProperty(this, "x", { value:42, writable:true, configurable:true });
assertEquals(42, x);
set_x(o);
assertEquals(o, x);
