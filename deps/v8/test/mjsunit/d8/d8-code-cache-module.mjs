// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --cache=code

import { helper } from './d8-code-cache-module-dep.mjs';

if (helper(21) !== 42) {
  throw new Error("Unexpected helper result");
}

export const value = 123;

import(import.meta.url).then(ns => {
  if (ns.value !== 123) {
    throw new Error("Unexpected exported value");
  }
});
