// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo(o, k) { return o[k]; }

const a = "a";
foo([1], 0);
foo({a:1}, a);

const p = new Proxy({}, {
  get(target, name) {
    return name;
  }
});

assertEquals(a + "b", foo(p, a + "b"));
