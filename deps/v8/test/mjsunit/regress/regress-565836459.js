// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Target() {}
const proxy_proto = new Proxy({}, {});
Target.prototype = proxy_proto;

function NewTarget() {}
NewTarget.prototype = proxy_proto;

class Base {
  constructor(o) {
    return o;
  }
}

class DefineY extends Base {
  y = 1.1;
}

for (let i = 0; i < 10; i++) {
  new DefineY({});
}

// Create a Smi transition for .y on the derived map.
Reflect.construct(Target, [], NewTarget).y = 1;

// Trigger a Smi -> Double field generalization via DefineNamedOwnIC on the
// derived map.
const victim = new DefineY(Reflect.construct(Target, [], NewTarget));
assertEquals(1.1, victim.y);
