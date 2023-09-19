// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let prop = "someName";
function foo(a, b, v) { return a[b] = 0 }
try {
  foo("", prop);
} catch(e) {}
var target = {};
var traps = { set() {return 42} };
var proxy = new Proxy(target, traps);
Object.defineProperty(target, prop, { value: 0 });
try {
  foo(proxy, prop);
} catch (e) { }
foo(proxy, prop, 0);
