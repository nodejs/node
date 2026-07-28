// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

import * as mod from "regress-module-export-proto-helper.mjs";

let protoA = {};
Object.setPrototypeOf(protoA, mod);

let obj = Object.create(protoA);

function readTF(o) {
  return o.value;
}

%PrepareFunctionForOptimization(readTF);
assertEquals(42, readTF(obj));
assertEquals(42, readTF(obj));

%OptimizeFunctionOnNextCall(readTF);
assertEquals(42, readTF(obj));

// Mutate prototype chain
Object.setPrototypeOf(protoA, { value: 999 });

// Should deoptimize and return new value
assertEquals(999, readTF(obj));
