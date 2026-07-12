// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


class A {};
class B {};
class C {};
class D {};
class E {};
class V { constructor() { this.v = 1 } };
class W { constructor() { this.w = 1 } };
class X { constructor() { this.x = 1 } };
class Y { constructor() { this.y = 1 } };
class Z { constructor() { this.z = 1 } };

var ctrs = [
  function() {},
  A,B,C,D,E,V,W,X,Y,Z
];

for (var it = 0; it < 20; ++it) {
  for (var i in ctrs) {
    for (var j in ctrs) {
      assertTrue(%HaveSameMap(Reflect.construct(ctrs[i],[],ctrs[j]),
                              Reflect.construct(ctrs[i],[],ctrs[j])));
    }
  }
}
