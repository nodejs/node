// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let obj2;
let obj1 = d8.test.createAccessCheckedInterceptorObject(true, () => {
  if (obj2) obj2.x = 1.5;
  return "ok";
});

const Ctor = obj1.constructor;
obj2 = new Ctor();
d8.test.setAccessPolicy(obj2, true);
obj1.x = 1;
obj2.x = 1;

d8.test.setAccessPolicy(obj1, false);

function Base(o) {
  return o;
}

class C extends Base {
  a = 1;
}

for (let i = 0; i < 20; i++) {
  new C({});
}

assertThrows(() => new C(obj1), TypeError);
assertEquals(1, obj2.x);

obj2.a = 1;
let key = "a";
class C2 extends Base {
  [key] = 42;
}
assertThrows(() => new C2(obj1), TypeError);
