// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

function f1() {
  return f1;
}
class C2 extends f1 {
  constructor() {
    try {
      const v6 = Symbol.dispose;
      const v10 = {
        [v6]() {
          gc();
        },
      };
      using v11 = v10;
      super();
      throw 889951875n;
    } catch (e12) {
    }
  }
}
for (let v13 = 0; v13 < 25; v13++) {
  new C2();
}

const holder = {obj: {}};
function f2() {
  return holder.obj;
}
class C3 extends f2 {
  constructor() {
    try {
      using v = {
        [Symbol.dispose]() {
          gc();
        },
      };
      super();
      throw 1;
    } catch (e) {
    }
  }
}
for (let i = 0; i < 25; i++) {
  new C3();
}
