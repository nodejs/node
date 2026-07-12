// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget=1024

function v1() {
  const v4 = Reflect;
  const v8 = [11.11];
  const v10 = {__proto__:1111, a:-1, c:RegExp, f:v8, d:1111, e:-1};
  const v12 = [11.11];
  function v13() {}
  const v16 = {a:v13, b:v13, c:v13, d:v13, e:v13, f:v13, g:v13, h:v13, i:v13, j:v13};
}

function foo() {
  let v22 = Number;
  v22 = v1;
  const v23 = false;
  if (v23) {
    v22 = Number;
  } else {
    function v24() {
      const v28 = ".Cactus"[0];
      for (let v32 = 0; v32 < 7; v32++) {}
    }
    new Promise(v24);
    try {
      for (const v37 of v36) {
        const v58 = [cactus,cactus,[] = cactus] = v117;
      }
    } catch(v119) {
    }
  }
  v22();
}

for (let i = 0; i < 10; i++) {
  foo();
}
