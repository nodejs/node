// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (!globalThis.eval_list) {
  globalThis.eval_list = [];
}
globalThis.eval_list.push('defer-1');

export const foo = 42;
