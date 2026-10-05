// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

function trigger() {
  let box = { x: 1.1 };
  let o = { field: 1.1 };

  for (let i = 0; i < 2; i++) {
    o.field = box.x;

    for (let j = 0; j < 2; j++) {
      o.field = box.x;
    }

    box.x = {};
  }

  return o.field;
}

%PrepareFunctionForOptimization(trigger);
trigger();
trigger();

%OptimizeFunctionOnNextCall(trigger);
trigger();
