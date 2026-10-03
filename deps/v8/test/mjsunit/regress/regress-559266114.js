// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function NewTarget() {}
NewTarget.prototype = new Proxy({}, {});

function makeFn(body, x, y) {
  const fn = Reflect.construct(Function, [body], NewTarget);
  fn.x = x;
  if (y) fn.y = y;
  return fn;
}

class Base {
  constructor(o) { return o; }
}

class DefineY extends Base {
  y = 1.1;
}

for (let i = 0; i < 15; i++) new DefineY({});

let strictWithY = makeFn('"use strict"', 42, 42);
let strictWithX = makeFn('"use strict"', 42);
let sloppyWithY = makeFn('', 2.3, 2.3);
let cachedTransition = new DefineY(makeFn('"use strict"', 42));
let victim = new DefineY(makeFn('"use strict"', 42));
victim.x = 13.37;
assertEquals(13.37, victim.x);
