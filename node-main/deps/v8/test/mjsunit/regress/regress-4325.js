// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function Inner() {
  this.p1 = 0;
  this.p2 = 3;
}

function Outer() {
  this.p3 = 0;
}

var i1 = new Inner();
var i2 = new Inner();
var o1 = new Outer();
o1.inner = i1;
// o1.map now thinks "inner" has type Inner.map1.
// Deprecate Inner.map1:
i1.p1 = 0.5;
// Let Inner.map1 die by migrating i2 to Inner.map2:
print(i2.p1);
gc();
// o1.map's descriptor for "inner" is now a cleared weak reference;
// o1.inner's actual map is Inner.map2.
// Prepare Inner.map3, deprecating Inner.map2.
i2.p2 = 0.5;
// Deprecate o1's map.
var o2 = new Outer();
o2.p3 = 0.5;
o2.inner = i2;
// o2.map (Outer.map2) now says that o2.inner's type is Inner.map3.
// Migrate o1 to Outer.map2.
print(o1.p3);
// o1.map now thinks that o1.inner has map Inner.map3 just like o2.inner,
// but in fact o1.inner.map is still Inner.map2!

function loader(o) {
  return o.inner.p2;
};
%PrepareFunctionForOptimization(loader);
loader(o2);
loader(o2);
%OptimizeFunctionOnNextCall(loader);
assertEquals(0.5, loader(o2));
assertEquals(3, loader(o1));
gc();  // Crashes with --verify-heap.
