// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

function CleanUp() {}
const registry = new FinalizationRegistry(CleanUp);
let target = {};
let unreg_token = {};
registry.register(target, undefined, unreg_token);
for(let i=0; i<2; i++)
  Object.assign(unreg_token, {});
target = undefined;
gc();
