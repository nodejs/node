// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --cache=after-execute

import { helper } from './d8-code-cache-module-dep.mjs';

function compute(a, b) {
  return helper(a) + b;
}

if (compute(10, 5) !== 25) {
  throw new Error("Unexpected compute result");
}

export const val = compute(2, 3);

import(import.meta.url).then(ns => {
  if (ns.val !== 7) {
    throw new Error("Unexpected exported value");
  }
});
