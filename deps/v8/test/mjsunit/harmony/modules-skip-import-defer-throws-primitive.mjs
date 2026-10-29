// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (!globalThis.eval_list) {
  globalThis.eval_list = [];
}
globalThis.eval_list.push('defer-throws-primitive');

// Throwing a primitive rather than an Error matters: d8's promise-rejection
// hook only fabricates an Error (and so only walks the stack) when the
// rejection value is not already a native Error.
throw 123;

export const foo = 42;
